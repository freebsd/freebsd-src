/* coff object file format
   Copyright (C) 1989, 90, 91, 92, 93, 94, 95, 96, 97, 98, 99, 2000
   Free Software Foundation, Inc.

   This file is part of GAS.

   GAS is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   GAS is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with GAS; see the file COPYING.  If not, write to the Free
   Software Foundation, 59 Temple Place - Suite 330, Boston, MA
   02111-1307, USA.  */

#define OBJ_HEADER "obj-coff.h"

#include "as.h"
#include "obstack.h"
#include "subsegs.h"

/* I think this is probably always correct.  */
#ifndef KEEP_RELOC_INFO
#define KEEP_RELOC_INFO
#endif

/* The BFD_ASSEMBLER version of obj_coff_section will use this macro to set
   a new section's attributes when a directive has no valid flags or the
   "w" flag is used. This default should be appropriate for most.  */
#ifndef TC_COFF_SECTION_DEFAULT_ATTRIBUTES
#define TC_COFF_SECTION_DEFAULT_ATTRIBUTES (SEC_LOAD | SEC_DATA)
#endif

static void obj_coff_bss PARAMS ((int));
const char *s_get_name PARAMS ((symbolS * s));
static void obj_coff_ln PARAMS ((int));
static void obj_coff_def PARAMS ((int));
static void obj_coff_endef PARAMS ((int));
static void obj_coff_dim PARAMS ((int));
static void obj_coff_line PARAMS ((int));
static void obj_coff_size PARAMS ((int));
static void obj_coff_scl PARAMS ((int));
static void obj_coff_tag PARAMS ((int));
static void obj_coff_val PARAMS ((int));
static void obj_coff_type PARAMS ((int));
static void obj_coff_ident PARAMS ((int));
#ifdef BFD_ASSEMBLER
static void obj_coff_loc PARAMS((int));
#endif

/* This is used to hold the symbol built by a sequence of pseudo-ops
   from .def and .endef.  */
static symbolS *def_symbol_in_progress;

/* stack stuff */
typedef struct
  {
    unsigned long chunk_size;
    unsigned long element_size;
    unsigned long size;
    char *data;
    unsigned long pointer;
  }
stack;

static stack *
stack_init (chunk_size, element_size)
     unsigned long chunk_size;
     unsigned long element_size;
{
  stack *st;

  st = (stack *) malloc (sizeof (stack));
  if (!st)
    return 0;
  st->data = malloc (chunk_size);
  if (!st->data)
    {
      free (st);
      return 0;
    }
  st->pointer = 0;
  st->size = chunk_size;
  st->chunk_size = chunk_size;
  st->element_size = element_size;
  return st;
}

#if 0
/* Not currently used.  */
static void
stack_delete (st)
     stack *st;
{
  free (st->data);
  free (st);
}
#endif

static char *
stack_push (st, element)
     stack *st;
     char *element;
{
  if (st->pointer + st->element_size >= st->size)
    {
      st->size += st->chunk_size;
      if ((st->data = xrealloc (st->data, st->size)) == (char *) 0)
	return (char *) 0;
    }
  memcpy (st->data + st->pointer, element, st->element_size);
  st->pointer += st->element_size;
  return st->data + st->pointer;
}

static char *
stack_pop (st)
     stack *st;
{
  if (st->pointer < st->element_size)
    {
      st->pointer = 0;
      return (char *) 0;
    }
  st->pointer -= st->element_size;
  return st->data + st->pointer;
}

/*
 * Maintain a list of the tagnames of the structres.
 */

static struct hash_control *tag_hash;

static void
tag_init ()
{
  tag_hash = hash_new ();
}

static void
tag_insert (name, symbolP)
     const char *name;
     symbolS *symbolP;
{
  const char *error_string;

  if ((error_string = hash_jam (tag_hash, name, (char *) symbolP)))
    {
      as_fatal (_("Inserting \"%s\" into structure table failed: %s"),
		name, error_string);
    }
}

static symbolS *
tag_find (name)
     char *name;
{
#ifdef STRIP_UNDERSCORE
  if (*name == '_')
    name++;
#endif /* STRIP_UNDERSCORE */
  return (symbolS *) hash_find (tag_hash, name);
}

static symbolS *
tag_find_or_make (name)
     char *name;
{
  symbolS *symbolP;

  if ((symbolP = tag_find (name)) == NULL)
    {
      symbolP = symbol_new (name, undefined_section,
			    0, &zero_address_frag);

      tag_insert (S_GET_NAME (symbolP), symbolP);
#ifdef BFD_ASSEMBLER
      symbol_table_insert (symbolP);
#endif
    }				/* not found */

  return symbolP;
}

/* We accept the .bss directive to set the section for backward
   compatibility with earlier versions of gas.  */

static void
obj_coff_bss (ignore)
     int ignore ATTRIBUTE_UNUSED;
{
  if (*input_line_pointer == '\n')
    subseg_new (".bss", get_absolute_expression ());
  else
    s_lcomm (0);
}

/* Handle .weak.  This is a GNU extension.  */

static void
obj_coff_weak (ignore)
     int ignore ATTRIBUTE_UNUSED;
{
  char *name;
  int c;
  symbolS *symbolP;

  do
    {
      name = input_line_pointer;
      c = get_symbol_end ();
      symbolP = symbol_find_or_make (name);
      *input_line_pointer = c;
      SKIP_WHITESPACE ();

#if defined BFD_ASSEMBLER || defined S_SET_WEAK
      S_SET_WEAK (symbolP);
#endif

#ifdef TE_PE
      S_SET_STORAGE_CLASS (symbolP, C_NT_WEAK);
#else
      S_SET_STORAGE_CLASS (symbolP, C_WEAKEXT);
#endif

      if (c == ',')
	{
	  input_line_pointer++;
	  SKIP_WHITESPACE ();
	  if (*input_line_pointer == '\n')
	    c = '\n';
	}
    }
  while (c == ',');

  demand_empty_rest_of_line ();
}

#ifdef BFD_ASSEMBLER

static void SA_SET_SYM_TAGNDX PARAMS ((symbolS *, symbolS *));

#define GET_FILENAME_STRING(X) \
((char*) (&((X)->sy_symbol.ost_auxent->x_file.x_n.x_offset))[1])

/* @@ Ick.  */
static segT
fetch_coff_debug_section ()
{
  static segT debug_section;
  if (!debug_section)
    {
      CONST asymbol *s;
      s = bfd_make_debug_symbol (stdoutput, (char *) 0, 0);
      assert (s != 0);
      debug_section = s->section;
    }
  return debug_section;
}

void
SA_SET_SYM_ENDNDX (sym, val)
     symbolS *sym;
     symbolS *val;
{
  combined_entry_type *entry, *p;

  entry = &coffsymbol (symbol_get_bfdsym (sym))->native[1];
  p = coffsymbol (symbol_get_bfdsym (val))->native;
  entry->u.auxent.x_sym.x_fcnary.x_fcn.x_endndx.p = p;
  entry->fix_end = 1;
}

static void
SA_SET_SYM_TAGNDX (sym, val)
     symbolS *sym;
     symbolS *val;
{
  combined_entry_type *entry, *p;

  entry = &coffsymbol (symbol_get_bfdsym (sym))->native[1];
  p = coffsymbol (symbol_get_bfdsym (val))->native;
  entry->u.auxent.x_sym.x_tagndx.p = p;
  entry->fix_tag = 1;
}

static int
S_GET_DATA_TYPE (sym)
     symbolS *sym;
{
  return coffsymbol (symbol_get_bfdsym (sym))->native->u.syment.n_type;
}

int
S_SET_DATA_TYPE (sym, val)
     symbolS *sym;
     int val;
{
  coffsymbol (symbol_get_bfdsym (sym))->native->u.syment.n_type = val;
  return val;
}

int
S_GET_STORAGE_CLASS (sym)
     symbolS *sym;
{
  return coffsymbol (symbol_get_bfdsym (sym))->native->u.syment.n_sclass;
}

int
S_SET_STORAGE_CLASS (sym, val)
     symbolS *sym;
     int val;
{
  coffsymbol (symbol_get_bfdsym (sym))->native->u.syment.n_sclass = val;
  return val;
}

/* Merge a debug symbol containing debug information into a normal symbol.  */

void
c_symbol_merge (debug, normal)
     symbolS *debug;
     symbolS *normal;
{
  S_SET_DATA_TYPE (normal, S_GET_DATA_TYPE (debug));
  S_SET_STORAGE_CLASS (normal, S_GET_STORAGE_CLASS (debug));

  if (S_GET_NUMBER_AUXILIARY (debug) > S_GET_NUMBER_AUXILIARY (normal))
    {
      /* take the most we have */
      S_SET_NUMBER_AUXILIARY (normal, S_GET_NUMBER_AUXILIARY (debug));
    }

  if (S_GET_NUMBER_AUXILIARY (debug) > 0)
    {
      /* Move all the auxiliary information.  */
      memcpy (SYM_AUXINFO (normal), SYM_AUXINFO (debug),
	      (S_GET_NUMBER_AUXILIARY (debug)
	       * sizeof (*SYM_AUXINFO (debug))));
    }

  /* Move the debug flags.  */
  SF_SET_DEBUG_FIELD (normal, SF_GET_DEBUG_FIELD (debug));
}

void
c_dot_file_symbol (filename)
     const char *filename;
{
  symbolS *symbolP;

  /* BFD converts filename to a .file symbol with an aux entry.  It
     also handles chaining.  */
  symbolP = symbol_new (filename, bfd_abs_section_ptr, 0, &zero_address_frag);

  S_SET_STORAGE_CLASS (symbolP, C_FILE);
  S_SET_NUMBER_AUXILIARY (symbolP, 1);

  symbol_get_bfdsym (symbolP)->flags = BSF_DEBUGGING;

#ifndef NO_LISTING
  {
    extern int listing;
    if (listing)
      {
	listing_source_file (filename);
      }
  }
#endif

  /* Make sure that the symbol is first on the symbol chain */
  if (symbol_rootP != symbolP)
    {
      symbol_remove (symbolP, &symbol_rootP, &symbol_lastP);
      symbol_insert (symbolP, symbol_rootP, &symbol_rootP, &symbol_lastP);
    }				/* if not first on the list */
}

/* Line number handling */

struct line_no {
  struct line_no *next;
  fragS *frag;
  alent l;
};

int coff_line_base;

/* Symbol of last function, which we should hang line#s off of.  */
static symbolS *line_fsym;

#define in_function()		(line_fsym != 0)
#define clear_function()	(line_fsym = 0)
#define set_function(F)		(line_fsym = (F), coff_add_linesym (F))


void
coff_obj_symbol_new_hook (symbolP)
     symbolS *symbolP;
{
  long   sz = (OBJ_COFF_MAX_AUXENTRIES + 1) * sizeof (combined_entry_type);
  char * s  = (char *) xmalloc (sz);

  memset (s, 0, sz);
  coffsymbol (symbol_get_bfdsym (symbolP))->native = (combined_entry_type *) s;

  S_SET_DATA_TYPE (symbolP, T_NULL);
  S_SET_STORAGE_CLASS (symbolP, 0);
  S_SET_NUMBER_AUXILIARY (symbolP, 0);

  if (S_IS_STRING (symbolP))
    SF_SET_STRING (symbolP);

  if (S_IS_LOCAL (symbolP))
    SF_SET_LOCAL (symbolP);
}


/*
 * Handle .ln directives.
 */

static symbolS *current_lineno_sym;
static struct line_no *line_nos;
/* @@ Blindly assume all .ln directives will be in the .text section...  */
int coff_n_line_nos;

static void
add_lineno (frag, offset, num)
     fragS *frag;
     addressT offset;
     int num;
{
  struct line_no *new_line =
    (struct line_no *) xmalloc (sizeof (struct line_no));
  if (!current_lineno_sym)
    {
      abort ();
    }
  if (num <= 0)
    {
      /* Zero is used as an end marker in the file.  */
      as_warn (_("Line numbers must be positive integers\n"));
      num = 1;
    }
  new_line->next = line_nos;
  new_line->frag = frag;
  new_line->l.line_number = num;
  new_line->l.u.offset = offset;
  line_nos = new_line;
  coff_n_line_nos++;
}

void
coff_add_linesym (sym)
     symbolS *sym;
{
  if (line_nos)
    {
      coffsymbol (symbol_get_bfdsym (current_lineno_sym))->lineno =
	(alent *) line_nos;
      coff_n_line_nos++;
      line_nos = 0;
    }
  current_lineno_sym = sym;
}

static void
obj_coff_ln (appline)
     int appline;
{
  int l;

  if (! appline && def_symbol_in_progress != NULL)
    {
      as_warn (_(".ln pseudo-op inside .def/.endef: ignored."));
      demand_empty_rest_of_line ();
      return;
    }

  l = get_absolute_expression ();
  if (!appline)
    {
      add_lineno (frag_now, frag_now_fix (), l);
    }

  if (appline)
    new_logical_line ((char *) NULL, l - 1);

#ifndef NO_LISTING
  {
    extern int listing;

    if (listing)
      {
	if (! appline)
	  l += coff_line_base - 1;
	listing_source_line (l);
      }
  }
#endif

  demand_empty_rest_of_line ();
}

/* .loc is essentially the same as .ln; parse it for assembler
   compatibility.  */

static void
obj_coff_loc (ignore)
     int ignore ATTRIBUTE_UNUSED;
{
  int lineno;

  /* FIXME: Why do we need this check?  We need it for ECOFF, but why
     do we need it for COFF?  */
  if (now_seg != text_section)
    {
      as_warn (_(".loc outside of .text"));
      demand_empty_rest_of_line ();
      return;
    }

  if (def_symbol_in_progress != NULL)
    {
      as_warn (_(".loc pseudo-op inside .def/.endef: ignored."));
      demand_empty_rest_of_line ();
      return;
    }

  /* Skip the file number.  */
  SKIP_WHITESPACE ();
  get_absolute_expression ();
  SKIP_WHITESPACE ();

  lineno = get_absolute_expression ();

#ifndef NO_LISTING
  {
    extern int listing;

    if (listing)
      {
        lineno += coff_line_base - 1;
	listing_source_line (lineno);
      }
  }
#endif

  demand_empty_rest_of_line ();

  add_lineno (frag_now, frag_now_fix (), lineno);
}

/* Handle the .ident pseudo-op.  */

static void
obj_coff_ident (ignore)
     int ignore ATTRIBUTE_UNUSED;
{
  segT current_seg = now_seg;
  subsegT current_subseg = now_subseg;

#ifdef TE_PE
  {
    segT sec;

    /* We could put it in .comment, but that creates an extra section
       that shouldn't be loaded into memory, which requires linker
       changes...  For now, until proven otherwise, use .rdata.  */
    sec = subseg_new (".rdata$zzz", 0);
    bfd_set_section_flags (stdoutput, sec,
			   ((SEC_ALLOC | SEC_LOAD | SEC_READONLY | SEC_DATA)
			    & bfd_applicable_section_flags (stdoutput)));
  }
#else
  subseg_new (".comment", 0);
#endif

  stringer (1);
  subseg_set (current_seg, current_subseg);
}

/*
 *			def()
 *
 * Handle .def directives.
 *
 * One might ask : why can't we symbol_new if the symbol does not
 * already exist and fill it with debug information.  Because of
 * the C_EFCN special symbol. It would clobber the value of the
 * function symbol before we have a chance to notice that it is
 * a C_EFCN. And a second reason is that the code is more clear this
 * way. (at least I think it is :-).
 *
 */

#define SKIP_SEMI_COLON()	while (*input_line_pointer++ != ';')
#define SKIP_WHITESPACES()	while (*input_line_pointer == ' ' || \
				       *input_line_pointer == '\t') \
    input_line_pointer++;

static void
obj_coff_def (what)
     int what ATTRIBUTE_UNUSED;
{
  char name_end;		/* Char after the end of name */
  char *symbol_name;		/* Name of the debug symbol */
  char *symbol_name_copy;	/* Temporary copy of the name */
  unsigned int symbol_name_length;

  if (def_symbol_in_progress != NULL)
    {
      as_warn (_(".def pseudo-op used inside of .def/.endef: ignored."));
      demand_empty_rest_of_line ();
      return;
    }				/* if not inside .def/.endef */

  SKIP_WHITESPACES ();

  symbol_name = input_line_pointer;
#ifdef STRIP_UNDERSCORE
  if (symbol_name[0] == '_' && symbol_name[1] != 0)
    symbol_name++;
#endif /* STRIP_UNDERSCORE */

  name_end = get_symbol_end ();
  symbol_name_length = strlen (symbol_name);
  symbol_name_copy = xmalloc (symbol_name_length + 1);
  strcpy (symbol_name_copy, symbol_name);
#ifdef tc_canonicalize_symbol_name
  symbol_name_copy = tc_canonicalize_symbol_name (symbol_name_copy);
#endif

  /* Initialize the new symbol */
  def_symbol_in_progress = symbol_make (symbol_name_copy);
  symbol_set_frag (def_symbol_in_progress, &zero_address_frag);
  S_SET_VALUE (def_symbol_in_progress, 0);

  if (S_IS_STRING (def_symbol_in_progress))
    SF_SET_STRING (def_symbol_in_progress);

  *input_line_pointer = name_end;

  demand_empty_rest_of_line ();
}

unsigned int dim_index;

static void
obj_coff_endef (ignore)
     int ignore ATTRIBUTE_UNUSED;
{
  symbolS *symbolP = NULL;

  /* DIM BUG FIX sac@cygnus.com */
  dim_index = 0;
  if (def_symbol_in_progress == NULL)
    {
      as_warn (_(".endef pseudo-op used outside of .def/.endef: ignored."));
      demand_empty_rest_of_line ();
      return;
    }				/* if not inside .def/.endef */

  /* Set the section number according to storage class.  */
  switch (S_GET_STORAGE_CLASS (def_symbol_in_progress))
    {
    case C_STRTAG:
    case C_ENTAG:
    case C_UNTAG:
      SF_SET_TAG (def_symbol_in_progress);
      /* intentional fallthrough */
    case C_FILE:
    case C_TPDEF:
      SF_SET_DEBUG (def_symbol_in_progress);
      S_SET_SEGMENT (def_symbol_in_progress, fetch_coff_debug_section ());
      break;

    case C_EFCN:
      SF_SET_LOCAL (def_symbol_in_progress);	/* Do not emit this symbol.  */
      /* intentional fallthrough */
    case C_BLOCK:
      SF_SET_PROCESS (def_symbol_in_progress);	/* Will need processing before writing */
      /* intentional fallthrough */
    case C_FCN:
      {
	CONST char *name;
	S_SET_SEGMENT (def_symbol_in_progress, text_section);

	name = S_GET_NAME (def_symbol_in_progress);
	if (name[0] == '.' && name[2] == 'f' && name[3] == '\0')
  	  {
	    switch (name[1])
	      {
	      case 'b':
		/* .bf */
		if (! in_function ())
		  as_warn (_("`%s' symbol without preceding function"), name);
		/* Will need relocating.  */
		SF_SET_PROCESS (def_symbol_in_progress);
		clear_function ();
		break;
#ifdef TE_PE
	      case 'e':
		/* .ef */
		/* The MS compilers output the actual endline, not the
		   function-relative one... we want to match without
		   changing the assembler input.  */
		SA_SET_SYM_LNNO (def_symbol_in_progress,
				 (SA_GET_SYM_LNNO (def_symbol_in_progress)
				  + coff_line_base));
		break;
#endif
	      }
	  }
      }
      break;

#ifdef C_AUTOARG
    case C_AUTOARG:
#endif /* C_AUTOARG */
    case C_AUTO:
    case C_REG:
    case C_ARG:
    case C_REGPARM:
    case C_FIELD:

    /* According to the COFF documentation:

       http://osr5doc.sco.com:1996/topics/COFF_SectNumFld.html

       A special section number (-2) marks symbolic debugging symbols,
       including structure/union/enumeration tag names, typedefs, and
       the name of the file. A section number of -1 indicates that the
       symbol has a value but is not relocatable. Examples of
       absolute-valued symbols include automatic and register variables,
       function arguments, and .eos symbols.

       But from Ian Lance Taylor:

       http://sources.redhat.com/ml/binutils/2000-08/msg00202.html

       the actual tools all marked them as section -1. So the GNU COFF
       assembler follows historical COFF assemblers.

       However, it causes problems for djgpp

       http://sources.redhat.com/ml/binutils/2000-08/msg00210.html

       By defining STRICTCOFF, a COFF port can make the assembler to
       follow the documented behavior.  */
#ifdef STRICTCOFF
    case C_MOS:
    case C_MOE:
    case C_MOU:
    case C_EOS:
#endif
      SF_SET_DEBUG (def_symbol_in_progress);
      S_SET_SEGMENT (def_symbol_in_progress, absolute_section);
      break;

#ifndef STRICTCOFF
    case C_MOS:
    case C_MOE:
    case C_MOU:
    case C_EOS:
      S_SET_SEGMENT (def_symbol_in_progress, absolute_section);
      break;
#endif

    case C_EXT:
    case C_WEAKEXT:
#ifdef TE_PE
    case C_NT_WEAK:
#endif
    case C_STAT:
    case C_LABEL:
      /* Valid but set somewhere else (s_comm, s_lcomm, colon) */
      break;

    default:
    case C_USTATIC:
    case C_EXTDEF:
    case C_ULABEL:
      as_warn (_("unexpected storage class %d"),
	       S_GET_STORAGE_CLASS (def_symbol_in_progress));
      break;
    }				/* switch on storage class */

  /* Now that we have built a debug symbol, try to find if we should
     merge with an existing symbol or not.  If a symbol is C_EFCN or
     absolute_section or untagged SEG_DEBUG it never merges.  We also
     don't merge labels, which are in a different namespace, nor
     symbols which have not yet been defined since they are typically
     unique, nor do we merge tags with non-tags.  */

  /* Two cases for functions.  Either debug followed by definition or
     definition followed by debug.  For definition first, we will
     merge the debug symbol into the definition.  For debug first, the
     lineno entry MUST point to the definition function or else it
     will point off into space when obj_crawl_symbol_chain() merges
     the debug symbol into the real symbol.  Therefor, let's presume
     the debug symbol is a real function reference.  */

  /* FIXME-SOON If for some reason the definition label/symbol is
     never seen, this will probably leave an undefined symbol at link
     time.  */

  if (S_GET_STORAGE_CLASS (def_symbol_in_progress) == C_EFCN
      || S_GET_STORAGE_CLASS (def_symbol_in_progress) == C_LABEL
      || (!strcmp (bfd_get_section_name (stdoutput,
					 S_GET_SEGMENT (def_symbol_in_progress)),
		   "*DEBUG*")
	  && !SF_GET_TAG (def_symbol_in_progress))
      || S_GET_SEGMENT (def_symbol_in_progress) == absolute_section
      || ! symbol_constant_p (def_symbol_in_progress)
      || (symbolP = symbol_find_base (S_GET_NAME (def_symbol_in_progress),
                                      DO_NOT_STRIP)) == NULL
      || SF_GET_TAG (def_symbol_in_progress) != SF_GET_TAG (symbolP))
    {
      /* If it already is at the end of the symbol list, do nothing */
      if (def_symbol_in_progress != symbol_lastP)
        {
	  symbol_remove (def_symbol_in_progress, &symbol_rootP, &symbol_lastP);
	  symbol_append (def_symbol_in_progress, symbol_lastP, &symbol_rootP,
			 &symbol_lastP);
        }
    }
  else
    {
      /* This symbol already exists, merge the newly created symbol
	 into the old one.  This is not mandatory. The linker can
	 handle duplicate symbols correctly. But I guess that it save
	 a *lot* of space if the assembly file defines a lot of
	 symbols. [loic] */

      /* The debug entry (def_symbol_in_progress) is merged into the
	 previous definition.  */

      c_symbol_merge (def_symbol_in_progress, symbolP);
      symbol_remove (def_symbol_in_progress, &symbol_rootP, &symbol_lastP);

      def_symbol_in_progress = symbolP;

      if (SF_GET_FUNCTION (def_symbol_in_progress)
	  || SF_GET_TAG (def_symbol_in_progress)
	  || S_GET_STORAGE_CLASS (def_symbol_in_progress) == C_STAT)
	{
	  /* For functions, and tags, and static symbols, the symbol
	     *must* be where the debug symbol appears.  Move the
	     existing symbol to the current place.  */
	  /* If it already is at the end of the symbol list, do nothing */
	  if (def_symbol_in_progress != symbol_lastP)
	    {
	      symbol_remove (def_symbol_in_progress, &symbol_rootP, &symbol_lastP);
	      symbol_append (def_symbol_in_progress, symbol_lastP, &symbol_rootP, &symbol_lastP);
	    }
	}
    }

  if (SF_GET_TAG (def_symbol_in_progress))
    {
      symbolS *oldtag;

      oldtag = symbol_find_base (S_GET_NAME (def_symbol_in_progress),
				 DO_NOT_STRIP);
      if (oldtag == NULL || ! SF_GET_TAG (oldtag))
	tag_insert (S_GET_NAME (def_symbol_in_progress),
		    def_symbol_in_progress);
    }

  if (SF_GET_FUNCTION (def_symbol_in_progress))
    {
      know (sizeof (def_symbol_in_progress) <= sizeof (long));
      set_function (def_symbol_in_progress);
      SF_SET_PROCESS (def_symbol_in_progress);

      if (symbolP == NULL)
	{
	  /* That is, if this is the first time we've seen the
	     function...  */
	  symbol_table_insert (def_symbol_in_progress);
	} /* definition follows debug */
    } /* Create the line number entry pointing to the function being defined */

  def_symbol_in_progress = NULL;
  demand_empty_rest_of_line ();
}

static void
obj_coff_dim (ignore)
     int ignore ATTRIBUTE_UNUSED;
{
  int dim_index;

  if (def_symbol_in_progress == NULL)
    {
      as_warn (_(".dim pseudo-op used outside of .def/.endef: ignored."));
      demand_empty_rest_of_line ();
      return;
    }				/* if not inside .def/.endef */

  S_SET_NUMBER_AUXILIARY (def_symbol_in_progress, 1);

  for (dim_index = 0; dim_index < DIMNUM; dim_index++)
    {
      SKIP_WHITESPACES ();
      SA_SET_SYM_DIMEN (def_symbol_in_progress, dim_index,
			get_absolute_expression ());

      switch (*input_line_pointer)
	{
	case ',':
	  input_line_pointer++;
	  break;

	default:
	  as_warn (_("badly formed .dim directive ignored"));
	  /* intentional fallthrough */
	case '\n':
	case ';':
	  dim_index = DIMNUM;
	  break;
	}
    }

  demand_empty_rest_of_line ();
}

static void
obj_coff_line (ignore)
     int ignore ATTRIBUTE_UNUSED;
{
  int this_base;

  if (def_symbol_in_progress == NULL)
    {
      /* Probably stabs-style line?  */
      obj_coff_ln (0);
      return;
    }

  this_base = get_absolute_expression ();
  if (!strcmp (".bf", S_GET_NAME (def_symbol_in_progress)))
    coff_line_base = this_base;

  S_SET_NUMBER_AUXILIARY (def_symbol_in_progress, 1);
  SA_SET_SYM_LNNO (def_symbol_in_progress, this_base);

  demand_empty_rest_of_line ();

#ifndef NO_LISTING
  if (strcmp (".bf", S_GET_NAME (def_symbol_in_progress)) == 0)
    {
      extern int listing;

      if (listing)
	listing_source_line ((unsigned int) this_base);
    }
#endif
}

static void
obj_coff_size (ignore)
     int ignore ATTRIBUTE_UNUSED;
{
  if (def_symbol_in_progress == NULL)
    {
      as_warn (_(".size pseudo-op used outside of .def/.endef ignored."));
      demand_empty_rest_of_line ();
      return;
    }				/* if not inside .def/.endef */

  S_SET_NUMBER_AUXILIARY (def_symbol_in_progress, 1);
  SA_SET_SYM_SIZE (def_symbol_in_progress, get_absolute_expression ());
  demand_empty_rest_of_line ();
}

static void
obj_coff_scl (ignore)
     int ignore ATTRIBUTE_UNUSED;
{
  if (def_symbol_in_progress == NULL)
    {
      as_warn (_(".scl pseudo-op used outside of .def/.endef ignored."));
      demand_empty_rest_of_line ();
      return;
    }				/* if not inside .def/.endef */

  S_SET_STORAGE_CLASS (def_symbol_in_progress, get_absolute_expression ());
  demand_empty_rest_of_line ();
}

static void
obj_coff_tag (ignore)
     int ignore ATTRIBUTE_UNUSED;
{
  char *symbol_name;
  char name_end;

  if (def_symbol_in_progress == NULL)
    {
      as_warn (_(".tag pseudo-op used outside of .def/.endef ignored."));
      demand_empty_rest_of_line ();
      return;
    }

  S_SET_NUMBER_AUXILIARY (def_symbol_in_progress, 1);
  symbol_name = input_line_pointer;
  name_end = get_symbol_end ();

#ifdef tc_canonicalize_symbol_name
  symbol_name = tc_canonicalize_symbol_name (symbol_name);
#endif

  /* Assume that the symbol referred to by .tag is always defined.
     This was a bad assumption.  I've added find_or_make. xoxorich.  */
  SA_SET_SYM_TAGNDX (def_symbol_in_progress,
		     tag_find_or_make (symbol_name));
  if (SA_GET_SYM_TAGNDX (def_symbol_in_progress) == 0L)
    {
      as_warn (_("tag not found for .tag %s"), symbol_name);
    }				/* not defined */

  SF_SET_TAGGED (def_symbol_in_progress);
  *input_line_pointer = name_end;

  demand_empty_rest_of_line ();
}

static void
obj_coff_type (ignore)
     int ignore ATTRIBUTE_UNUSED;
{
  if (def_symbol_in_progress == NULL)
    {
      as_warn (_(".type pseudo-op used outside of .def/.endef ignored."));
      demand_empty_rest_of_line ();
      return;
    }				/* if not inside .def/.endef */

  S_SET_DATA_TYPE (def_symbol_in_progress, get_absolute_expression ());

  if (ISFCN (S_GET_DATA_TYPE (def_symbol_in_progress)) &&
      S_GET_STORAGE_CLASS (def_symbol_in_progress) != C_TPDEF)
    {
      SF_SET_FUNCTION (def_symbol_in_progress);
    }				/* is a function */

  demand_empty_rest_of_line ();
}

static void
obj_coff_val (ignore)
     int ignore ATTRIBUTE_UNUSED;
{
  if (def_symbol_in_progress == NULL)
    {
      as_warn (_(".val pseudo-op used outside of .def/.endef ignored."));
      demand_empty_rest_of_line ();
      return;
    }				/* if not inside .def/.endef */

  if (is_name_beginner (*input_line_pointer))
    {
      char *symbol_name = input_line_pointer;
      char name_end = get_symbol_end ();

#ifdef tc_canonicalize_symbol_name
  symbol_name = tc_canonicalize_symbol_name (symbol_name);
#endif
      if (!strcmp (symbol_name, "."))
	{
	  symbol_set_frag (def_symbol_in_progress, frag_now);
	  S_SET_VALUE (def_symbol_in_progress, (valueT) frag_now_fix ());
	  /* If the .val is != from the .def (e.g. statics) */
	}
      else if (strcmp (S_GET_NAME (def_symbol_in_progress), symbol_name))
	{
	  expressionS exp;

	  exp.X_op = O_symbol;
	  exp.X_add_symbol = symbol_find_or_make (symbol_name);
	  exp.X_op_symbol = NULL;
	  exp.X_add_number = 0;
	  symbol_set_value_expression (def_symbol_in_progress, &exp);

	  /* If the segment is undefined when the forward reference is
	     resolved, then copy the segment id from the forward
	     symbol.  */
	  SF_SET_GET_SEGMENT (def_symbol_in_progress);

	  /* FIXME: gcc can generate address expressions here in
	     unusual cases (search for "obscure" in sdbout.c).  We
	     just ignore the offset here, thus generating incorrect
	     debugging information.  We ignore the rest of the line
	     just below.  */
	}
      /* Otherwise, it is the name of a non debug symbol and its value
         will be calculated later.  */
      *input_line_pointer = name_end;
    }
  else
    {
      S_SET_VALUE (def_symbol_in_progress, get_absolute_expression ());
    }				/* if symbol based */

  demand_empty_rest_of_line ();
}

void
coff_obj_read_begin_hook ()
{
  /* These had better be the same.  Usually 18 bytes.  */
#ifndef BFD_HEADERS
  know (sizeof (SYMENT) == sizeof (AUXENT));
  know (SYMESZ == AUXESZ);
#endif
  tag_init ();
}

symbolS *coff_last_function;
static symbolS *coff_last_bf;

void
coff_frob_symbol (symp, punt)
     symbolS *symp;
     int *punt;
{
  static symbolS *last_tagP;
  static stack *block_stack;
  static symbolS *set_end;
  symbolS *next_set_end = NULL;

  if (symp == &abs_symbol)
    {
      *punt = 1;
      return;
    }

  if (current_lineno_sym)
    coff_add_linesym ((symbolS *) 0);

  if (!block_stack)
    block_stack = stack_init (512, sizeof (symbolS*));

  if (S_IS_WEAK (symp))
    {
#ifdef TE_PE
      S_SET_STORAGE_CLASS (symp, C_NT_WEAK);
#else
      S_SET_STORAGE_CLASS (symp, C_WEAKEXT);
#endif
    }

  if (!S_IS_DEFINED (symp)
      && !S_IS_WEAK (symp)
      && S_GET_STORAGE_CLASS (symp) != C_STAT)
    S_SET_STORAGE_CLASS (symp, C_EXT);

  if (!SF_GET_DEBUG (symp))
    {
      symbolS *real;
      if (!SF_GET_LOCAL (symp)
	  && !SF_GET_STATICS (symp)
	  && S_GET_STORAGE_CLASS (symp) != C_LABEL
	  && symbol_constant_p(symp)
	  && (real = symbol_find_base (S_GET_NAME (symp), DO_NOT_STRIP))
	  && real != symp)
	{
	  c_symbol_merge (symp, real);
	  *punt = 1;
	  return;
	}
      if (!S_IS_DEFINED (symp) && !SF_GET_LOCAL (symp))
	{
	  assert (S_GET_VALUE (symp) == 0);
	  S_SET_EXTERNAL (symp);
	}
      else if (S_GET_STORAGE_CLASS (symp) == C_NULL)
	{
	  if (S_GET_SEGMENT (symp) == text_section
	      && symp != seg_info (text_section)->sym)
	    S_SET_STORAGE_CLASS (symp, C_LABEL);
	  else
	    S_SET_STORAGE_CLASS (symp, C_STAT);
	}
      if (SF_GET_PROCESS (symp))
	{
	  if (S_GET_STORAGE_CLASS (symp) == C_BLOCK)
	    {
	      if (!strcmp (S_GET_NAME (symp), ".bb"))
		stack_push (block_stack, (char *) &symp);
	      else
		{
		  symbolS *begin;
		  begin = *(symbolS **) stack_pop (block_stack);
		  if (begin == 0)
		    as_warn (_("mismatched .eb"));
		  else
		    next_set_end = begin;
		}
	    }
	  if (coff_last_function == 0 && SF_GET_FUNCTION (symp))
	    {
	      union internal_auxent *auxp;
	      coff_last_function = symp;
	      if (S_GET_NUMBER_AUXILIARY (symp) < 1)
		S_SET_NUMBER_AUXILIARY (symp, 1);
	      auxp = SYM_AUXENT (symp);
	      memset (auxp->x_sym.x_fcnary.x_ary.x_dimen, 0,
		      sizeof (auxp->x_sym.x_fcnary.x_ary.x_dimen));
	    }
	  if (S_GET_STORAGE_CLASS (symp) == C_EFCN)
	    {
	      if (coff_last_function == 0)
		as_fatal (_("C_EFCN symbol out of scope"));
	      SA_SET_SYM_FSIZE (coff_last_function,
				(long) (S_GET_VALUE (symp)
					- S_GET_VALUE (coff_last_function)));
	      next_set_end = coff_last_function;
	      coff_last_function = 0;
	    }
	}
      if (S_IS_EXTERNAL (symp))
	S_SET_STORAGE_CLASS (symp, C_EXT);
      else if (SF_GET_LOCAL (symp))
	*punt = 1;

      if (SF_GET_FUNCTION (symp))
	symbol_get_bfdsym (symp)->flags |= BSF_FUNCTION;

      /* more ...  */
    }

  /* Double check weak symbols.  */
  if (S_IS_WEAK (symp) && S_IS_COMMON (symp))
    as_bad (_("Symbol `%s' can not be both weak and common"),
	    S_GET_NAME (symp));

  if (SF_GET_TAG (symp))
    last_tagP = symp;
  else if (S_GET_STORAGE_CLASS (symp) == C_EOS)
    next_set_end = last_tagP;

#ifdef OBJ_XCOFF
  /* This is pretty horrible, but we have to set *punt correctly in
     order to call SA_SET_SYM_ENDNDX correctly.  */
  if (! symbol_used_in_reloc_p (symp)
      && ((symbol_get_bfdsym (symp)->flags & BSF_SECTION_SYM) != 0
	  || (! S_IS_EXTERNAL (symp)
	      && ! symbol_get_tc (symp)->output
	      && S_GET_STORAGE_CLASS (symp) != C_FILE)))
    *punt = 1;
#endif

  if (set_end != (symbolS *) NULL
      && ! *punt
      && ((symbol_get_bfdsym (symp)->flags & BSF_NOT_AT_END) != 0
	  || (S_IS_DEFINED (symp)
	      && ! S_IS_COMMON (symp)
	      && (! S_IS_EXTERNAL (symp) || SF_GET_FUNCTION (symp)))))
    {
      SA_SET_SYM_ENDNDX (set_end, symp);
      set_end = NULL;
    }

  if (next_set_end != NULL)
    {
      if (set_end != NULL)
	as_warn ("Warning: internal error: forgetting to set endndx of %s",
		 S_GET_NAME (set_end));
      set_end = next_set_end;
    }

  if (! *punt
      && S_GET_STORAGE_CLASS (symp) == C_FCN
      && strcmp (S_GET_NAME (symp), ".bf") == 0)
    {
      if (coff_last_bf != NULL)
	SA_SET_SYM_ENDNDX (coff_last_bf, symp);
      coff_last_bf = symp;
    }

  if (coffsymbol (symbol_get_bfdsym (symp))->lineno)
    {
      int i;
      struct line_no *lptr;
      alent *l;

      lptr = (struct line_no *) coffsymbol (symbol_get_bfdsym (symp))->lineno;
      for (i = 0; lptr; lptr = lptr->next)
	i++;
      lptr = (struct line_no *) coffsymbol (symbol_get_bfdsym (symp))->lineno;

      /* We need i entries for line numbers, plus 1 for the first
	 entry which BFD will override, plus 1 for the last zero
	 entry (a marker for BFD).  */
      l = (alent *) xmalloc ((i + 2) * sizeof (alent));
      coffsymbol (symbol_get_bfdsym (symp))->lineno = l;
      l[i + 1].line_number = 0;
      l[i + 1].u.sym = NULL;
      for (; i > 0; i--)
	{
	  if (lptr->frag)
	    lptr->l.u.offset += lptr->frag->fr_address / OCTETS_PER_BYTE;
	  l[i] = lptr->l;
	  lptr = lptr->next;
	}
    }
}

void
coff_adjust_section_syms (abfd, sec, x)
     bfd *abfd ATTRIBUTE_UNUSED;
     asection *sec;
     PTR x ATTRIBUTE_UNUSED;
{
  symbolS *secsym;
  segment_info_type *seginfo = seg_info (sec);
  int nlnno, nrelocs = 0;

  /* RS/6000 gas creates a .debug section manually in ppc_frob_file in
     tc-ppc.c.  Do not get confused by it.  */
  if (seginfo == NULL)
    return;

  if (!strcmp (sec->name, ".text"))
    nlnno = coff_n_line_nos;
  else
    nlnno = 0;
  {
    /* @@ Hope that none of the fixups expand to more than one reloc
       entry...  */
    fixS *fixp = seginfo->fix_root;
    while (fixp)
      {
	if (! fixp->fx_done)
	  nrelocs++;
	fixp = fixp->fx_next;
      }
  }
  if (bfd_get_section_size_before_reloc (sec) == 0
      && nrelocs == 0
      && nlnno == 0
      && sec != text_section
      && sec != data_section
      && sec != bss_section)
    return;
  secsym = section_symbol (sec);
  /* This is an estimate; we'll plug in the real value using
     SET_SECTION_RELOCS later */
  SA_SET_SCN_NRELOC (secsym, nrelocs);
  SA_SET_SCN_NLINNO (secsym, nlnno);
}

void
coff_frob_file_after_relocs ()
{
  bfd_map_over_sections (stdoutput, coff_adjust_section_syms, (char*) 0);
}

/*
 * implement the .section pseudo op:
 *	.section name {, "flags"}
 *                ^         ^
 *                |         +--- optional flags: 'b' for bss
 *                |                              'i' for info
 *                +-- section name               'l' for lib
 *                                               'n' for noload
 *                                               'o' for over
 *                                               'w' for data
 *						 'd' (apparently m88k for data)
 *                                               'x' for text
 *						 'r' for read-only data
 *						 's' for shared data (PE)
 * But if the argument is not a quoted string, treat it as a
 * subsegment number.
 */

void
obj_coff_section (ignore)
     int ignore ATTRIBUTE_UNUSED;
{
  /* Strip out the section name */
  char *section_name;
  char c;
  char *name;
  unsigned int exp;
  flagword flags, oldflags;
  asection *sec;

  if (flag_mri)
    {
      char type;

      s_mri_sect (&type);
      return;
    }

  section_name = input_line_pointer;
  c = get_symbol_end ();

  name = xmalloc (input_line_pointer - section_name + 1);
  strcpy (name, section_name);

  *input_line_pointer = c;

  SKIP_WHITESPACE ();

  exp = 0;
  flags = SEC_NO_FLAGS;

  if (*input_line_pointer == ',')
    {
      ++input_line_pointer;
      SKIP_WHITESPACE ();
      if (*input_line_pointer != '"')
	exp = get_absolute_expression ();
      else
	{
	  ++input_line_pointer;
	  while (*input_line_pointer != '"'
		 && ! is_end_of_line[(unsigned char) *input_line_pointer])
	    {
	      switch (*input_line_pointer)
		{
		case 'b': flags |= SEC_ALLOC; flags &=~ SEC_LOAD; break;
		case 'n': flags &=~ SEC_LOAD; break;
		case 'd': flags |= SEC_DATA | SEC_LOAD; /* fall through */
		case 'w': flags &=~ SEC_READONLY; break;
		case 'x': flags |= SEC_CODE | SEC_LOAD; break;
		case 'r': flags |= SEC_READONLY; break;
		case 's': flags |= SEC_SHARED; break;

		case 'i': /* STYP_INFO */
		case 'l': /* STYP_LIB */
		case 'o': /* STYP_OVER */
		  as_warn (_("unsupported section attribute '%c'"),
			   *input_line_pointer);
		  break;

		default:
		  as_warn(_("unknown section attribute '%c'"),
			  *input_line_pointer);
		  break;
		}
	      ++input_line_pointer;
	    }
	  if (*input_line_pointer == '"')
	    ++input_line_pointer;
	}
    }

  sec = subseg_new (name, (subsegT) exp);

  oldflags = bfd_get_section_flags (stdoutput, sec);
  if (oldflags == SEC_NO_FLAGS)
    {
      /* Set section flags for a new section just created by subseg_new.
         Provide a default if no flags were parsed.  */
      if (flags == SEC_NO_FLAGS)
	flags = TC_COFF_SECTION_DEFAULT_ATTRIBUTES;

#ifdef COFF_LONG_SECTION_NAMES
      /* Add SEC_LINK_ONCE and SEC_LINK_DUPLICATES_DISCARD to .gnu.linkonce
         sections so adjust_reloc_syms in write.c will correctly handle
         relocs which refer to non-local symbols in these sections.  */
      if (strncmp (name, ".gnu.linkonce", sizeof (".gnu.linkonce") - 1) == 0)
        flags |= SEC_LINK_ONCE | SEC_LINK_DUPLICATES_DISCARD;
#endif

      if (! bfd_set_section_flags (stdoutput, sec, flags))
        as_warn (_("error setting flags for \"%s\": %s"),
                 bfd_section_name (stdoutput, sec),
                 bfd_errmsg (bfd_get_error ()));
    }
  else if (flags != SEC_NO_FLAGS)
    {
      /* This section's attributes have already been set. Warn if the
         attributes don't match.  */
      flagword matchflags = SEC_ALLOC | SEC_LOAD | SEC_READONLY | SEC_CODE
                           | SEC_DATA | SEC_SHARED;
      if ((flags ^ oldflags) & matchflags)
	as_warn (_("Ignoring changed section attributes for %s"), name);
    }

  demand_empty_rest_of_line ();
}

void
coff_adjust_symtab ()
{
  if (symbol_rootP == NULL
      || S_GET_STORAGE_CLASS (symbol_rootP) != C_FILE)
    c_dot_file_symbol ("fake");
}

void
coff_frob_section (sec)
     segT sec;
{
  segT strsec;
  char *p;
  fragS *fragp;
  bfd_vma size, n_entries, mask;
  bfd_vma align_power = (bfd_vma)sec->alignment_power + OCTETS_PER_BYTE_POWER;

  /* The COFF back end in BFD requires that all section sizes be
     rounded up to multiples of the corresponding section alignments,
     supposedly because standard COFF has no other way of encoding alignment
     for sections.  If your COFF flavor has a different way of encoding
     section alignment, then skip this step, as TICOFF does.  */
  size = bfd_get_section_size_before_reloc (sec);
  mask = ((bfd_vma) 1 << align_power) - 1;
#if !defined(TICOFF)
  if (size & mask)
    {
      bfd_vma new_size;
      fragS *last;

      new_size = (size + mask) & ~mask;
      bfd_set_section_size (stdoutput, sec, new_size);

      /* If the size had to be rounded up, add some padding in
         the last non-empty frag.  */
      fragp = seg_info (sec)->frchainP->frch_root;
      last = seg_info (sec)->frchainP->frch_last;
      while (fragp->fr_next != last)
        fragp = fragp->fr_next;
      last->fr_address = size;
      fragp->fr_offset += new_size - size;
    }
#endif

  /* If the section size is non-zero, the section symbol needs an aux
     entry associated with it, indicating the size.  We don't know
     all the values yet; coff_frob_symbol will fill them in later.  */
#ifndef TICOFF
  if (size != 0
      || sec == text_section
      || sec == data_section
      || sec == bss_section)
#endif
    {
      symbolS *secsym = section_symbol (sec);

      S_SET_STORAGE_CLASS (secsym, C_STAT);
      S_SET_NUMBER_AUXILIARY (secsym, 1);
      SF_SET_STATICS (secsym);
      SA_SET_SCN_SCNLEN (secsym, size);
    }

  /* @@ these should be in a "stabs.h" file, or maybe as.h */
#ifndef STAB_SECTION_NAME
#define STAB_SECTION_NAME ".stab"
#endif
#ifndef STAB_STRING_SECTION_NAME
#define STAB_STRING_SECTION_NAME ".stabstr"
#endif
  if (strcmp (STAB_STRING_SECTION_NAME, sec->name))
    return;

  strsec = sec;
  sec = subseg_get (STAB_SECTION_NAME, 0);
  /* size is already rounded up, since other section will be listed first */
  size = bfd_get_section_size_before_reloc (strsec);

  n_entries = bfd_get_section_size_before_reloc (sec) / 12 - 1;

  /* Find first non-empty frag.  It should be large enough.  */
  fragp = seg_info (sec)->frchainP->frch_root;
  while (fragp && fragp->fr_fix == 0)
    fragp = fragp->fr_next;
  assert (fragp != 0 && fragp->fr_fix >= 12);

  /* Store the values.  */
  p = fragp->fr_literal;
  bfd_h_put_16 (stdoutput, n_entries, (bfd_byte *) p + 6);
  bfd_h_put_32 (stdoutput, size, (bfd_byte *) p + 8);
}

void
obj_coff_init_stab_section (seg)
     segT seg;
{
  char *file;
  char *p;
  char *stabstr_name;
  unsigned int stroff;

  /* Make space for this first symbol.  */
  p = frag_more (12);
  /* Zero it out.  */
  memset (p, 0, 12);
  as_where (&file, (unsigned int *) NULL);
  stabstr_name = (char *) alloca (strlen (seg->name) + 4);
  strcpy (stabstr_name, seg->name);
  strcat (stabstr_name, "str");
  stroff = get_stab_string_offset (file, stabstr_name);
  know (stroff == 1);
  md_number_to_chars (p, stroff, 4);
}

#ifdef DEBUG
/* for debugging */
const char *
s_get_name (s)
     symbolS *s;
{
  return ((s == NULL) ? "(NULL)" : S_GET_NAME (s));
}

void
symbol_dump ()
{
  symbolS *symbolP;

  for (symbolP = symbol_rootP; symbolP; symbolP = symbol_next (symbolP))
    {
      printf (_("0x%lx: \"%s\" type = %ld, class = %d, segment = %d\n"),
	     (unsigned long) symbolP,
	     S_GET_NAME(symbolP),
	     (long) S_GET_DATA_TYPE(symbolP),
	     S_GET_STORAGE_CLASS(symbolP),
	     (int) S_GET_SEGMENT(symbolP));
    }
}

#endif /* DEBUG */

#else /* not BFD_ASSEMBLER */

#include "frags.h"
/* This is needed because we include internal bfd things.  */
#include <time.h>

#include "libbfd.h"
#include "libcoff.h"

#ifdef TE_PE
#include "coff/pe.h"
#endif

/* The NOP_OPCODE is for the alignment fill value.  Fill with nop so
   that we can stick sections together without causing trouble.  */
#ifndef NOP_OPCODE
#define NOP_OPCODE 0x00
#endif

/* The zeroes if symbol name is longer than 8 chars */
#define S_SET_ZEROES(s,v)		((s)->sy_symbol.ost_entry.n_zeroes = (v))

#define MIN(a,b) ((a) < (b)? (a) : (b))

/* This vector is used to turn a gas internal segment number into a
   section number suitable for insertion into a coff symbol table.
   This must correspond to seg_info_off_by_4.  */

const short seg_N_TYPE[] =
{				/* in: segT   out: N_TYPE bits */
  C_ABS_SECTION,
  1,    2,  3,   4,    5,   6,   7,   8,   9,  10,
  11,  12,  13,  14,  15,  16,  17,  18,  19,  20,
  21,  22,  23,  24,  25,  26,  27,  28,  29,  30,
  31,  32,  33,  34,  35,  36,  37,  38,  39,  40,
  C_UNDEF_SECTION,		/* SEG_UNKNOWN */
  C_UNDEF_SECTION,		/* SEG_GOOF */
  C_UNDEF_SECTION,		/* SEG_EXPR */
  C_DEBUG_SECTION,		/* SEG_DEBUG */
  C_NTV_SECTION,		/* SEG_NTV */
  C_PTV_SECTION,		/* SEG_PTV */
  C_REGISTER_SECTION,		/* SEG_REGISTER */
};

int function_lineoff = -1;	/* Offset in line#s where the last function
				   started (the odd entry for line #0) */

/* structure used to keep the filenames which
   are too long around so that we can stick them
   into the string table */
struct filename_list
{
  char *filename;
  struct filename_list *next;
};

static struct filename_list *filename_list_head;
static struct filename_list *filename_list_tail;

static symbolS *last_line_symbol;

/* Add 4 to the real value to get the index and compensate the
   negatives. This vector is used by S_GET_SEGMENT to turn a coff
   section number into a segment number
*/
static symbolS *previous_file_symbol;
void c_symbol_merge ();
static int line_base;

symbolS *c_section_symbol ();
bfd *abfd;

static void fixup_segment PARAMS ((segment_info_type *segP,
				   segT this_segment_type));

static void fixup_mdeps PARAMS ((fragS *,
				 object_headers *,
				 segT));

static void fill_section PARAMS ((bfd * abfd,
				  object_headers *,
				  unsigned long *));

static int c_line_new PARAMS ((symbolS * symbol, long paddr,
			       int line_number,
			       fragS * frag));

static void w_symbols PARAMS ((bfd * abfd, char *where,
			       symbolS * symbol_rootP));

static void adjust_stab_section PARAMS ((bfd *abfd, segT seg));

static void obj_coff_lcomm PARAMS ((int));
static void obj_coff_text PARAMS ((int));
static void obj_coff_data PARAMS ((int));
void obj_coff_section PARAMS ((int));

/* When not using BFD_ASSEMBLER, we permit up to 40 sections.

   This array maps a COFF section number into a gas section number.
   Because COFF uses negative section numbers, you must add 4 to the
   COFF section number when indexing into this array; this is done via
   the SEG_INFO_FROM_SECTION_NUMBER macro.  This must correspond to
   seg_N_TYPE.  */

static const segT seg_info_off_by_4[] =
{
 SEG_PTV,
 SEG_NTV,
 SEG_DEBUG,
 SEG_ABSOLUTE,
 SEG_UNKNOWN,
 SEG_E0,  SEG_E1,  SEG_E2,  SEG_E3,  SEG_E4,
 SEG_E5,  SEG_E6,  SEG_E7,  SEG_E8,  SEG_E9,
 SEG_E10, SEG_E11, SEG_E12, SEG_E13, SEG_E14,
 SEG_E15, SEG_E16, SEG_E17, SEG_E18, SEG_E19,
 SEG_E20, SEG_E21, SEG_E22, SEG_E23, SEG_E24,
 SEG_E25, SEG_E26, SEG_E27, SEG_E28, SEG_E29,
 SEG_E30, SEG_E31, SEG_E32, SEG_E33, SEG_E34,
 SEG_E35, SEG_E36, SEG_E37, SEG_E38, SEG_E39,
 (segT) 40,
 (segT) 41,
 (segT) 42,
 (segT) 43,
 (segT) 44,
 (segT) 45,
 (segT) 0,
 (segT) 0,
 (segT) 0,
 SEG_REGISTER
};

#define SEG_INFO_FROM_SECTION_NUMBER(x) (seg_info_off_by_4[(x)+4])

static relax_addressT
relax_align (address, alignment)
     relax_addressT address;
     long alignment;
{
  relax_addressT mask;
  relax_addressT new_address;

  mask = ~((~0) << alignment);
  new_address = (address + mask) & (~mask);
  return (new_address - address);
}

segT
s_get_segment (x)
     symbolS * x;
{
  return SEG_INFO_FROM_SECTION_NUMBER (x->sy_symbol.ost_entry.n_scnum);
}

/* calculate the size of the frag chain and fill in the section header
   to contain all of it, also fill in the addr of the sections */
static unsigned int
size_section (abfd, idx)
     bfd *abfd ATTRIBUTE_UNUSED;
     unsigned int idx;
{

  unsigned int size = 0;
  fragS *frag = segment_info[idx].frchainP->frch_root;
  while (frag)
    {
      size = frag->fr_address;
      if (frag->fr_address != size)
	{
	  fprintf (stderr, _("Out of step\n"));
	  size = frag->fr_address;
	}

      switch (frag->fr_type)
	{
#ifdef TC_COFF_SIZEMACHDEP
	case rs_machine_dependent:
	  size += TC_COFF_SIZEMACHDEP (frag);
	  break;
#endif
	case rs_space:
	  assert (frag->fr_symbol == 0);
	case rs_fill:
	case rs_org:
	  size += frag->fr_fix;
	  size += frag->fr_offset * frag->fr_var;
	  break;
	case rs_align:
	case rs_align_code:
	case rs_align_test:
	  {
	    addressT off;

	    size += frag->fr_fix;
	    off = relax_align (size, frag->fr_offset);
	    if (frag->fr_subtype != 0 && off > frag->fr_subtype)
	      off = 0;
	    size += off;
	  }
	  break;
	default:
	  BAD_CASE (frag->fr_type);
	  break;
	}
      frag = frag->fr_next;
    }
  segment_info[idx].scnhdr.s_size = size;
  return size;
}

static unsigned int
count_entries_in_chain (idx)
     unsigned int idx;
{
  unsigned int nrelocs;
  fixS *fixup_ptr;

  /* Count the relocations */
  fixup_ptr = segment_info[idx].fix_root;
  nrelocs = 0;
  while (fixup_ptr != (fixS *) NULL)
    {
      if (fixup_ptr->fx_done == 0 && TC_COUNT_RELOC (fixup_ptr))
	{
#ifdef TC_A29K
	  if (fixup_ptr->fx_r_type == RELOC_CONSTH)
	    nrelocs += 2;
	  else
	    nrelocs++;
#else
	  nrelocs++;
#endif
	}

      fixup_ptr = fixup_ptr->fx_next;
    }
  return nrelocs;
}

#ifdef TE_AUX

static int compare_external_relocs PARAMS ((const PTR, const PTR));

/* AUX's ld expects relocations to be sorted */
static int
compare_external_relocs (x, y)
     const PTR x;
     const PTR y;
{
  struct external_reloc *a = (struct external_reloc *) x;
  struct external_reloc *b = (struct external_reloc *) y;
  bfd_vma aadr = bfd_getb32 (a->r_vaddr);
  bfd_vma badr = bfd_getb32 (b->r_vaddr);
  return (aadr < badr ? -1 : badr < aadr ? 1 : 0);
}

#endif

/* output all the relocations for a section */
void
do_relocs_for (abfd, h, file_cursor)
     bfd * abfd;
     object_headers * h;
     unsigned long *file_cursor;
{
  unsigned int nrelocs;
  unsigned int idx;
  unsigned long reloc_start = *file_cursor;

  for (idx = SEG_E0; idx < SEG_LAST; idx++)
    {
      if (segment_info[idx].scnhdr.s_name[0])
	{
	  struct external_reloc *ext_ptr;
	  struct external_reloc *external_reloc_vec;
	  unsigned int external_reloc_size;
	  unsigned int base = segment_info[idx].scnhdr.s_paddr;
	  fixS *fix_ptr = segment_info[idx].fix_root;
	  nrelocs = count_entries_in_chain (idx);

	  if (nrelocs)
	    /* Bypass this stuff if no relocs.  This also incidentally
	       avoids a SCO bug, where free(malloc(0)) tends to crash.  */
	    {
	      external_reloc_size = nrelocs * RELSZ;
	      external_reloc_vec =
		(struct external_reloc *) malloc (external_reloc_size);

	      ext_ptr = external_reloc_vec;

	      /* Fill in the internal coff style reloc struct from the
		 internal fix list.  */
	      while (fix_ptr)
		{
		  struct internal_reloc intr;

		  /* Only output some of the relocations */
		  if (fix_ptr->fx_done == 0 && TC_COUNT_RELOC (fix_ptr))
		    {
#ifdef TC_RELOC_MANGLE
		      TC_RELOC_MANGLE (&segment_info[idx], fix_ptr, &intr,
				       base);

#else
		      symbolS *dot;
		      symbolS *symbol_ptr = fix_ptr->fx_addsy;

		      intr.r_type = TC_COFF_FIX2RTYPE (fix_ptr);
		      intr.r_vaddr =
			base + fix_ptr->fx_frag->fr_address + fix_ptr->fx_where;

#ifdef TC_KEEP_FX_OFFSET
		      intr.r_offset = fix_ptr->fx_offset;
#else
		      intr.r_offset = 0;
#endif

		      while (symbol_ptr->sy_value.X_op == O_symbol
			     && (! S_IS_DEFINED (symbol_ptr)
				 || S_IS_COMMON (symbol_ptr)))
			{
			  symbolS *n;

			  /* We must avoid looping, as that can occur
                             with a badly written program.  */
			  n = symbol_ptr->sy_value.X_add_symbol;
			  if (n == symbol_ptr)
			    break;
			  symbol_ptr = n;
			}

		      /* Turn the segment of the symbol into an offset.  */
		      if (symbol_ptr)
			{
			  resolve_symbol_value (symbol_ptr, 1);
			  if (! symbol_ptr->sy_resolved)
			    {
			      char *file;
			      unsigned int line;

			      if (expr_symbol_where (symbol_ptr, &file, &line))
				as_bad_where (file, line,
					      _("unresolved relocation"));
			      else
				as_bad (_("bad relocation: symbol `%s' not in symbol table"),
					S_GET_NAME (symbol_ptr));
			    }
			  dot = segment_info[S_GET_SEGMENT (symbol_ptr)].dot;
			  if (dot)
			    {
			      intr.r_symndx = dot->sy_number;
			    }
			  else
			    {
			      intr.r_symndx = symbol_ptr->sy_number;
			    }

			}
		      else
			{
			  intr.r_symndx = -1;
			}
#endif

		      (void) bfd_coff_swap_reloc_out (abfd, &intr, ext_ptr);
		      ext_ptr++;

#if defined(TC_A29K)

		      /* The 29k has a special kludge for the high 16 bit
			 reloc.  Two relocations are emited, R_IHIHALF,
			 and R_IHCONST. The second one doesn't contain a
			 symbol, but uses the value for offset.  */

		      if (intr.r_type == R_IHIHALF)
			{
			  /* now emit the second bit */
			  intr.r_type = R_IHCONST;
			  intr.r_symndx = fix_ptr->fx_addnumber;
			  (void) bfd_coff_swap_reloc_out (abfd, &intr, ext_ptr);
			  ext_ptr++;
			}
#endif
		    }

		  fix_ptr = fix_ptr->fx_next;
		}

#ifdef TE_AUX
	      /* Sort the reloc table */
	      qsort ((PTR) external_reloc_vec, nrelocs,
		     sizeof (struct external_reloc), compare_external_relocs);
#endif

	      /* Write out the reloc table */
	      bfd_write ((PTR) external_reloc_vec, 1, external_reloc_size,
			 abfd);
	      free (external_reloc_vec);

	      /* Fill in section header info.  */
	      segment_info[idx].scnhdr.s_relptr = *file_cursor;
	      *file_cursor += external_reloc_size;
	      segment_info[idx].scnhdr.s_nreloc = nrelocs;
	    }
	  else
	    {
	      /* No relocs */
	      segment_info[idx].scnhdr.s_relptr = 0;
	    }
	}
    }
  /* Set relocation_size field in file headers */
  H_SET_RELOCATION_SIZE (h, *file_cursor - reloc_start, 0);
}

/* run through a frag chain and write out the data to go with it, fill
   in the scnhdrs with the info on the file postions
*/
static void
fill_section (abfd, h, file_cursor)
     bfd * abfd;
     object_headers *h ATTRIBUTE_UNUSED;
     unsigned long *file_cursor;
{

  unsigned int i;
  unsigned int paddr = 0;

  for (i = SEG_E0; i < SEG_UNKNOWN; i++)
    {
      unsigned int offset = 0;
      struct internal_scnhdr *s = &(segment_info[i].scnhdr);

      PROGRESS (1);

      if (s->s_name[0])
	{
	  fragS *frag = segment_info[i].frchainP->frch_root;
	  char *buffer;

	  if (s->s_size == 0)
	    s->s_scnptr = 0;
	  else
	    {
	      buffer = xmalloc (s->s_size);
	      s->s_scnptr = *file_cursor;
	    }
	  know (s->s_paddr == paddr);

	  if (strcmp (s->s_name, ".text") == 0)
	    s->s_flags |= STYP_TEXT;
	  else if (strcmp (s->s_name, ".data") == 0)
	    s->s_flags |= STYP_DATA;
	  else if (strcmp (s->s_name, ".bss") == 0)
	    {
	      s->s_scnptr = 0;
	      s->s_flags |= STYP_BSS;

	      /* @@ Should make the i386 and a29k coff targets define
		 COFF_NOLOAD_PROBLEM, and have only one test here.  */
#ifndef TC_I386
#ifndef TC_A29K
#ifndef COFF_NOLOAD_PROBLEM
	      /* Apparently the SVR3 linker (and exec syscall) and UDI
		 mondfe progrem are confused by noload sections.  */
	      s->s_flags |= STYP_NOLOAD;
#endif
#endif
#endif
	    }
	  else if (strcmp (s->s_name, ".lit") == 0)
	    s->s_flags = STYP_LIT | STYP_TEXT;
	  else if (strcmp (s->s_name, ".init") == 0)
	    s->s_flags |= STYP_TEXT;
	  else if (strcmp (s->s_name, ".fini") == 0)
	    s->s_flags |= STYP_TEXT;
	  else if (strncmp (s->s_name, ".comment", 8) == 0)
	    s->s_flags |= STYP_INFO;

	  while (frag)
	    {
	      unsigned int fill_size;
	      switch (frag->fr_type)
		{
		case rs_machine_dependent:
		  if (frag->fr_fix)
		    {
		      memcpy (buffer + frag->fr_address,
			      frag->fr_literal,
			      (unsigned int) frag->fr_fix);
		      offset += frag->fr_fix;
		    }

		  break;
		case rs_space:
		  assert (frag->fr_symbol == 0);
		case rs_fill:
		case rs_align:
		case rs_align_code:
		case rs_align_test:
		case rs_org:
		  if (frag->fr_fix)
		    {
		      memcpy (buffer + frag->fr_address,
			      frag->fr_literal,
			      (unsigned int) frag->fr_fix);
		      offset += frag->fr_fix;
		    }

		  fill_size = frag->fr_var;
		  if (fill_size && frag->fr_offset > 0)
		    {
		      unsigned int count;
		      unsigned int off = frag->fr_fix;
		      for (count = frag->fr_offset; count; count--)
			{
			  if (fill_size + frag->fr_address + off <= s->s_size)
			    {
			      memcpy (buffer + frag->fr_address + off,
				      frag->fr_literal + frag->fr_fix,
				      fill_size);
			      off += fill_size;
			      offset += fill_size;
			    }
			}
		    }
		  break;
		case rs_broken_word:
		  break;
		default:
		  abort ();
		}
	      frag = frag->fr_next;
	    }

	  if (s->s_size != 0)
	    {
	      if (s->s_scnptr != 0)
		{
		  bfd_write (buffer, s->s_size, 1, abfd);
		  *file_cursor += s->s_size;
		}
	      free (buffer);
	    }
	  paddr += s->s_size;
	}
    }
}

/* Coff file generation & utilities */

static void
coff_header_append (abfd, h)
     bfd * abfd;
     object_headers * h;
{
  unsigned int i;
  char buffer[1000];
  char buffero[1000];
#ifdef COFF_LONG_SECTION_NAMES
  unsigned long string_size = 4;
#endif

  bfd_seek (abfd, 0, 0);

#ifndef OBJ_COFF_OMIT_OPTIONAL_HEADER
  H_SET_MAGIC_NUMBER (h, COFF_MAGIC);
  H_SET_VERSION_STAMP (h, 0);
  H_SET_ENTRY_POINT (h, 0);
  H_SET_TEXT_START (h, segment_info[SEG_E0].frchainP->frch_root->fr_address);
  H_SET_DATA_START (h, segment_info[SEG_E1].frchainP->frch_root->fr_address);
  H_SET_SIZEOF_OPTIONAL_HEADER (h, bfd_coff_swap_aouthdr_out(abfd, &h->aouthdr,
							     buffero));
#else /* defined (OBJ_COFF_OMIT_OPTIONAL_HEADER) */
  H_SET_SIZEOF_OPTIONAL_HEADER (h, 0);
#endif /* defined (OBJ_COFF_OMIT_OPTIONAL_HEADER) */

  i = bfd_coff_swap_filehdr_out (abfd, &h->filehdr, buffer);

  bfd_write (buffer, i, 1, abfd);
  bfd_write (buffero, H_GET_SIZEOF_OPTIONAL_HEADER (h), 1, abfd);

  for (i = SEG_E0; i < SEG_LAST; i++)
    {
      if (segment_info[i].scnhdr.s_name[0])
	{
	  unsigned int size;

#ifdef COFF_LONG_SECTION_NAMES
	  /* Support long section names as found in PE.  This code
             must coordinate with that in write_object_file and
             w_strings.  */
	  if (strlen (segment_info[i].name) > SCNNMLEN)
	    {
	      memset (segment_info[i].scnhdr.s_name, 0, SCNNMLEN);
	      sprintf (segment_info[i].scnhdr.s_name, "/%lu", string_size);
	      string_size += strlen (segment_info[i].name) + 1;
	    }
#endif

	  size = bfd_coff_swap_scnhdr_out (abfd,
					   &(segment_info[i].scnhdr),
					   buffer);
	  if (size == 0)
	    as_bad (_("bfd_coff_swap_scnhdr_out failed"));
	  bfd_write (buffer, size, 1, abfd);
	}
    }
}

char *
symbol_to_chars (abfd, where, symbolP)
     bfd * abfd;
     char *where;
     symbolS * symbolP;
{
  unsigned int numaux = symbolP->sy_symbol.ost_entry.n_numaux;
  unsigned int i;
  valueT val;

  /* Turn any symbols with register attributes into abs symbols */
  if (S_GET_SEGMENT (symbolP) == reg_section)
    {
      S_SET_SEGMENT (symbolP, absolute_section);
    }
  /* At the same time, relocate all symbols to their output value */

#ifndef TE_PE
  val = (segment_info[S_GET_SEGMENT (symbolP)].scnhdr.s_paddr
	 + S_GET_VALUE (symbolP));
#else
  val = S_GET_VALUE (symbolP);
#endif

  S_SET_VALUE (symbolP, val);

  symbolP->sy_symbol.ost_entry.n_value = val;

  where += bfd_coff_swap_sym_out (abfd, &symbolP->sy_symbol.ost_entry,
				  where);

  for (i = 0; i < numaux; i++)
    {
      where += bfd_coff_swap_aux_out (abfd,
				      &symbolP->sy_symbol.ost_auxent[i],
				      S_GET_DATA_TYPE (symbolP),
				      S_GET_STORAGE_CLASS (symbolP),
				      i, numaux, where);
    }
  return where;

}

void
coff_obj_symbol_new_hook (symbolP)
     symbolS *symbolP;
{
  char underscore = 0;		/* Symbol has leading _ */

  /* Effective symbol */
  /* Store the pointer in the offset.  */
  S_SET_ZEROES (symbolP, 0L);
  S_SET_DATA_TYPE (symbolP, T_NULL);
  S_SET_STORAGE_CLASS (symbolP, 0);
  S_SET_NUMBER_AUXILIARY (symbolP, 0);
  /* Additional information */
  symbolP->sy_symbol.ost_flags = 0;
  /* Auxiliary entries */
  memset ((char *) &symbolP->sy_symbol.ost_auxent[0], 0, AUXESZ);

  if (S_IS_STRING (symbolP))
    SF_SET_STRING (symbolP);
  if (!underscore && S_IS_LOCAL (symbolP))
    SF_SET_LOCAL (symbolP);
}

/*
 * Handle .ln directives.
 */

static void
obj_coff_ln (appline)
     int appline;
{
  int l;

  if (! appline && def_symbol_in_progress != NULL)
    {
      as_warn (_(".ln pseudo-op inside .def/.endef: ignored."));
      demand_empty_rest_of_line ();
      return;
    }				/* wrong context */

  l = get_absolute_expression ();
  c_line_new (0, frag_now_fix (), l, frag_now);

  if (appline)
    new_logical_line ((char *) NULL, l - 1);

#ifndef NO_LISTING
  {
    extern int listing;

    if (listing)
      {
	if (! appline)
	  l += line_base - 1;
	listing_source_line ((unsigned int) l);
      }

  }
#endif
  demand_empty_rest_of_line ();
}

/*
 *			def()
 *
 * Handle .def directives.
 *
 * One might ask : why can't we symbol_new if the symbol does not
 * already exist and fill it with debug information.  Because of
 * the C_EFCN special symbol. It would clobber the value of the
 * function symbol before we have a chance to notice that it is
 * a C_EFCN. And a second reason is that the code is more clear this
 * way. (at least I think it is :-).
 *
 */

#define SKIP_SEMI_COLON()	while (*input_line_pointer++ != ';')
#define SKIP_WHITESPACES()	while (*input_line_pointer == ' ' || \
				      *input_line_pointer == '\t') \
                                         input_line_pointer++;

static void
obj_coff_def (what)
     int what ATTRIBUTE_UNUSED;
{
  char name_end;		/* Char after the end of name */
  char *symbol_name;		/* Name of the debug symbol */
  char *symbol_name_copy;	/* Temporary copy of the name */
  unsigned int symbol_name_length;

  if (def_symbol_in_progress != NULL)
    {
      as_warn (_(".def pseudo-op used inside of .def/.endef: ignored."));
      demand_empty_rest_of_line ();
      return;
    }				/* if not inside .def/.endef */

  SKIP_WHITESPACES ();

  def_symbol_in_progress = (symbolS *) obstack_alloc (&notes, sizeof (*def_symbol_in_progress));
  memset (def_symbol_in_progress, 0, sizeof (*def_symbol_in_progress));

  symbol_name = input_line_pointer;
  name_end = get_symbol_end ();
  symbol_name_length = strlen (symbol_name);
  symbol_name_copy = xmalloc (symbol_name_length + 1);
  strcpy (symbol_name_copy, symbol_name);
#ifdef tc_canonicalize_symbol_name
  symbol_name_copy = tc_canonicalize_symbol_name (symbol_name_copy);
#endif

  /* Initialize the new symbol */
#ifdef STRIP_UNDERSCORE
  S_SET_NAME (def_symbol_in_progress, (*symbol_name_copy == '_'
				       ? symbol_name_copy + 1
				       : symbol_name_copy));
#else /* STRIP_UNDERSCORE */
  S_SET_NAME (def_symbol_in_progress, symbol_name_copy);
#endif /* STRIP_UNDERSCORE */
  /* free(symbol_name_copy); */
  def_symbol_in_progress->sy_name_offset = (unsigned long) ~0;
  def_symbol_in_progress->sy_number = ~0;
  def_symbol_in_progress->sy_frag = &zero_address_frag;
  S_SET_VALUE (def_symbol_in_progress, 0);

  if (S_IS_STRING (def_symbol_in_progress))
    SF_SET_STRING (def_symbol_in_progress);

  *input_line_pointer = name_end;

  demand_empty_rest_of_line ();
}

unsigned int dim_index;

static void
obj_coff_endef (ignore)
     int ignore ATTRIBUTE_UNUSED;
{
  symbolS *symbolP = 0;
  /* DIM BUG FIX sac@cygnus.com */
  dim_index = 0;
  if (def_symbol_in_progress == NULL)
    {
      as_warn (_(".endef pseudo-op used outside of .def/.endef: ignored."));
      demand_empty_rest_of_line ();
      return;
    }				/* if not inside .def/.endef */

  /* Set the section number according to storage class.  */
  switch (S_GET_STORAGE_CLASS (def_symbol_in_progress))
    {
    case C_STRTAG:
    case C_ENTAG:
    case C_UNTAG:
      SF_SET_TAG (def_symbol_in_progress);
      /* intentional fallthrough */
    case C_FILE:
    case C_TPDEF:
      SF_SET_DEBUG (def_symbol_in_progress);
      S_SET_SEGMENT (def_symbol_in_progress, SEG_DEBUG);
      break;

    case C_EFCN:
      SF_SET_LOCAL (def_symbol_in_progress);	/* Do not emit this symbol.  */
      /* intentional fallthrough */
    case C_BLOCK:
      SF_SET_PROCESS (def_symbol_in_progress);	/* Will need processing before writing */
      /* intentional fallthrough */
    case C_FCN:
      S_SET_SEGMENT (def_symbol_in_progress, SEG_E0);

      if (strcmp (S_GET_NAME (def_symbol_in_progress), ".bf") == 0)
	{			/* .bf */
	  if (function_lineoff < 0)
	    {
	      fprintf (stderr, _("`.bf' symbol without preceding function\n"));
	    }			/* missing function symbol */
	  SA_GET_SYM_LNNOPTR (last_line_symbol) = function_lineoff;

	  SF_SET_PROCESS (last_line_symbol);
	  SF_SET_ADJ_LNNOPTR (last_line_symbol);
	  SF_SET_PROCESS (def_symbol_in_progress);
	  function_lineoff = -1;
	}
      /* Value is always set to .  */
      def_symbol_in_progress->sy_frag = frag_now;
      S_SET_VALUE (def_symbol_in_progress, (valueT) frag_now_fix ());
      break;

#ifdef C_AUTOARG
    case C_AUTOARG:
#endif /* C_AUTOARG */
    case C_AUTO:
    case C_REG:
    case C_MOS:
    case C_MOE:
    case C_MOU:
    case C_ARG:
    case C_REGPARM:
    case C_FIELD:
    case C_EOS:
      SF_SET_DEBUG (def_symbol_in_progress);
      S_SET_SEGMENT (def_symbol_in_progress, absolute_section);
      break;

    case C_EXT:
    case C_WEAKEXT:
#ifdef TE_PE
    case C_NT_WEAK:
#endif
    case C_STAT:
    case C_LABEL:
      /* Valid but set somewhere else (s_comm, s_lcomm, colon) */
      break;

    case C_USTATIC:
    case C_EXTDEF:
    case C_ULABEL:
      as_warn (_("unexpected storage class %d"), S_GET_STORAGE_CLASS (def_symbol_in_progress));
      break;
    }				/* switch on storage class */

  /* Now that we have built a debug symbol, try to find if we should
     merge with an existing symbol or not.  If a symbol is C_EFCN or
     absolute_section or untagged SEG_DEBUG it never merges.  We also
     don't merge labels, which are in a different namespace, nor
     symbols which have not yet been defined since they are typically
     unique, nor do we merge tags with non-tags.  */

  /* Two cases for functions.  Either debug followed by definition or
     definition followed by debug.  For definition first, we will
     merge the debug symbol into the definition.  For debug first, the
     lineno entry MUST point to the definition function or else it
     will point off into space when crawl_symbols() merges the debug
     symbol into the real symbol.  Therefor, let's presume the debug
     symbol is a real function reference.  */

  /* FIXME-SOON If for some reason the definition label/symbol is
     never seen, this will probably leave an undefined symbol at link
     time.  */

  if (S_GET_STORAGE_CLASS (def_symbol_in_progress) == C_EFCN
      || S_GET_STORAGE_CLASS (def_symbol_in_progress) == C_LABEL
      || (S_GET_SEGMENT (def_symbol_in_progress) == SEG_DEBUG
	  && !SF_GET_TAG (def_symbol_in_progress))
      || S_GET_SEGMENT (def_symbol_in_progress) == absolute_section
      || def_symbol_in_progress->sy_value.X_op != O_constant
      || (symbolP = symbol_find_base (S_GET_NAME (def_symbol_in_progress), DO_NOT_STRIP)) == NULL
      || (SF_GET_TAG (def_symbol_in_progress) != SF_GET_TAG (symbolP)))
    {
      symbol_append (def_symbol_in_progress, symbol_lastP, &symbol_rootP,
		     &symbol_lastP);
    }
  else
    {
      /* This symbol already exists, merge the newly created symbol
	 into the old one.  This is not mandatory. The linker can
	 handle duplicate symbols correctly. But I guess that it save
	 a *lot* of space if the assembly file defines a lot of
	 symbols. [loic] */

      /* The debug entry (def_symbol_in_progress) is merged into the
	 previous definition.  */

      c_symbol_merge (def_symbol_in_progress, symbolP);
      /* FIXME-SOON Should *def_symbol_in_progress be free'd? xoxorich.  */
      def_symbol_in_progress = symbolP;

      if (SF_GET_FUNCTION (def_symbol_in_progress)
	  || SF_GET_TAG (def_symbol_in_progress)
	  || S_GET_STORAGE_CLASS (def_symbol_in_progress) == C_STAT)
	{
	  /* For functions, and tags, and static symbols, the symbol
	     *must* be where the debug symbol appears.  Move the
	     existing symbol to the current place.  */
	  /* If it already is at the end of the symbol list, do nothing */
	  if (def_symbol_in_progress != symbol_lastP)
	    {
	      symbol_remove (def_symbol_in_progress, &symbol_rootP,
			     &symbol_lastP);
	      symbol_append (def_symbol_in_progress, symbol_lastP,
			     &symbol_rootP, &symbol_lastP);
	    }			/* if not already in place */
	}			/* if function */
    }				/* normal or mergable */

  if (SF_GET_TAG (def_symbol_in_progress))
    {
      symbolS *oldtag;

      oldtag = symbol_find_base (S_GET_NAME (def_symbol_in_progress),
				 DO_NOT_STRIP);
      if (oldtag == NULL || ! SF_GET_TAG (oldtag))
	tag_insert (S_GET_NAME (def_symbol_in_progress),
		    def_symbol_in_progress);
    }

  if (SF_GET_FUNCTION (def_symbol_in_progress))
    {
      know (sizeof (def_symbol_in_progress) <= sizeof (long));
      function_lineoff
	= c_line_new (def_symbol_in_progress, 0, 0, &zero_address_frag);

      SF_SET_PROCESS (def_symbol_in_progress);

      if (symbolP == NULL)
	{
	  /* That is, if this is the first time we've seen the
	     function...  */
	  symbol_table_insert (def_symbol_in_progress);
	}			/* definition follows debug */
    }				/* Create the line number entry pointing to the function being defined */

  def_symbol_in_progress = NULL;
  demand_empty_rest_of_line ();
}

static void
obj_coff_dim (ignore)
     int ignore ATTRIBUTE_UNUSED;
{
  int dim_index;

  if (def_symbol_in_progress == NULL)
    {
      as_warn (_(".dim pseudo-op used outside of .def/.endef: ignored."));
      demand_empty_rest_of_line ();
      return;
    }				/* if not inside .def/.endef */

  S_SET_NUMBER_AUXILIARY (def_symbol_in_progress, 1);

  for (dim_index = 0; dim_index < DIMNUM; dim_index++)
    {
      SKIP_WHITESPACES ();
      SA_SET_SYM_DIMEN (def_symbol_in_progress, dim_index,
			get_absolute_expression ());

      switch (*input_line_pointer)
	{
	case ',':
	  input_line_pointer++;
	  break;

	default:
	  as_warn (_("badly formed .dim directive ignored"));
	  /* intentional fallthrough */
	case '\n':
	case ';':
	  dim_index = DIMNUM;
	  break;
	}
    }

  demand_empty_rest_of_line ();
}

static void
obj_coff_line (ignore)
     int ignore ATTRIBUTE_UNUSED;
{
  int this_base;
  const char *name;

  if (def_symbol_in_progress == NULL)
    {
      obj_coff_ln (0);
      return;
    }

  name = S_GET_NAME (def_symbol_in_progress);
  this_base = get_absolute_expression ();

  /* Only .bf symbols indicate the use of a new base line number; the
     line numbers associated with .ef, .bb, .eb are relative to the
     start of the containing function.  */
  if (!strcmp (".bf", name))
    {
#if 0 /* XXX Can we ever have line numbers going backwards?  */
      if (this_base > line_base)
#endif
	{
	  line_base = this_base;
	}

#ifndef NO_LISTING
      {
	extern int listing;
	if (listing)
	  {
	    listing_source_line ((unsigned int) line_base);
	  }
      }
#endif
    }

  S_SET_NUMBER_AUXILIARY (def_symbol_in_progress, 1);
  SA_SET_SYM_LNNO (def_symbol_in_progress, this_base);

  demand_empty_rest_of_line ();
}

static void
obj_coff_size (ignore)
     int ignore ATTRIBUTE_UNUSED;
{
  if (def_symbol_in_progress == NULL)
    {
      as_warn (_(".size pseudo-op used outside of .def/.endef ignored."));
      demand_empty_rest_of_line ();
      return;
    }				/* if not inside .def/.endef */

  S_SET_NUMBER_AUXILIARY (def_symbol_in_progress, 1);
  SA_SET_SYM_SIZE (def_symbol_in_progress, get_absolute_expression ());
  demand_empty_rest_of_line ();
}

static void
obj_coff_scl (ignore)
     int ignore ATTRIBUTE_UNUSED;
{
  if (def_symbol_in_progress == NULL)
    {
      as_warn (_(".scl pseudo-op used outside of .def/.endef ignored."));
      demand_empty_rest_of_line ();
      return;
    }				/* if not inside .def/.endef */

  S_SET_STORAGE_CLASS (def_symbol_in_progress, get_absolute_expression ());
  demand_empty_rest_of_line ();
}

static void
obj_coff_tag (ignore)
     int ignore ATTRIBUTE_UNUSED;
{
  char *symbol_name;
  char name_end;

  if (def_symbol_in_progress == NULL)
    {
      as_warn (_(".tag pseudo-op used outside of .def/.endef ignored."));
      demand_empty_rest_of_line ();
      return;
    }

  S_SET_NUMBER_AUXILIARY (def_symbol_in_progress, 1);
  symbol_name = input_line_pointer;
  name_end = get_symbol_end ();
#ifdef tc_canonicalize_symbol_name
  symbol_name = tc_canonicalize_symbol_name (symbol_name);
#endif

  /* Assume that the symbol referred to by .tag is always defined.
     This was a bad assumption.  I've added find_or_make. xoxorich.  */
  SA_SET_SYM_TAGNDX (def_symbol_in_progress,
		     (long) tag_find_or_make (symbol_name));
  if (SA_GET_SYM_TAGNDX (def_symbol_in_progress) == 0L)
    {
      as_warn (_("tag not found for .tag %s"), symbol_name);
    }				/* not defined */

  SF_SET_TAGGED (def_symbol_in_progress);
  *input_line_pointer = name_end;

  demand_empty_rest_of_line ();
}

static void
obj_coff_type (ignore)
     int ignore ATTRIBUTE_UNUSED;
{
  if (def_symbol_in_progress == NULL)
    {
      as_warn (_(".type pseudo-op used outside of .def/.endef ignored."));
      demand_empty_rest_of_line ();
      return;
    }				/* if not inside .def/.endef */

  S_SET_DATA_TYPE (def_symbol_in_progress, get_absolute_expression ());

  if (ISFCN (S_GET_DATA_TYPE (def_symbol_in_progress)) &&
      S_GET_STORAGE_CLASS (def_symbol_in_progress) != C_TPDEF)
    {
      SF_SET_FUNCTION (def_symbol_in_progress);
    }				/* is a function */

  demand_empty_rest_of_line ();
}

static void
obj_coff_val (ignore)
     int ignore ATTRIBUTE_UNUSED;
{
  if (def_symbol_in_progress == NULL)
    {
      as_warn (_(".val pseudo-op used outside of .def/.endef ignored."));
      demand_empty_rest_of_line ();
      return;
    }				/* if not inside .def/.endef */

  if (is_name_beginner (*input_line_pointer))
    {
      char *symbol_name = input_line_pointer;
      char name_end = get_symbol_end ();

#ifdef tc_canonicalize_symbol_name
  symbol_name = tc_canonicalize_symbol_name (symbol_name);
#endif

      if (!strcmp (symbol_name, "."))
	{
	  def_symbol_in_progress->sy_frag = frag_now;
	  S_SET_VALUE (def_symbol_in_progress, (valueT) frag_now_fix ());
	  /* If the .val is != from the .def (e.g. statics) */
	}
      else if (strcmp (S_GET_NAME (def_symbol_in_progress), symbol_name))
	{
	  def_symbol_in_progress->sy_value.X_op = O_symbol;
	  def_symbol_in_progress->sy_value.X_add_symbol =
	    symbol_find_or_make (symbol_name);
	  def_symbol_in_progress->sy_value.X_op_symbol = NULL;
	  def_symbol_in_progress->sy_value.X_add_number = 0;

	  /* If the segment is undefined when the forward reference is
	     resolved, then copy the segment id from the forward
	     symbol.  */
	  SF_SET_GET_SEGMENT (def_symbol_in_progress);

	  /* FIXME: gcc can generate address expressions here in
	     unusual cases (search for "obscure" in sdbout.c).  We
	     just ignore the offset here, thus generating incorrect
	     debugging information.  We ignore the rest of the line
	     just below.  */
	}
      /* Otherwise, it is the name of a non debug symbol and
	 its value will be calculated later.  */
      *input_line_pointer = name_end;

      /* FIXME: this is to avoid an error message in the
	 FIXME case mentioned just above.  */
      while (! is_end_of_line[(unsigned char) *input_line_pointer])
	++input_line_pointer;
    }
  else
    {
      S_SET_VALUE (def_symbol_in_progress,
		   (valueT) get_absolute_expression ());
    }				/* if symbol based */

  demand_empty_rest_of_line ();
}

#ifdef TE_PE

/* Handle the .linkonce pseudo-op.  This is parsed by s_linkonce in
   read.c, which then calls this object file format specific routine.  */

void
obj_coff_pe_handle_link_once (type)
     enum linkonce_type type;
{
  seg_info (now_seg)->scnhdr.s_flags |= IMAGE_SCN_LNK_COMDAT;

  /* We store the type in the seg_info structure, and use it to set up
     the auxiliary entry for the section symbol in c_section_symbol.  */
  seg_info (now_seg)->linkonce = type;
}

#endif /* TE_PE */

void
coff_obj_read_begin_hook ()
{
  /* These had better be the same.  Usually 18 bytes.  */
#ifndef BFD_HEADERS
  know (sizeof (SYMENT) == sizeof (AUXENT));
  know (SYMESZ == AUXESZ);
#endif
  tag_init ();
}

/* This function runs through the symbol table and puts all the
   externals onto another chain */

/* The chain of globals.  */
symbolS *symbol_globalP;
symbolS *symbol_global_lastP;

/* The chain of externals */
symbolS *symbol_externP;
symbolS *symbol_extern_lastP;

stack *block_stack;
symbolS *last_functionP;
static symbolS *last_bfP;
symbolS *last_tagP;

static unsigned int
yank_symbols ()
{
  symbolS *symbolP;
  unsigned int symbol_number = 0;
  unsigned int last_file_symno = 0;

  struct filename_list *filename_list_scan = filename_list_head;

  for (symbolP = symbol_rootP;
       symbolP;
       symbolP = symbolP ? symbol_next (symbolP) : symbol_rootP)
    {
      if (symbolP->sy_mri_common)
	{
	  if (S_GET_STORAGE_CLASS (symbolP) == C_EXT
#ifdef TE_PE
	      || S_GET_STORAGE_CLASS (symbolP) == C_NT_WEAK
#endif
	      || S_GET_STORAGE_CLASS (symbolP) == C_WEAKEXT)
	    as_bad (_("%s: global symbols not supported in common sections"),
		    S_GET_NAME (symbolP));
	  symbol_remove (symbolP, &symbol_rootP, &symbol_lastP);
	  continue;
	}

      if (!SF_GET_DEBUG (symbolP))
	{
	  /* Debug symbols do not need all this rubbish */
	  symbolS *real_symbolP;

	  /* L* and C_EFCN symbols never merge.  */
	  if (!SF_GET_LOCAL (symbolP)
	      && !SF_GET_STATICS (symbolP)
	      && S_GET_STORAGE_CLASS (symbolP) != C_LABEL
	      && symbolP->sy_value.X_op == O_constant
	      && (real_symbolP = symbol_find_base (S_GET_NAME (symbolP), DO_NOT_STRIP))
	      && real_symbolP != symbolP)
	    {
	      /* FIXME-SOON: where do dups come from?
		 Maybe tag references before definitions? xoxorich.  */
	      /* Move the debug data from the debug symbol to the
		 real symbol. Do NOT do the oposite (i.e. move from
		 real symbol to debug symbol and remove real symbol from the
		 list.) Because some pointers refer to the real symbol
		 whereas no pointers refer to the debug symbol.  */
	      c_symbol_merge (symbolP, real_symbolP);
	      /* Replace the current symbol by the real one */
	      /* The symbols will never be the last or the first
		 because : 1st symbol is .file and 3 last symbols are
		 .text, .data, .bss */
	      symbol_remove (real_symbolP, &symbol_rootP, &symbol_lastP);
	      symbol_insert (real_symbolP, symbolP, &symbol_rootP, &symbol_lastP);
	      symbol_remove (symbolP, &symbol_rootP, &symbol_lastP);
	      symbolP = real_symbolP;
	    }			/* if not local but dup'd */

	  if (flag_readonly_data_in_text && (S_GET_SEGMENT (symbolP) == SEG_E1))
	    {
	      S_SET_SEGMENT (symbolP, SEG_E0);
	    }			/* push data into text */

	  resolve_symbol_value (symbolP, 1);

	  if (S_GET_STORAGE_CLASS (symbolP) == C_NULL)
	    {
	      if (!S_IS_DEFINED (symbolP) && !SF_GET_LOCAL (symbolP))
		{
		  S_SET_EXTERNAL (symbolP);
		}
	      else if (S_GET_SEGMENT (symbolP) == SEG_E0)
		{
		  S_SET_STORAGE_CLASS (symbolP, C_LABEL);
		}
	      else
		{
		  S_SET_STORAGE_CLASS (symbolP, C_STAT);
		}
	    }

	  /* Mainly to speed up if not -g */
	  if (SF_GET_PROCESS (symbolP))
	    {
	      /* Handle the nested blocks auxiliary info.  */
	      if (S_GET_STORAGE_CLASS (symbolP) == C_BLOCK)
		{
		  if (!strcmp (S_GET_NAME (symbolP), ".bb"))
		    stack_push (block_stack, (char *) &symbolP);
		  else
		    {		/* .eb */
		      register symbolS *begin_symbolP;
		      begin_symbolP = *(symbolS **) stack_pop (block_stack);
		      if (begin_symbolP == (symbolS *) 0)
			as_warn (_("mismatched .eb"));
		      else
			SA_SET_SYM_ENDNDX (begin_symbolP, symbol_number + 2);
		    }
		}
	      /* If we are able to identify the type of a function, and we
	       are out of a function (last_functionP == 0) then, the
	       function symbol will be associated with an auxiliary
	       entry.  */
	      if (last_functionP == (symbolS *) 0 &&
		  SF_GET_FUNCTION (symbolP))
		{
		  last_functionP = symbolP;

		  if (S_GET_NUMBER_AUXILIARY (symbolP) < 1)
		    {
		      S_SET_NUMBER_AUXILIARY (symbolP, 1);
		    }		/* make it at least 1 */

		  /* Clobber possible stale .dim information.  */
#if 0
		  /* Iffed out by steve - this fries the lnnoptr info too */
		  bzero (symbolP->sy_symbol.ost_auxent[0].x_sym.x_fcnary.x_ary.x_dimen,
			 sizeof (symbolP->sy_symbol.ost_auxent[0].x_sym.x_fcnary.x_ary.x_dimen));
#endif
		}
	      if (S_GET_STORAGE_CLASS (symbolP) == C_FCN)
		{
		  if (strcmp (S_GET_NAME (symbolP), ".bf") == 0)
		    {
		      if (last_bfP != NULL)
			SA_SET_SYM_ENDNDX (last_bfP, symbol_number);
		      last_bfP = symbolP;
		    }
		}
	      else if (S_GET_STORAGE_CLASS (symbolP) == C_EFCN)
		{
		  /* I don't even know if this is needed for sdb. But
		     the standard assembler generates it, so...  */
		  if (last_functionP == (symbolS *) 0)
		    as_fatal (_("C_EFCN symbol out of scope"));
		  SA_SET_SYM_FSIZE (last_functionP,
				    (long) (S_GET_VALUE (symbolP) -
					    S_GET_VALUE (last_functionP)));
		  SA_SET_SYM_ENDNDX (last_functionP, symbol_number);
		 last_functionP = (symbolS *) 0;
		}
	    }
	}
      else if (SF_GET_TAG (symbolP))
	{
	  /* First descriptor of a structure must point to
	       the first slot after the structure description.  */
	  last_tagP = symbolP;

	}
      else if (S_GET_STORAGE_CLASS (symbolP) == C_EOS)
	{
	  /* +2 take in account the current symbol */
	  SA_SET_SYM_ENDNDX (last_tagP, symbol_number + 2);
	}
      else if (S_GET_STORAGE_CLASS (symbolP) == C_FILE)
	{
	  /* If the filename was too long to fit in the
	     auxent, put it in the string table */
	  if (SA_GET_FILE_FNAME_ZEROS (symbolP) == 0
	      && SA_GET_FILE_FNAME_OFFSET (symbolP) != 0)
	    {
	      SA_SET_FILE_FNAME_OFFSET (symbolP, string_byte_count);
	      string_byte_count += strlen (filename_list_scan->filename) + 1;
	      filename_list_scan = filename_list_scan->next;
	    }
	  if (S_GET_VALUE (symbolP))
	    {
	      S_SET_VALUE (symbolP, last_file_symno);
	      last_file_symno = symbol_number;
	    }			/* no one points at the first .file symbol */
	}			/* if debug or tag or eos or file */

#ifdef tc_frob_coff_symbol
      tc_frob_coff_symbol (symbolP);
#endif

      /* We must put the external symbols apart. The loader
	 does not bomb if we do not. But the references in
	 the endndx field for a .bb symbol are not corrected
	 if an external symbol is removed between .bb and .be.
	 I.e in the following case :
	 [20] .bb endndx = 22
	 [21] foo external
	 [22] .be
	 ld will move the symbol 21 to the end of the list but
	 endndx will still be 22 instead of 21.  */

      if (SF_GET_LOCAL (symbolP))
	{
	  /* remove C_EFCN and LOCAL (L...) symbols */
	  /* next pointer remains valid */
	  symbol_remove (symbolP, &symbol_rootP, &symbol_lastP);

	}
      else if (symbolP->sy_value.X_op == O_symbol
	       && (! S_IS_DEFINED (symbolP) || S_IS_COMMON (symbolP)))
	{
	  /* Skip symbols which were equated to undefined or common
	     symbols.  */
	  symbol_remove (symbolP, &symbol_rootP, &symbol_lastP);
	}
      else if (!S_IS_DEFINED (symbolP)
	       && !S_IS_DEBUG (symbolP)
	       && !SF_GET_STATICS (symbolP)
	       && (S_GET_STORAGE_CLASS (symbolP) == C_EXT
#ifdef TE_PE
		   || S_GET_STORAGE_CLASS (symbolP) == C_NT_WEAK
#endif
		   || S_GET_STORAGE_CLASS (symbolP) == C_WEAKEXT))
	{
	  /* if external, Remove from the list */
	  symbolS *hold = symbol_previous (symbolP);

	  symbol_remove (symbolP, &symbol_rootP, &symbol_lastP);
	  symbol_clear_list_pointers (symbolP);
	  symbol_append (symbolP, symbol_extern_lastP, &symbol_externP, &symbol_extern_lastP);
	  symbolP = hold;
	}
      else if (! S_IS_DEBUG (symbolP)
	       && ! SF_GET_STATICS (symbolP)
	       && ! SF_GET_FUNCTION (symbolP)
	       && (S_GET_STORAGE_CLASS (symbolP) == C_EXT
#ifdef TE_PE
		   || S_GET_STORAGE_CLASS (symbolP) == C_NT_WEAK
#endif
		   || S_GET_STORAGE_CLASS (symbolP) == C_NT_WEAK))
	{
	  symbolS *hold = symbol_previous (symbolP);

	  /* The O'Reilly COFF book says that defined global symbols
             come at the end of the symbol table, just before
             undefined global symbols.  */

	  symbol_remove (symbolP, &symbol_rootP, &symbol_lastP);
	  symbol_clear_list_pointers (symbolP);
	  symbol_append (symbolP, symbol_global_lastP, &symbol_globalP,
			 &symbol_global_lastP);
	  symbolP = hold;
	}
      else
	{
	  if (SF_GET_STRING (symbolP))
	    {
	      symbolP->sy_name_offset = string_byte_count;
	      string_byte_count += strlen (S_GET_NAME (symbolP)) + 1;
	    }
	  else
	    {
	      symbolP->sy_name_offset = 0;
	    }			/* fix "long" names */

	  symbolP->sy_number = symbol_number;
	  symbol_number += 1 + S_GET_NUMBER_AUXILIARY (symbolP);
	}			/* if local symbol */
    }				/* traverse the symbol list */
  return symbol_number;

}

static unsigned int
glue_symbols (head, tail)
     symbolS **head;
     symbolS **tail;
{
  unsigned int symbol_number = 0;

  while (*head != NULL)
    {
      symbolS *tmp = *head;

      /* append */
      symbol_remove (tmp, head, tail);
      symbol_append (tmp, symbol_lastP, &symbol_rootP, &symbol_lastP);

      /* and process */
      if (SF_GET_STRING (tmp))
	{
	  tmp->sy_name_offset = string_byte_count;
	  string_byte_count += strlen (S_GET_NAME (tmp)) + 1;
	}
      else
	{
	  tmp->sy_name_offset = 0;
	}			/* fix "long" names */

      tmp->sy_number = symbol_number;
      symbol_number += 1 + S_GET_NUMBER_AUXILIARY (tmp);
    }				/* append the entire extern chain */

  return symbol_number;
}

static unsigned int
tie_tags ()
{
  unsigned int symbol_number = 0;
  symbolS *symbolP;

  for (symbolP = symbol_rootP; symbolP; symbolP = symbol_next (symbolP))
    {
      symbolP->sy_number = symbol_number;

      if (SF_GET_TAGGED (symbolP))
	{
	  SA_SET_SYM_TAGNDX
	    (symbolP,
	     ((symbolS *) SA_GET_SYM_TAGNDX (symbolP))->sy_number);
	}

      symbol_number += 1 + S_GET_NUMBER_AUXILIARY (symbolP);
    }

  return symbol_number;
}

static void
crawl_symbols (h, abfd)
     object_headers *h;
     bfd *abfd ATTRIBUTE_UNUSED;
{
  unsigned int i;

  /* Initialize the stack used to keep track of the matching .bb .be */

  block_stack = stack_init (512, sizeof (symbolS *));

  /* The symbol list should be ordered according to the following sequence
   * order :
   * . .file symbol
   * . debug entries for functions
   * . fake symbols for the sections, including .text .data and .bss
   * . defined symbols
   * . undefined symbols
   * But this is not mandatory. The only important point is to put the
   * undefined symbols at the end of the list.
   */

  /* Is there a .file symbol ? If not insert one at the beginning.  */
  if (symbol_rootP == NULL
      || S_GET_STORAGE_CLASS (symbol_rootP) != C_FILE)
    {
      c_dot_file_symbol ("fake");
    }

  /*
   * Build up static symbols for the sections, they are filled in later
   */

  for (i = SEG_E0; i < SEG_LAST; i++)
    if (segment_info[i].scnhdr.s_name[0])
      segment_info[i].dot = c_section_symbol (segment_info[i].name,
					      i - SEG_E0 + 1);

  /* Take all the externals out and put them into another chain */
  H_SET_SYMBOL_TABLE_SIZE (h, yank_symbols ());
  /* Take the externals and glue them onto the end.*/
  H_SET_SYMBOL_TABLE_SIZE (h,
			   (H_GET_SYMBOL_COUNT (h)
			    + glue_symbols (&symbol_globalP,
					    &symbol_global_lastP)
			    + glue_symbols (&symbol_externP,
					    &symbol_extern_lastP)));

  H_SET_SYMBOL_TABLE_SIZE (h, tie_tags ());
  know (symbol_globalP == NULL);
  know (symbol_global_lastP == NULL);
  know (symbol_externP == NULL);
  know (symbol_extern_lastP == NULL);
}

/*
 * Find strings by crawling along symbol table chain.
 */

void
w_strings (where)
     char *where;
{
  symbolS *symbolP;
  struct filename_list *filename_list_scan = filename_list_head;

  /* Gotta do md_ byte-ordering stuff for string_byte_count first - KWK */
  md_number_to_chars (where, (valueT) string_byte_count, 4);
  where += 4;

#ifdef COFF_LONG_SECTION_NAMES
  /* Support long section names as found in PE.  This code must
     coordinate with that in coff_header_append and write_object_file.  */
  {
    unsigned int i;

    for (i = SEG_E0; i < SEG_LAST; i++)
      {
	if (segment_info[i].scnhdr.s_name[0]
	    && strlen (segment_info[i].name) > SCNNMLEN)
	  {
	    unsigned int size;

	    size = strlen (segment_info[i].name) + 1;
	    memcpy (where, segment_info[i].name, size);
	    where += size;
	  }
      }
  }
#endif /* COFF_LONG_SECTION_NAMES */

  for (symbolP = symbol_rootP;
       symbolP;
       symbolP = symbol_next (symbolP))
    {
      unsigned int size;

      if (SF_GET_STRING (symbolP))
	{
	  size = strlen (S_GET_NAME (symbolP)) + 1;
	  memcpy (where, S_GET_NAME (symbolP), size);
	  where += size;
	}
      if (S_GET_STORAGE_CLASS (symbolP) == C_FILE
	  && SA_GET_FILE_FNAME_ZEROS (symbolP) == 0
	  && SA_GET_FILE_FNAME_OFFSET (symbolP) != 0)
	{
	  size = strlen (filename_list_scan->filename) + 1;
	  memcpy (where, filename_list_scan->filename, size);
	  filename_list_scan = filename_list_scan ->next;
	  where += size;
	}
    }
}

static void
do_linenos_for (abfd, h, file_cursor)
     bfd * abfd;
     object_headers * h;
     unsigned long *file_cursor;
{
  unsigned int idx;
  unsigned long start = *file_cursor;

  for (idx = SEG_E0; idx < SEG_LAST; idx++)
    {
      segment_info_type *s = segment_info + idx;

      if (s->scnhdr.s_nlnno != 0)
	{
	  struct lineno_list *line_ptr;

	  struct external_lineno *buffer =
	  (struct external_lineno *) xmalloc (s->scnhdr.s_nlnno * LINESZ);

	  struct external_lineno *dst = buffer;

	  /* Run through the table we've built and turn it into its external
	 form, take this chance to remove duplicates */

	  for (line_ptr = s->lineno_list_head;
	       line_ptr != (struct lineno_list *) NULL;
	       line_ptr = line_ptr->next)
	    {

	      if (line_ptr->line.l_lnno == 0)
		{
		  /* Turn a pointer to a symbol into the symbols' index */
		  line_ptr->line.l_addr.l_symndx =
		    ((symbolS *) line_ptr->line.l_addr.l_symndx)->sy_number;
		}
	      else
		{
		  line_ptr->line.l_addr.l_paddr += ((struct frag *) (line_ptr->frag))->fr_address;
		}

	      (void) bfd_coff_swap_lineno_out (abfd, &(line_ptr->line), dst);
	      dst++;

	    }

	  s->scnhdr.s_lnnoptr = *file_cursor;

	  bfd_write (buffer, 1, s->scnhdr.s_nlnno * LINESZ, abfd);
	  free (buffer);

	  *file_cursor += s->scnhdr.s_nlnno * LINESZ;
	}
    }
  H_SET_LINENO_SIZE (h, *file_cursor - start);
}

/* Now we run through the list of frag chains in a segment and
   make all the subsegment frags appear at the end of the
   list, as if the seg 0 was extra long */

static void
remove_subsegs ()
{
  unsigned int i;

  for (i = SEG_E0; i < SEG_UNKNOWN; i++)
    {
      frchainS *head = segment_info[i].frchainP;
      fragS dummy;
      fragS *prev_frag = &dummy;

      while (head && head->frch_seg == i)
	{
	  prev_frag->fr_next = head->frch_root;
	  prev_frag = head->frch_last;
	  head = head->frch_next;
	}
      prev_frag->fr_next = 0;
    }
}

unsigned long machine;
int coff_flags;
extern void
write_object_file ()
{
  int i;
  const char *name;
  struct frchain *frchain_ptr;

  object_headers headers;
  unsigned long file_cursor;
  bfd *abfd;
  unsigned int addr;
  abfd = bfd_openw (out_file_name, TARGET_FORMAT);

  if (abfd == 0)
    {
      as_perror (_("FATAL: Can't create %s"), out_file_name);
      exit (EXIT_FAILURE);
    }
  bfd_set_format (abfd, bfd_object);
  bfd_set_arch_mach (abfd, BFD_ARCH, machine);

  string_byte_count = 4;

  for (frchain_ptr = frchain_root;
       frchain_ptr != (struct frchain *) NULL;
       frchain_ptr = frchain_ptr->frch_next)
    {
      /* Run through all the sub-segments and align them up.  Also
	 close any open frags.  We tack a .fill onto the end of the
	 frag chain so that any .align's size can be worked by looking
	 at the next frag.  */

      subseg_set (frchain_ptr->frch_seg, frchain_ptr->frch_subseg);

#ifndef SUB_SEGMENT_ALIGN
#define SUB_SEGMENT_ALIGN(SEG) 1
#endif
#ifdef md_do_align
      md_do_align (SUB_SEGMENT_ALIGN (now_seg), (char *) NULL, 0, 0,
		   alignment_done);
#endif
      if (subseg_text_p (now_seg))
	frag_align_code (SUB_SEGMENT_ALIGN (now_seg), 0);
      else
	frag_align (SUB_SEGMENT_ALIGN (now_seg), 0, 0);

#ifdef md_do_align
    alignment_done:
#endif

      frag_wane (frag_now);
      frag_now->fr_fix = 0;
      know (frag_now->fr_next == NULL);
    }

  remove_subsegs ();

  for (i = SEG_E0; i < SEG_UNKNOWN; i++)
    {
      relax_segment (segment_info[i].frchainP->frch_root, i);
    }

  H_SET_NUMBER_OF_SECTIONS (&headers, 0);

  /* Find out how big the sections are, and set the addresses.  */
  addr = 0;
  for (i = SEG_E0; i < SEG_UNKNOWN; i++)
    {
      long size;

      segment_info[i].scnhdr.s_paddr = addr;
      segment_info[i].scnhdr.s_vaddr = addr;

      if (segment_info[i].scnhdr.s_name[0])
	{
	  H_SET_NUMBER_OF_SECTIONS (&headers,
				    H_GET_NUMBER_OF_SECTIONS (&headers) + 1);

#ifdef COFF_LONG_SECTION_NAMES
	  /* Support long section names as found in PE.  This code
	     must coordinate with that in coff_header_append and
	     w_strings.  */
	  {
	    unsigned int len;

	    len = strlen (segment_info[i].name);
	    if (len > SCNNMLEN)
	      string_byte_count += len + 1;
	  }
#endif /* COFF_LONG_SECTION_NAMES */
	}

      size = size_section (abfd, (unsigned int) i);
      addr += size;

      /* I think the section alignment is only used on the i960; the
	 i960 needs it, and it should do no harm on other targets.  */
#ifdef ALIGNMENT_IN_S_FLAGS
      segment_info[i].scnhdr.s_flags |= (section_alignment[i] & 0xF) << 8;
#else
      segment_info[i].scnhdr.s_align = 1 << section_alignment[i];
#endif

      if (i == SEG_E0)
	H_SET_TEXT_SIZE (&headers, size);
      else if (i == SEG_E1)
	H_SET_DATA_SIZE (&headers, size);
      else if (i == SEG_E2)
	H_SET_BSS_SIZE (&headers, size);
    }

  /* Turn the gas native symbol table shape into a coff symbol table */
  crawl_symbols (&headers, abfd);

  if (string_byte_count == 4)
    string_byte_count = 0;

  H_SET_STRING_SIZE (&headers, string_byte_count);

#ifdef tc_frob_file
  tc_frob_file ();
#endif

  for (i = SEG_E0; i < SEG_UNKNOWN; i++)
    {
      fixup_mdeps (segment_info[i].frchainP->frch_root, &headers, i);
      fixup_segment (&segment_info[i], i);
    }

  /* Look for ".stab" segments and fill in their initial symbols
     correctly.  */
  for (i = SEG_E0; i < SEG_UNKNOWN; i++)
    {
      name = segment_info[i].name;

      if (name != NULL
	  && strncmp (".stab", name, 5) == 0
	  && strncmp (".stabstr", name, 8) != 0)
	adjust_stab_section (abfd, i);
    }

  file_cursor = H_GET_TEXT_FILE_OFFSET (&headers);

  bfd_seek (abfd, (file_ptr) file_cursor, 0);

  /* Plant the data */

  fill_section (abfd, &headers, &file_cursor);

  do_relocs_for (abfd, &headers, &file_cursor);

  do_linenos_for (abfd, &headers, &file_cursor);

  H_SET_FILE_MAGIC_NUMBER (&headers, COFF_MAGIC);
#ifndef OBJ_COFF_OMIT_TIMESTAMP
  H_SET_TIME_STAMP (&headers, (long)time((time_t *)0));
#else
  H_SET_TIME_STAMP (&headers, 0);
#endif
#ifdef TC_COFF_SET_MACHINE
  TC_COFF_SET_MACHINE (&headers);
#endif

#ifndef COFF_FLAGS
#define COFF_FLAGS 0
#endif

#ifdef KEEP_RELOC_INFO
  H_SET_FLAGS (&headers, ((H_GET_LINENO_SIZE(&headers) ? 0 : F_LNNO) |
			  COFF_FLAGS | coff_flags));
#else
  H_SET_FLAGS (&headers, ((H_GET_LINENO_SIZE(&headers)     ? 0 : F_LNNO)   |
			  (H_GET_RELOCATION_SIZE(&headers) ? 0 : F_RELFLG) |
			  COFF_FLAGS | coff_flags));
#endif

  {
    unsigned int symtable_size = H_GET_SYMBOL_TABLE_SIZE (&headers);
    char *buffer1 = xmalloc (symtable_size + string_byte_count + 1);

    H_SET_SYMBOL_TABLE_POINTER (&headers, bfd_tell (abfd));
    w_symbols (abfd, buffer1, symbol_rootP);
    if (string_byte_count > 0)
      w_strings (buffer1 + symtable_size);
    bfd_write (buffer1, 1, symtable_size + string_byte_count, abfd);
    free (buffer1);
  }

  coff_header_append (abfd, &headers);
#if 0
  /* Recent changes to write need this, but where it should
     go is up to Ken..  */
  if (bfd_close_all_done (abfd) == false)
    as_fatal (_("Can't close %s: %s"), out_file_name,
	      bfd_errmsg (bfd_get_error ()));
#else
  {
    extern bfd *stdoutput;
    stdoutput = abfd;
  }
#endif

}

/* Add a new segment.  This is called from subseg_new via the
   obj_new_segment macro.  */

segT
obj_coff_add_segment (name)
     const char *name;
{
  unsigned int i;

#ifndef COFF_LONG_SECTION_NAMES
  char buf[SCNNMLEN + 1];

  strncpy (buf, name, SCNNMLEN);
  buf[SCNNMLEN] = '\0';
  name = buf;
#endif

  for (i = SEG_E0; i < SEG_LAST && segment_info[i].scnhdr.s_name[0]; i++)
    if (strcmp (name, segment_info[i].name) == 0)
      return (segT) i;

  if (i == SEG_LAST)
    {
      as_bad (_("Too many new sections; can't add \"%s\""), name);
      return now_seg;
    }

  /* Add a new section.  */
  strncpy (segment_info[i].scnhdr.s_name, name,
	   sizeof (segment_info[i].scnhdr.s_name));
  segment_info[i].scnhdr.s_flags = STYP_REG;
  segment_info[i].name = xstrdup (name);

  return (segT) i;
}

/*
 * implement the .section pseudo op:
 *	.section name {, "flags"}
 *                ^         ^
 *                |         +--- optional flags: 'b' for bss
 *                |                              'i' for info
 *                +-- section name               'l' for lib
 *                                               'n' for noload
 *                                               'o' for over
 *                                               'w' for data
 *						 'd' (apparently m88k for data)
 *                                               'x' for text
 *						 'r' for read-only data
 * But if the argument is not a quoted string, treat it as a
 * subsegment number.
 */

void
obj_coff_section (ignore)
     int ignore ATTRIBUTE_UNUSED;
{
  /* Strip out the section name */
  char *section_name, *name;
  char c;
  unsigned int exp;
  long flags;

  if (flag_mri)
    {
      char type;

      s_mri_sect (&type);
      flags = 0;
      if (type == 'C')
	flags = STYP_TEXT;
      else if (type == 'D')
	flags = STYP_DATA;
      segment_info[now_seg].scnhdr.s_flags |= flags;

      return;
    }

  section_name = input_line_pointer;
  c = get_symbol_end ();

  name = xmalloc (input_line_pointer - section_name + 1);
  strcpy (name, section_name);

  *input_line_pointer = c;

  exp = 0;
  flags = 0;

  SKIP_WHITESPACE ();
  if (*input_line_pointer == ',')
    {
      ++input_line_pointer;
      SKIP_WHITESPACE ();

      if (*input_line_pointer != '"')
	exp = get_absolute_expression ();
      else
	{
	  ++input_line_pointer;
	  while (*input_line_pointer != '"'
		 && ! is_end_of_line[(unsigned char) *input_line_pointer])
	    {
	      switch (*input_line_pointer)
		{
		case 'b': flags |= STYP_BSS;    break;
		case 'i': flags |= STYP_INFO;   break;
		case 'l': flags |= STYP_LIB;    break;
		case 'n': flags |= STYP_NOLOAD; break;
		case 'o': flags |= STYP_OVER;   break;
		case 'd':
		case 'w': flags |= STYP_DATA;   break;
		case 'x': flags |= STYP_TEXT;   break;
		case 'r': flags |= STYP_LIT;	break;
		default:
		  as_warn(_("unknown section attribute '%c'"),
			  *input_line_pointer);
		  break;
		}
	      ++input_line_pointer;
	    }
	  if (*input_line_pointer == '"')
	    ++input_line_pointer;
	}
    }

  subseg_new (name, (subsegT) exp);

  segment_info[now_seg].scnhdr.s_flags |= flags;

  demand_empty_rest_of_line ();
}

static void
obj_coff_text (ignore)
     int ignore ATTRIBUTE_UNUSED;
{
  subseg_new (".text", get_absolute_expression ());
}

static void
obj_coff_data (ignore)
     int ignore ATTRIBUTE_UNUSED;
{
  if (flag_readonly_data_in_text)
    subseg_new (".text", get_absolute_expression () + 1000);
  else
    subseg_new (".data", get_absolute_expression ());
}

static void
obj_coff_ident (ignore)
     int ignore ATTRIBUTE_UNUSED;
{
  segT current_seg = now_seg;		/* save current seg	*/
  subsegT current_subseg = now_subseg;
  subseg_new (".comment", 0);		/* .comment seg		*/
  stringer (1);				/* read string		*/
  subseg_set (current_seg, current_subseg);	/* restore current seg	*/
}

void
c_symbol_merge (debug, normal)
     symbolS *debug;
     symbolS *normal;
{
  S_SET_DATA_TYPE (normal, S_GET_DATA_TYPE (debug));
  S_SET_STORAGE_CLASS (normal, S_GET_STORAGE_CLASS (debug));

  if (S_GET_NUMBER_AUXILIARY (debug) > S_GET_NUMBER_AUXILIARY (normal))
    {
      S_SET_NUMBER_AUXILIARY (normal, S_GET_NUMBER_AUXILIARY (debug));
    }				/* take the most we have */

  if (S_GET_NUMBER_AUXILIARY (debug) > 0)
    {
      memcpy ((char *) &normal->sy_symbol.ost_auxent[0],
	      (char *) &debug->sy_symbol.ost_auxent[0],
	      (unsigned int) (S_GET_NUMBER_AUXILIARY (debug) * AUXESZ));
    }				/* Move all the auxiliary information */

  /* Move the debug flags.  */
  SF_SET_DEBUG_FIELD (normal, SF_GET_DEBUG_FIELD (debug));
}				/* c_symbol_merge() */

static int
c_line_new (symbol, paddr, line_number, frag)
     symbolS * symbol;
     long paddr;
     int line_number;
     fragS * frag;
{
  struct lineno_list *new_line =
  (struct lineno_list *) xmalloc (sizeof (struct lineno_list));

  segment_info_type *s = segment_info + now_seg;
  new_line->line.l_lnno = line_number;

  if (line_number == 0)
    {
      last_line_symbol = symbol;
      new_line->line.l_addr.l_symndx = (long) symbol;
    }
  else
    {
      new_line->line.l_addr.l_paddr = paddr;
    }

  new_line->frag = (char *) frag;
  new_line->next = (struct lineno_list *) NULL;

  if (s->lineno_list_head == (struct lineno_list *) NULL)
    {
      s->lineno_list_head = new_line;
    }
  else
    {
      s->lineno_list_tail->next = new_line;
    }
  s->lineno_list_tail = new_line;
  return LINESZ * s->scnhdr.s_nlnno++;
}

void
c_dot_file_symbol (filename)
     char *filename;
{
  symbolS *symbolP;

  symbolP = symbol_new (".file",
			SEG_DEBUG,
			0,
			&zero_address_frag);

  S_SET_STORAGE_CLASS (symbolP, C_FILE);
  S_SET_NUMBER_AUXILIARY (symbolP, 1);

  if (strlen (filename) > FILNMLEN)
    {
      /* Filename is too long to fit into an auxent,
	 we stick it into the string table instead.  We keep
	 a linked list of the filenames we find so we can emit
	 them later.*/
      struct filename_list *f = ((struct filename_list *)
				 xmalloc (sizeof (struct filename_list)));

      f->filename = filename;
      f->next = 0;

      SA_SET_FILE_FNAME_ZEROS (symbolP, 0);
      SA_SET_FILE_FNAME_OFFSET (symbolP, 1);

      if (filename_list_tail)
	filename_list_tail->next = f;
      else
	filename_list_head = f;
      filename_list_tail = f;
    }
  else
    {
      SA_SET_FILE_FNAME (symbolP, filename);
    }
#ifndef NO_LISTING
  {
    extern int listing;
    if (listing)
      {
	listing_source_file (filename);
      }

  }

#endif
  SF_SET_DEBUG (symbolP);
  S_SET_VALUE (symbolP, (valueT) previous_file_symbol);

  previous_file_symbol = symbolP;

  /* Make sure that the symbol is first on the symbol chain */
  if (symbol_rootP != symbolP)
    {
      symbol_remove (symbolP, &symbol_rootP, &symbol_lastP);
      symbol_insert (symbolP, symbol_rootP, &symbol_rootP, &symbol_lastP);
    }
}				/* c_dot_file_symbol() */

/*
 * Build a 'section static' symbol.
 */

symbolS *
c_section_symbol (name, idx)
     char *name;
     int idx;
{
  symbolS *symbolP;

  symbolP = symbol_find_base (name, DO_NOT_STRIP);
  if (symbolP == NULL)
    symbolP = symbol_new (name, idx, 0, &zero_address_frag);
  else
    {
      /* Mmmm.  I just love violating interfaces.  Makes me feel...dirty.  */
      S_SET_SEGMENT (symbolP, idx);
      symbolP->sy_frag = &zero_address_frag;
    }

  S_SET_STORAGE_CLASS (symbolP, C_STAT);
  S_SET_NUMBER_AUXILIARY (symbolP, 1);

  SF_SET_STATICS (symbolP);

#ifdef TE_DELTA
  /* manfred@s-direktnet.de: section symbols *must* have the LOCAL bit cleared,
     which is set by the new definition of LOCAL_LABEL in tc-m68k.h.  */
  SF_CLEAR_LOCAL (symbolP);
#endif
#ifdef TE_PE
  /* If the .linkonce pseudo-op was used for this section, we must
     store the information in the auxiliary entry for the section
     symbol.  */
  if (segment_info[idx].linkonce != LINKONCE_UNSET)
    {
      int type;

      switch (segment_info[idx].linkonce)
	{
	default:
	  abort ();
	case LINKONCE_DISCARD:
	  type = IMAGE_COMDAT_SELECT_ANY;
	  break;
	case LINKONCE_ONE_ONLY:
	  type = IMAGE_COMDAT_SELECT_NODUPLICATES;
	  break;
	case LINKONCE_SAME_SIZE:
	  type = IMAGE_COMDAT_SELECT_SAME_SIZE;
	  break;
	case LINKONCE_SAME_CONTENTS:
	  type = IMAGE_COMDAT_SELECT_EXACT_MATCH;
	  break;
	}

      SYM_AUXENT (symbolP)->x_scn.x_comdat = type;
    }
#endif /* TE_PE */

  return symbolP;
}				/* c_section_symbol() */

static void
w_symbols (abfd, where, symbol_rootP)
     bfd * abfd;
     char *where;
     symbolS * symbol_rootP;
{
  symbolS *symbolP;
  unsigned int i;

  /* First fill in those values we have only just worked out */
  for (i = SEG_E0; i < SEG_LAST; i++)
    {
      symbolP = segment_info[i].dot;
      if (symbolP)
	{
	  SA_SET_SCN_SCNLEN (symbolP, segment_info[i].scnhdr.s_size);
	  SA_SET_SCN_NRELOC (symbolP, segment_info[i].scnhdr.s_nreloc);
	  SA_SET_SCN_NLINNO (symbolP, segment_info[i].scnhdr.s_nlnno);
	}
    }

  /*
     * Emit all symbols left in the symbol chain.
     */
  for (symbolP = symbol_rootP; symbolP; symbolP = symbol_next (symbolP))
    {
      /* Used to save the offset of the name. It is used to point
	       to the string in memory but must be a file offset.  */
      register char *temp;

      /* We can't fix the lnnoptr field in yank_symbols with the other
         adjustments, because we have to wait until we know where they
         go in the file.  */
      if (SF_GET_ADJ_LNNOPTR (symbolP))
	{
	  SA_GET_SYM_LNNOPTR (symbolP) +=
	    segment_info[S_GET_SEGMENT (symbolP)].scnhdr.s_lnnoptr;
	}

      tc_coff_symbol_emit_hook (symbolP);

      temp = S_GET_NAME (symbolP);
      if (SF_GET_STRING (symbolP))
	{
	  S_SET_OFFSET (symbolP, symbolP->sy_name_offset);
	  S_SET_ZEROES (symbolP, 0);
	}
      else
	{
	  memset (symbolP->sy_symbol.ost_entry.n_name, 0, SYMNMLEN);
	  strncpy (symbolP->sy_symbol.ost_entry.n_name, temp, SYMNMLEN);
	}
      where = symbol_to_chars (abfd, where, symbolP);
      S_SET_NAME (symbolP, temp);
    }

}				/* w_symbols() */

static void
obj_coff_lcomm (ignore)
     int ignore ATTRIBUTE_UNUSED;
{
  s_lcomm(0);
  return;
#if 0
  char *name;
  char c;
  int temp;
  char *p;

  symbolS *symbolP;

  name = input_line_pointer;

  c = get_symbol_end ();
  p = input_line_pointer;
  *p = c;
  SKIP_WHITESPACE ();
  if (*input_line_pointer != ',')
    {
      as_bad (_("Expected comma after name"));
      ignore_rest_of_line ();
      return;
    }
  if (*input_line_pointer == '\n')
    {
      as_bad (_("Missing size expression"));
      return;
    }
  input_line_pointer++;
  if ((temp = get_absolute_expression ()) < 0)
    {
      as_warn (_("lcomm length (%d.) <0! Ignored."), temp);
      ignore_rest_of_line ();
      return;
    }
  *p = 0;

  symbolP = symbol_find_or_make(name);

  if (S_GET_SEGMENT(symbolP) == SEG_UNKNOWN &&
      S_GET_VALUE(symbolP) == 0)
    {
      if (! need_pass_2)
	{
	  char *p;
	  segT current_seg = now_seg; 	/* save current seg     */
	  subsegT current_subseg = now_subseg;

	  subseg_set (SEG_E2, 1);
	  symbolP->sy_frag = frag_now;
	  p = frag_var(rs_org, 1, 1, (relax_substateT)0, symbolP,
		       (offsetT) temp, (char *) 0);
	  *p = 0;
	  subseg_set (current_seg, current_subseg); /* restore current seg */
	  S_SET_SEGMENT(symbolP, SEG_E2);
	  S_SET_STORAGE_CLASS(symbolP, C_STAT);
	}
    }
  else
    as_bad(_("Symbol %s already defined"), name);

  demand_empty_rest_of_line();
#endif
}

static void
fixup_mdeps (frags, h, this_segment)
     fragS * frags;
     object_headers * h;
     segT this_segment;
{
  subseg_change (this_segment, 0);
  while (frags)
    {
      switch (frags->fr_type)
	{
	case rs_align:
	case rs_align_code:
	case rs_align_test:
	case rs_org:
#ifdef HANDLE_ALIGN
	  HANDLE_ALIGN (frags);
#endif
	  frags->fr_type = rs_fill;
	  frags->fr_offset =
	    ((frags->fr_next->fr_address - frags->fr_address - frags->fr_fix)
	     / frags->fr_var);
	  break;
	case rs_machine_dependent:
	  md_convert_frag (h, this_segment, frags);
	  frag_wane (frags);
	  break;
	default:
	  ;
	}
      frags = frags->fr_next;
    }
}

#if 1

#ifndef TC_FORCE_RELOCATION
#define TC_FORCE_RELOCATION(fix) 0
#endif

static void
fixup_segment (segP, this_segment_type)
     segment_info_type * segP;
     segT this_segment_type;
{
  register fixS * fixP;
  register symbolS *add_symbolP;
  register symbolS *sub_symbolP;
  long add_number;
  register int size;
  register char *place;
  register long where;
  register char pcrel;
  register fragS *fragP;
  register segT add_symbol_segment = absolute_section;

  for (fixP = segP->fix_root; fixP; fixP = fixP->fx_next)
    {
      fragP = fixP->fx_frag;
      know (fragP);
      where = fixP->fx_where;
      place = fragP->fr_literal + where;
      size = fixP->fx_size;
      add_symbolP = fixP->fx_addsy;
      sub_symbolP = fixP->fx_subsy;
      add_number = fixP->fx_offset;
      pcrel = fixP->fx_pcrel;

      /* We want function-relative stabs to work on systems which
	 may use a relaxing linker; thus we must handle the sym1-sym2
	 fixups function-relative stabs generates.

	 Of course, if you actually enable relaxing in the linker, the
	 line and block scoping information is going to be incorrect
	 in some cases.  The only way to really fix this is to support
	 a reloc involving the difference of two symbols.  */
      if (linkrelax
	  && (!sub_symbolP || pcrel))
	continue;

#ifdef TC_I960
      if (fixP->fx_tcbit && SF_GET_CALLNAME (add_symbolP))
	{
	  /* Relocation should be done via the associated 'bal' entry
	     point symbol.  */

	  if (!SF_GET_BALNAME (tc_get_bal_of_call (add_symbolP)))
	    {
	      as_bad_where (fixP->fx_file, fixP->fx_line,
			    _("No 'bal' entry point for leafproc %s"),
			    S_GET_NAME (add_symbolP));
	      continue;
	    }
	  fixP->fx_addsy = add_symbolP = tc_get_bal_of_call (add_symbolP);
	}
#endif

      /* Make sure the symbols have been resolved; this may not have
         happened if these are expression symbols.  */
      if (add_symbolP != NULL && ! add_symbolP->sy_resolved)
	resolve_symbol_value (add_symbolP, 1);

      if (add_symbolP != NULL)
	{
	  /* If this fixup is against a symbol which has been equated
	     to another symbol, convert it to the other symbol.  */
	  if (add_symbolP->sy_value.X_op == O_symbol
	      && (! S_IS_DEFINED (add_symbolP)
		  || S_IS_COMMON (add_symbolP)))
	    {
	      while (add_symbolP->sy_value.X_op == O_symbol
		     && (! S_IS_DEFINED (add_symbolP)
			 || S_IS_COMMON (add_symbolP)))
		{
		  symbolS *n;

		  /* We must avoid looping, as that can occur with a
		     badly written program.  */
		  n = add_symbolP->sy_value.X_add_symbol;
		  if (n == add_symbolP)
		    break;
		  add_number += add_symbolP->sy_value.X_add_number;
		  add_symbolP = n;
		}
	      fixP->fx_addsy = add_symbolP;
	      fixP->fx_offset = add_number;
	    }
	}

      if (sub_symbolP != NULL && ! sub_symbolP->sy_resolved)
	resolve_symbol_value (sub_symbolP, 1);

      if (add_symbolP != NULL
	  && add_symbolP->sy_mri_common)
	{
	  know (add_symbolP->sy_value.X_op == O_symbol);
	  add_number += S_GET_VALUE (add_symbolP);
	  fixP->fx_offset = add_number;
	  add_symbolP = fixP->fx_addsy = add_symbolP->sy_value.X_add_symbol;
	}

      if (add_symbolP)
	{
	  add_symbol_segment = S_GET_SEGMENT (add_symbolP);
	}			/* if there is an addend */

      if (sub_symbolP)
	{
	  if (add_symbolP == NULL || add_symbol_segment == absolute_section)
	    {
	      if (add_symbolP != NULL)
		{
		  add_number += S_GET_VALUE (add_symbolP);
		  add_symbolP = NULL;
		  fixP->fx_addsy = NULL;
		}

	      /* It's just -sym.  */
	      if (S_GET_SEGMENT (sub_symbolP) == absolute_section)
		{
		  add_number -= S_GET_VALUE (sub_symbolP);
		  fixP->fx_subsy = 0;
		  fixP->fx_done = 1;
		}
	      else
		{
#ifndef TC_M68K
		  as_bad_where (fixP->fx_file, fixP->fx_line,
				_("Negative of non-absolute symbol %s"),
				S_GET_NAME (sub_symbolP));
#endif
		  add_number -= S_GET_VALUE (sub_symbolP);
		}		/* not absolute */

	      /* if sub_symbol is in the same segment that add_symbol
		 and add_symbol is either in DATA, TEXT, BSS or ABSOLUTE */
	    }
	  else if (S_GET_SEGMENT (sub_symbolP) == add_symbol_segment
		   && SEG_NORMAL (add_symbol_segment))
	    {
	      /* Difference of 2 symbols from same segment.  Can't
		 make difference of 2 undefineds: 'value' means
		 something different for N_UNDF.  */
#ifdef TC_I960
	      /* Makes no sense to use the difference of 2 arbitrary symbols
	         as the target of a call instruction.  */
	      if (fixP->fx_tcbit)
		{
		  as_bad_where (fixP->fx_file, fixP->fx_line,
				_("callj to difference of 2 symbols"));
		}
#endif /* TC_I960 */
	      add_number += S_GET_VALUE (add_symbolP) -
		S_GET_VALUE (sub_symbolP);
	      add_symbolP = NULL;

	      if (!TC_FORCE_RELOCATION (fixP))
		{
		  fixP->fx_addsy = NULL;
		  fixP->fx_subsy = NULL;
		  fixP->fx_done = 1;
#ifdef TC_M68K /* is this right? */
		  pcrel = 0;
		  fixP->fx_pcrel = 0;
#endif
		}
	    }
	  else
	    {
	      /* Different segments in subtraction.  */
	      know (!(S_IS_EXTERNAL (sub_symbolP) && (S_GET_SEGMENT (sub_symbolP) == absolute_section)));

	      if ((S_GET_SEGMENT (sub_symbolP) == absolute_section))
		{
		  add_number -= S_GET_VALUE (sub_symbolP);
		}
#ifdef DIFF_EXPR_OK
	      else if (S_GET_SEGMENT (sub_symbolP) == this_segment_type
#if 0 /* Okay for 68k, at least...  */
		       && !pcrel
#endif
		       )
		{
		  /* Make it pc-relative.  */
		  add_number += (md_pcrel_from (fixP)
				 - S_GET_VALUE (sub_symbolP));
		  pcrel = 1;
		  fixP->fx_pcrel = 1;
		  sub_symbolP = 0;
		  fixP->fx_subsy = 0;
		}
#endif
	      else
		{
		  as_bad_where (fixP->fx_file, fixP->fx_line,
				_("Can't emit reloc {- %s-seg symbol \"%s\"} @ file address %ld."),
				segment_name (S_GET_SEGMENT (sub_symbolP)),
				S_GET_NAME (sub_symbolP),
				(long) (fragP->fr_address + where));
		}		/* if absolute */
	    }
	}			/* if sub_symbolP */

      if (add_symbolP)
	{
	  if (add_symbol_segment == this_segment_type && pcrel)
	    {
	      /*
	       * This fixup was made when the symbol's segment was
	       * SEG_UNKNOWN, but it is now in the local segment.
	       * So we know how to do the address without relocation.
	       */
#ifdef TC_I960
	      /* reloc_callj() may replace a 'call' with a 'calls' or a 'bal',
	       * in which cases it modifies *fixP as appropriate.  In the case
	       * of a 'calls', no further work is required, and *fixP has been
	       * set up to make the rest of the code below a no-op.
	       */
	      reloc_callj (fixP);
#endif /* TC_I960 */

	      add_number += S_GET_VALUE (add_symbolP);
	      add_number -= md_pcrel_from (fixP);

	      /* We used to do
		   add_number -= segP->scnhdr.s_vaddr;
		 if defined (TC_I386) || defined (TE_LYNX).  I now
		 think that was an error propagated from the case when
		 we are going to emit the relocation.  If we are not
		 going to emit the relocation, then we just want to
		 set add_number to the difference between the symbols.
		 This is a case that would only arise when there is a
		 PC relative reference from a section other than .text
		 to a symbol defined in the same section, and the
		 reference is not relaxed.  Since jump instructions on
		 the i386 are relaxed, this could only arise with a
		 call instruction.  */

	      pcrel = 0;	/* Lie. Don't want further pcrel processing.  */
	      if (!TC_FORCE_RELOCATION (fixP))
		{
		  fixP->fx_addsy = NULL;
		  fixP->fx_done = 1;
		}
	    }
	  else
	    {
	      switch (add_symbol_segment)
		{
		case absolute_section:
#ifdef TC_I960
		  reloc_callj (fixP);	/* See comment about reloc_callj() above*/
#endif /* TC_I960 */
		  add_number += S_GET_VALUE (add_symbolP);
		  add_symbolP = NULL;

		  if (!TC_FORCE_RELOCATION (fixP))
		    {
		      fixP->fx_addsy = NULL;
		      fixP->fx_done = 1;
		    }
		  break;
		default:

#if defined(TC_A29K) || (defined(TE_PE) && defined(TC_I386)) || defined(TC_M88K)
		  /* This really should be handled in the linker, but
		     backward compatibility forbids.  */
		  add_number += S_GET_VALUE (add_symbolP);
#else
		  add_number += S_GET_VALUE (add_symbolP) +
		    segment_info[S_GET_SEGMENT (add_symbolP)].scnhdr.s_paddr;
#endif
		  break;

		case SEG_UNKNOWN:
#ifdef TC_I960
		  if ((int) fixP->fx_bit_fixP == 13)
		    {
		      /* This is a COBR instruction.  They have only a
		       * 13-bit displacement and are only to be used
		       * for local branches: flag as error, don't generate
		       * relocation.
		       */
		      as_bad_where (fixP->fx_file, fixP->fx_line,
				    _("can't use COBR format with external label"));
		      fixP->fx_addsy = NULL;
		      fixP->fx_done = 1;
		      continue;
		    }		/* COBR */
#endif /* TC_I960 */
#if ((defined (TC_I386) || defined (TE_LYNX) || defined (TE_AUX)) && !defined(TE_PE)) || defined (COFF_COMMON_ADDEND)
		  /* 386 COFF uses a peculiar format in which the
		     value of a common symbol is stored in the .text
		     segment (I've checked this on SVR3.2 and SCO
		     3.2.2) Ian Taylor <ian@cygnus.com>.  */
		  /* This is also true for 68k COFF on sysv machines
		     (Checked on Motorola sysv68 R3V6 and R3V7.1, and also on
		     UNIX System V/M68000, Release 1.0 from ATT/Bell Labs)
		     Philippe De Muyter <phdm@info.ucl.ac.be>.  */
		  if (S_IS_COMMON (add_symbolP))
		    add_number += S_GET_VALUE (add_symbolP);
#endif
		  break;

		}		/* switch on symbol seg */
	    }			/* if not in local seg */
	}			/* if there was a + symbol */

      if (pcrel)
	{
#if !defined(TC_M88K) && !(defined(TE_PE) && defined(TC_I386)) && !defined(TC_A29K)
	  /* This adjustment is not correct on the m88k, for which the
	     linker does all the computation.  */
	  add_number -= md_pcrel_from (fixP);
#endif
	  if (add_symbolP == 0)
	    {
	      fixP->fx_addsy = &abs_symbol;
	    }			/* if there's an add_symbol */
#if defined (TC_I386) || defined (TE_LYNX) || defined (TC_I960) || defined (TC_M68K)
	  /* On the 386 we must adjust by the segment vaddr as well.
	     Ian Taylor.

	     I changed the i960 to work this way as well.  This is
	     compatible with the current GNU linker behaviour.  I do
	     not know what other i960 COFF assemblers do.  This is not
	     a common case: normally, only assembler code will contain
	     a PC relative reloc, and only branches which do not
	     originate in the .text section will have a non-zero
	     address.

	     I changed the m68k to work this way as well.  This will
	     break existing PC relative relocs from sections which do
	     not start at address 0, but it will make ld -r work.
	     Ian Taylor, 4 Oct 96.  */

	  add_number -= segP->scnhdr.s_vaddr;
#endif
	}			/* if pcrel */

#ifdef MD_APPLY_FIX3
      md_apply_fix3 (fixP, (valueT *) &add_number, this_segment_type);
#else
      md_apply_fix (fixP, add_number);
#endif

      if (!fixP->fx_bit_fixP && ! fixP->fx_no_overflow)
	{
#ifndef TC_M88K
	  /* The m88k uses the offset field of the reloc to get around
	     this problem.  */
	  if ((size == 1
	       && ((add_number & ~0xFF)
		   || (fixP->fx_signed && (add_number & 0x80)))
	       && ((add_number & ~0xFF) != (-1 & ~0xFF)
		   || (add_number & 0x80) == 0))
	      || (size == 2
		  && ((add_number & ~0xFFFF)
		      || (fixP->fx_signed && (add_number & 0x8000)))
		  && ((add_number & ~0xFFFF) != (-1 & ~0xFFFF)
		      || (add_number & 0x8000) == 0)))
	    {
	      as_bad_where (fixP->fx_file, fixP->fx_line,
			    _("Value of %ld too large for field of %d bytes at 0x%lx"),
			    (long) add_number, size,
			    (unsigned long) (fragP->fr_address + where));
	    }
#endif
#ifdef WARN_SIGNED_OVERFLOW_WORD
	  /* Warn if a .word value is too large when treated as a
	     signed number.  We already know it is not too negative.
	     This is to catch over-large switches generated by gcc on
	     the 68k.  */
	  if (!flag_signed_overflow_ok
	      && size == 2
	      && add_number > 0x7fff)
	    as_bad_where (fixP->fx_file, fixP->fx_line,
			  _("Signed .word overflow; switch may be too large; %ld at 0x%lx"),
			  (long) add_number,
			  (unsigned long) (fragP->fr_address + where));
#endif
	}			/* not a bit fix */
    }				/* For each fixS in this segment.  */
}				/* fixup_segment() */

#endif

/* The first entry in a .stab section is special.  */

void
obj_coff_init_stab_section (seg)
     segT seg;
{
  char *file;
  char *p;
  char *stabstr_name;
  unsigned int stroff;

  /* Make space for this first symbol.  */
  p = frag_more (12);
  /* Zero it out.  */
  memset (p, 0, 12);
  as_where (&file, (unsigned int *) NULL);
  stabstr_name = (char *) alloca (strlen (segment_info[seg].name) + 4);
  strcpy (stabstr_name, segment_info[seg].name);
  strcat (stabstr_name, "str");
  stroff = get_stab_string_offset (file, stabstr_name);
  know (stroff == 1);
  md_number_to_chars (p, stroff, 4);
}

/* Fill in the counts in the first entry in a .stab section.  */

static void
adjust_stab_section(abfd, seg)
     bfd *abfd;
     segT seg;
{
  segT stabstrseg = SEG_UNKNOWN;
  const char *secname, *name2;
  char *name;
  char *p = NULL;
  int i, strsz = 0, nsyms;
  fragS *frag = segment_info[seg].frchainP->frch_root;

  /* Look for the associated string table section.  */

  secname = segment_info[seg].name;
  name = (char *) alloca (strlen (secname) + 4);
  strcpy (name, secname);
  strcat (name, "str");

  for (i = SEG_E0; i < SEG_UNKNOWN; i++)
    {
      name2 = segment_info[i].name;
      if (name2 != NULL && strncmp(name2, name, 8) == 0)
	{
	  stabstrseg = i;
	  break;
	}
    }

  /* If we found the section, get its size.  */
  if (stabstrseg != SEG_UNKNOWN)
    strsz = size_section (abfd, stabstrseg);

  nsyms = size_section (abfd, seg) / 12 - 1;

  /* Look for the first frag of sufficient size for the initial stab
     symbol, and collect a pointer to it.  */
  while (frag && frag->fr_fix < 12)
    frag = frag->fr_next;
  assert (frag != 0);
  p = frag->fr_literal;
  assert (p != 0);

  /* Write in the number of stab symbols and the size of the string
     table.  */
  bfd_h_put_16 (abfd, (bfd_vma) nsyms, (bfd_byte *) p + 6);
  bfd_h_put_32 (abfd, (bfd_vma) strsz, (bfd_byte *) p + 8);
}

#endif /* not BFD_ASSEMBLER */

const pseudo_typeS coff_pseudo_table[] =
{
  {"def", obj_coff_def, 0},
  {"dim", obj_coff_dim, 0},
  {"endef", obj_coff_endef, 0},
  {"line", obj_coff_line, 0},
  {"ln", obj_coff_ln, 0},
#ifdef BFD_ASSEMBLER
  {"loc", obj_coff_loc, 0},
#endif
  {"appline", obj_coff_ln, 1},
  {"scl", obj_coff_scl, 0},
  {"size", obj_coff_size, 0},
  {"tag", obj_coff_tag, 0},
  {"type", obj_coff_type, 0},
  {"val", obj_coff_val, 0},
  {"section", obj_coff_section, 0},
  {"sect", obj_coff_section, 0},
  /* FIXME: We ignore the MRI short attribute.  */
  {"section.s", obj_coff_section, 0},
  {"sect.s", obj_coff_section, 0},
  /* We accept the .bss directive for backward compatibility with
     earlier versions of gas.  */
  {"bss", obj_coff_bss, 0},
  {"weak", obj_coff_weak, 0},
  {"ident", obj_coff_ident, 0},
#ifndef BFD_ASSEMBLER
  {"use", obj_coff_section, 0},
  {"text", obj_coff_text, 0},
  {"data", obj_coff_data, 0},
  {"lcomm", obj_coff_lcomm, 0},
#else
  {"optim", s_ignore, 0},	/* For sun386i cc (?) */
#endif
  {"version", s_ignore, 0},
  {"ABORT", s_abort, 0},
#ifdef TC_M88K
  /* The m88k uses sdef instead of def.  */
  {"sdef", obj_coff_def, 0},
#endif
  {NULL, NULL, 0}		/* end sentinel */
};				/* coff_pseudo_table */

#ifdef BFD_ASSEMBLER

/* Support for a COFF emulation.  */

static void coff_pop_insert PARAMS ((void));
static int coff_separate_stab_sections PARAMS ((void));

static void
coff_pop_insert ()
{
  pop_insert (coff_pseudo_table);
}

static int
coff_separate_stab_sections ()
{
  return 1;
}

const struct format_ops coff_format_ops =
{
  bfd_target_coff_flavour,
  0,	/* dfl_leading_underscore */
  1,	/* emit_section_symbols */
  0,    /* begin */
  c_dot_file_symbol,
  coff_frob_symbol,
  0,	/* frob_file */
  0,	/* frob_file_before_adjust */
  coff_frob_file_after_relocs,
  0,	/* s_get_size */
  0,	/* s_set_size */
  0,	/* s_get_align */
  0,	/* s_set_align */
  0,	/* s_get_other */
  0,	/* s_set_other */
  0,	/* s_get_desc */
  0,	/* s_set_desc */
  0,	/* s_get_type */
  0,	/* s_set_type */
  0,	/* copy_symbol_attributes */
  0,	/* generate_asm_lineno */
  0,	/* process_stab */
  coff_separate_stab_sections,
  obj_coff_init_stab_section,
  0,	/* sec_sym_ok_for_reloc */
  coff_pop_insert,
  0,	/* ecoff_set_ext */
  coff_obj_read_begin_hook,
  coff_obj_symbol_new_hook
};

#endif
