/* a.out object file format
   Copyright (C) 1989, 90, 91, 92, 93, 94, 95, 96, 97, 98, 99, 2000
   Free Software Foundation, Inc.

This file is part of GAS, the GNU Assembler.

GAS is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as
published by the Free Software Foundation; either version 2,
or (at your option) any later version.

GAS is distributed in the hope that it will be useful, but
WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See
the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with GAS; see the file COPYING.  If not, write to the Free
Software Foundation, 59 Temple Place - Suite 330, Boston, MA
02111-1307, USA.  */

#define OBJ_HEADER "obj-aout.h"

#include "as.h"
#ifdef BFD_ASSEMBLER
#undef NO_RELOC
#include "aout/aout64.h"
#endif
#include "obstack.h"

#ifndef BFD_ASSEMBLER
/* in: segT   out: N_TYPE bits */
const short seg_N_TYPE[] =
{
  N_ABS,
  N_TEXT,
  N_DATA,
  N_BSS,
  N_UNDF,			/* unknown */
  N_UNDF,			/* error */
  N_UNDF,			/* expression */
  N_UNDF,			/* debug */
  N_UNDF,			/* ntv */
  N_UNDF,			/* ptv */
  N_REGISTER,			/* register */
};

const segT N_TYPE_seg[N_TYPE + 2] =
{				/* N_TYPE == 0x1E = 32-2 */
  SEG_UNKNOWN,			/* N_UNDF == 0 */
  SEG_GOOF,
  SEG_ABSOLUTE,			/* N_ABS == 2 */
  SEG_GOOF,
  SEG_TEXT,			/* N_TEXT == 4 */
  SEG_GOOF,
  SEG_DATA,			/* N_DATA == 6 */
  SEG_GOOF,
  SEG_BSS,			/* N_BSS == 8 */
  SEG_GOOF,
  SEG_GOOF, SEG_GOOF, SEG_GOOF, SEG_GOOF, SEG_GOOF, SEG_GOOF, SEG_GOOF, SEG_GOOF,
  SEG_GOOF, SEG_GOOF, SEG_GOOF, SEG_GOOF, SEG_GOOF, SEG_GOOF, SEG_GOOF, SEG_GOOF,
  SEG_GOOF, SEG_GOOF, SEG_GOOF, SEG_GOOF,
  SEG_REGISTER,			/* dummy N_REGISTER for regs = 30 */
  SEG_GOOF,
};
#endif

static void obj_aout_line PARAMS ((int));
static void obj_aout_weak PARAMS ((int));
static void obj_aout_type PARAMS ((int));

const pseudo_typeS aout_pseudo_table[] =
{
  {"line", obj_aout_line, 0},	/* source code line number */
  {"ln", obj_aout_line, 0},	/* coff line number that we use anyway */

  {"weak", obj_aout_weak, 0},	/* mark symbol as weak.  */

  {"type", obj_aout_type, 0},

  /* coff debug pseudos (ignored) */
  {"def", s_ignore, 0},
  {"dim", s_ignore, 0},
  {"endef", s_ignore, 0},
  {"ident", s_ignore, 0},
  {"line", s_ignore, 0},
  {"ln", s_ignore, 0},
  {"scl", s_ignore, 0},
  {"size", s_ignore, 0},
  {"tag", s_ignore, 0},
  {"val", s_ignore, 0},
  {"version", s_ignore, 0},

  {"optim", s_ignore, 0},	/* For sun386i cc (?) */

  /* other stuff */
  {"ABORT", s_abort, 0},

  {NULL, NULL, 0}		/* end sentinel */
};				/* aout_pseudo_table */

#ifdef BFD_ASSEMBLER

void
obj_aout_frob_symbol (sym, punt)
     symbolS *sym;
     int *punt ATTRIBUTE_UNUSED;
{
  flagword flags;
  asection *sec;
  int desc, type, other;

  flags = symbol_get_bfdsym (sym)->flags;
  desc = aout_symbol (symbol_get_bfdsym (sym))->desc;
  type = aout_symbol (symbol_get_bfdsym (sym))->type;
  other = aout_symbol (symbol_get_bfdsym (sym))->other;
  sec = S_GET_SEGMENT (sym);

  /* Only frob simple symbols this way right now.  */
  if (! (type & ~ (N_TYPE | N_EXT)))
    {
      if (type == (N_UNDF | N_EXT)
	  && sec == &bfd_abs_section)
	{
	  sec = bfd_und_section_ptr;
	  S_SET_SEGMENT (sym, sec);
	}

      if ((type & N_TYPE) != N_INDR
	  && (type & N_TYPE) != N_SETA
	  && (type & N_TYPE) != N_SETT
	  && (type & N_TYPE) != N_SETD
	  && (type & N_TYPE) != N_SETB
	  && type != N_WARNING
	  && (sec == &bfd_abs_section
	      || sec == &bfd_und_section))
	return;
      if (flags & BSF_EXPORT)
	type |= N_EXT;

      switch (type & N_TYPE)
	{
	case N_SETA:
	case N_SETT:
	case N_SETD:
	case N_SETB:
	  /* Set the debugging flag for constructor symbols so that
	     BFD leaves them alone.  */
	  symbol_get_bfdsym (sym)->flags |= BSF_DEBUGGING;

	  /* You can't put a common symbol in a set.  The way a set
	     element works is that the symbol has a definition and a
	     name, and the linker adds the definition to the set of
	     that name.  That does not work for a common symbol,
	     because the linker can't tell which common symbol the
	     user means.  FIXME: Using as_bad here may be
	     inappropriate, since the user may want to force a
	     particular type without regard to the semantics of sets;
	     on the other hand, we certainly don't want anybody to be
	     mislead into thinking that their code will work.  */
	  if (S_IS_COMMON (sym))
	    as_bad (_("Attempt to put a common symbol into set %s"),
		    S_GET_NAME (sym));
	  /* Similarly, you can't put an undefined symbol in a set.  */
	  else if (! S_IS_DEFINED (sym))
	    as_bad (_("Attempt to put an undefined symbol into set %s"),
		    S_GET_NAME (sym));

	  break;
	case N_INDR:
	  /* Put indirect symbols in the indirect section.  */
	  S_SET_SEGMENT (sym, bfd_ind_section_ptr);
	  symbol_get_bfdsym (sym)->flags |= BSF_INDIRECT;
	  if (type & N_EXT)
	    {
	      symbol_get_bfdsym (sym)->flags |= BSF_EXPORT;
	      symbol_get_bfdsym (sym)->flags &=~ BSF_LOCAL;
	    }
	  break;
	case N_WARNING:
	  /* Mark warning symbols.  */
	  symbol_get_bfdsym (sym)->flags |= BSF_WARNING;
	  break;
	}
    }
  else
    {
      symbol_get_bfdsym (sym)->flags |= BSF_DEBUGGING;
    }

  aout_symbol (symbol_get_bfdsym (sym))->type = type;

  /* Double check weak symbols.  */
  if (S_IS_WEAK (sym))
    {
      if (S_IS_COMMON (sym))
	as_bad (_("Symbol `%s' can not be both weak and common"),
		S_GET_NAME (sym));
    }
}

void
obj_aout_frob_file ()
{
  /* Relocation processing may require knowing the VMAs of the sections.
     Since writing to a section will cause the BFD back end to compute the
     VMAs, fake it out here....  */
  bfd_byte b = 0;
  boolean x = true;
  if (bfd_section_size (stdoutput, text_section) != 0)
    {
      x = bfd_set_section_contents (stdoutput, text_section, &b, (file_ptr) 0,
				    (bfd_size_type) 1);
    }
  else if (bfd_section_size (stdoutput, data_section) != 0)
    {
      x = bfd_set_section_contents (stdoutput, data_section, &b, (file_ptr) 0,
				    (bfd_size_type) 1);
    }
  assert (x == true);
}

#else /* ! BFD_ASSEMBLER */

/* Relocation.  */

/*
 *		emit_relocations()
 *
 * Crawl along a fixS chain. Emit the segment's relocations.
 */
void
obj_emit_relocations (where, fixP, segment_address_in_file)
     char **where;
     fixS *fixP;		/* Fixup chain for this segment.  */
     relax_addressT segment_address_in_file;
{
  for (; fixP; fixP = fixP->fx_next)
    if (fixP->fx_done == 0)
      {
	symbolS *sym;

	sym = fixP->fx_addsy;
	while (sym->sy_value.X_op == O_symbol
	       && (! S_IS_DEFINED (sym) || S_IS_COMMON (sym)))
	  sym = sym->sy_value.X_add_symbol;
	fixP->fx_addsy = sym;

	if (! sym->sy_resolved && ! S_IS_DEFINED (sym))
	  {
	    char *file;
	    unsigned int line;

	    if (expr_symbol_where (sym, &file, &line))
	      as_bad_where (file, line, _("unresolved relocation"));
	    else
	      as_bad (_("bad relocation: symbol `%s' not in symbol table"),
		      S_GET_NAME (sym));
	  }

	tc_aout_fix_to_chars (*where, fixP, segment_address_in_file);
	*where += md_reloc_size;
      }
}

#ifndef obj_header_append
/* Aout file generation & utilities */
void
obj_header_append (where, headers)
     char **where;
     object_headers *headers;
{
  tc_headers_hook (headers);

#ifdef CROSS_COMPILE
  md_number_to_chars (*where, headers->header.a_info, sizeof (headers->header.a_info));
  *where += sizeof (headers->header.a_info);
  md_number_to_chars (*where, headers->header.a_text, sizeof (headers->header.a_text));
  *where += sizeof (headers->header.a_text);
  md_number_to_chars (*where, headers->header.a_data, sizeof (headers->header.a_data));
  *where += sizeof (headers->header.a_data);
  md_number_to_chars (*where, headers->header.a_bss, sizeof (headers->header.a_bss));
  *where += sizeof (headers->header.a_bss);
  md_number_to_chars (*where, headers->header.a_syms, sizeof (headers->header.a_syms));
  *where += sizeof (headers->header.a_syms);
  md_number_to_chars (*where, headers->header.a_entry, sizeof (headers->header.a_entry));
  *where += sizeof (headers->header.a_entry);
  md_number_to_chars (*where, headers->header.a_trsize, sizeof (headers->header.a_trsize));
  *where += sizeof (headers->header.a_trsize);
  md_number_to_chars (*where, headers->header.a_drsize, sizeof (headers->header.a_drsize));
  *where += sizeof (headers->header.a_drsize);

#else /* CROSS_COMPILE */

  append (where, (char *) &headers->header, sizeof (headers->header));
#endif /* CROSS_COMPILE */

}
#endif /* ! defined (obj_header_append) */

void
obj_symbol_to_chars (where, symbolP)
     char **where;
     symbolS *symbolP;
{
  md_number_to_chars ((char *) &(S_GET_OFFSET (symbolP)), S_GET_OFFSET (symbolP), sizeof (S_GET_OFFSET (symbolP)));
  md_number_to_chars ((char *) &(S_GET_DESC (symbolP)), S_GET_DESC (symbolP), sizeof (S_GET_DESC (symbolP)));
  md_number_to_chars ((char *) &(symbolP->sy_symbol.n_value), S_GET_VALUE (symbolP), sizeof (symbolP->sy_symbol.n_value));

  append (where, (char *) &symbolP->sy_symbol, sizeof (obj_symbol_type));
}

void
obj_emit_symbols (where, symbol_rootP)
     char **where;
     symbolS *symbol_rootP;
{
  symbolS *symbolP;

  /* Emit all symbols left in the symbol chain.  */
  for (symbolP = symbol_rootP; symbolP; symbolP = symbol_next (symbolP))
    {
      /* Used to save the offset of the name. It is used to point
	 to the string in memory but must be a file offset.  */
      register char *temp;

      temp = S_GET_NAME (symbolP);
      S_SET_OFFSET (symbolP, symbolP->sy_name_offset);

      /* Any symbol still undefined and is not a dbg symbol is made N_EXT.  */
      if (!S_IS_DEBUG (symbolP) && !S_IS_DEFINED (symbolP))
	S_SET_EXTERNAL (symbolP);

      /* Adjust the type of a weak symbol.  */
      if (S_GET_WEAK (symbolP))
	{
	  switch (S_GET_TYPE (symbolP))
	    {
	    case N_UNDF: S_SET_TYPE (symbolP, N_WEAKU); break;
	    case N_ABS:	 S_SET_TYPE (symbolP, N_WEAKA); break;
	    case N_TEXT: S_SET_TYPE (symbolP, N_WEAKT); break;
	    case N_DATA: S_SET_TYPE (symbolP, N_WEAKD); break;
	    case N_BSS:  S_SET_TYPE (symbolP, N_WEAKB); break;
	    default: as_bad (_("%s: bad type for weak symbol"), temp); break;
	    }
	}

      obj_symbol_to_chars (where, symbolP);
      S_SET_NAME (symbolP, temp);
    }
}

#endif /* ! BFD_ASSEMBLER */

static void
obj_aout_line (ignore)
     int ignore ATTRIBUTE_UNUSED;
{
  /* Assume delimiter is part of expression.
     BSD4.2 as fails with delightful bug, so we
     are not being incompatible here.  */
  new_logical_line ((char *) NULL, (int) (get_absolute_expression ()));
  demand_empty_rest_of_line ();
}				/* obj_aout_line() */

/* Handle .weak.  This is a GNU extension.  */

static void
obj_aout_weak (ignore)
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
      S_SET_WEAK (symbolP);
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

/* Handle .type.  On {Net,Open}BSD, this is used to set the n_other field,
   which is then apparently used when doing dynamic linking.  Older
   versions of gas ignored the .type pseudo-op, so we also ignore it if
   we can't parse it.  */

static void
obj_aout_type (ignore)
     int ignore ATTRIBUTE_UNUSED;
{
  char *name;
  int c;
  symbolS *sym;

  name = input_line_pointer;
  c = get_symbol_end ();
  sym = symbol_find_or_make (name);
  *input_line_pointer = c;
  SKIP_WHITESPACE ();
  if (*input_line_pointer == ',')
    {
      ++input_line_pointer;
      SKIP_WHITESPACE ();
      if (*input_line_pointer == '@')
	{
	  ++input_line_pointer;
	  if (strncmp (input_line_pointer, "object", 6) == 0)
#ifdef BFD_ASSEMBLER
	    aout_symbol (symbol_get_bfdsym (sym))->other = 1;
#else
	  S_SET_OTHER (sym, 1);
#endif
	  else if (strncmp (input_line_pointer, "function", 8) == 0)
#ifdef BFD_ASSEMBLER
	    aout_symbol (symbol_get_bfdsym (sym))->other = 2;
#else
	  S_SET_OTHER (sym, 2);
#endif
	}
    }

  /* Ignore everything else on the line.  */
  s_ignore (0);
}

#ifndef BFD_ASSEMBLER

void
obj_crawl_symbol_chain (headers)
     object_headers *headers;
{
  symbolS *symbolP;
  symbolS **symbolPP;
  int symbol_number = 0;

  tc_crawl_symbol_chain (headers);

  symbolPP = &symbol_rootP;	/*->last symbol chain link.  */
  while ((symbolP = *symbolPP) != NULL)
    {
      if (symbolP->sy_mri_common)
	{
	  if (S_IS_EXTERNAL (symbolP))
	    as_bad (_("%s: global symbols not supported in common sections"),
		    S_GET_NAME (symbolP));
	  *symbolPP = symbol_next (symbolP);
	  continue;
	}

      if (flag_readonly_data_in_text && (S_GET_SEGMENT (symbolP) == SEG_DATA))
	{
	  S_SET_SEGMENT (symbolP, SEG_TEXT);
	}			/* if pusing data into text */

      resolve_symbol_value (symbolP, 1);

      /* Skip symbols which were equated to undefined or common
	 symbols.  */
      if (symbolP->sy_value.X_op == O_symbol
	  && (! S_IS_DEFINED (symbolP) || S_IS_COMMON (symbolP)))
	{
	  *symbolPP = symbol_next (symbolP);
	  continue;
	}

      /* OK, here is how we decide which symbols go out into the brave
	 new symtab.  Symbols that do are:

	 * symbols with no name (stabd's?)
	 * symbols with debug info in their N_TYPE

	 Symbols that don't are:
	 * symbols that are registers
	 * symbols with \1 as their 3rd character (numeric labels)
	 * "local labels" as defined by S_LOCAL_NAME(name) if the -L
	 switch was passed to gas.

	 All other symbols are output.  We complain if a deleted
	 symbol was marked external.  */

      if (!S_IS_REGISTER (symbolP)
	  && (!S_GET_NAME (symbolP)
	      || S_IS_DEBUG (symbolP)
	      || !S_IS_DEFINED (symbolP)
	      || S_IS_EXTERNAL (symbolP)
	      || (S_GET_NAME (symbolP)[0] != '\001'
		  && (flag_keep_locals || !S_LOCAL_NAME (symbolP)))))
	{
	  symbolP->sy_number = symbol_number++;

	  /* The + 1 after strlen account for the \0 at the
			   end of each string */
	  if (!S_IS_STABD (symbolP))
	    {
	      /* Ordinary case.  */
	      symbolP->sy_name_offset = string_byte_count;
	      string_byte_count += strlen (S_GET_NAME (symbolP)) + 1;
	    }
	  else			/* .Stabd case.  */
	    symbolP->sy_name_offset = 0;
	  symbolPP = &symbolP->sy_next;
	}
      else
	{
	  if (S_IS_EXTERNAL (symbolP) || !S_IS_DEFINED (symbolP))
	    /* This warning should never get triggered any more.
	       Well, maybe if you're doing twisted things with
	       register names...  */
	    {
	      as_bad (_("Local symbol %s never defined."), decode_local_label_name (S_GET_NAME (symbolP)));
	    }			/* oops.  */

	  /* Unhook it from the chain */
	  *symbolPP = symbol_next (symbolP);
	}			/* if this symbol should be in the output */
    }				/* for each symbol */

  H_SET_SYMBOL_TABLE_SIZE (headers, symbol_number);
}

/*
 * Find strings by crawling along symbol table chain.
 */

void
obj_emit_strings (where)
     char **where;
{
  symbolS *symbolP;

#ifdef CROSS_COMPILE
  /* Gotta do md_ byte-ordering stuff for string_byte_count first - KWK */
  md_number_to_chars (*where, string_byte_count, sizeof (string_byte_count));
  *where += sizeof (string_byte_count);
#else /* CROSS_COMPILE */
  append (where, (char *) &string_byte_count, (unsigned long) sizeof (string_byte_count));
#endif /* CROSS_COMPILE */

  for (symbolP = symbol_rootP; symbolP; symbolP = symbol_next (symbolP))
    {
      if (S_GET_NAME (symbolP))
	append (&next_object_file_charP, S_GET_NAME (symbolP),
		(unsigned long) (strlen (S_GET_NAME (symbolP)) + 1));
    }				/* walk symbol chain */
}

#ifndef AOUT_VERSION
#define AOUT_VERSION 0
#endif

void
obj_pre_write_hook (headers)
     object_headers *headers;
{
  H_SET_DYNAMIC (headers, 0);
  H_SET_VERSION (headers, AOUT_VERSION);
  H_SET_MACHTYPE (headers, AOUT_MACHTYPE);
  tc_aout_pre_write_hook (headers);
}

void
s_sect ()
{
  /* Strip out the section name */
  char *section_name;
  char *section_name_end;
  char c;

  unsigned int len;
  unsigned int exp;
  char *save;

  section_name = input_line_pointer;
  c = get_symbol_end ();
  section_name_end = input_line_pointer;

  len = section_name_end - section_name;
  input_line_pointer++;
  save = input_line_pointer;

  SKIP_WHITESPACE ();
  if (c == ',')
    {
      exp = get_absolute_expression ();
    }
  else if (*input_line_pointer == ',')
    {
      input_line_pointer++;
      exp = get_absolute_expression ();
    }
  else
    {
      input_line_pointer = save;
      exp = 0;
    }
  if (exp >= 1000)
    {
      as_bad (_("subsegment index too high"));
    }

  if (strcmp (section_name, ".text") == 0)
    {
      subseg_set (SEG_TEXT, (subsegT) exp);
    }

  if (strcmp (section_name, ".data") == 0)
    {
      if (flag_readonly_data_in_text)
	subseg_set (SEG_TEXT, (subsegT) exp + 1000);
      else
	subseg_set (SEG_DATA, (subsegT) exp);
    }

  *section_name_end = c;
}

#endif /* ! BFD_ASSEMBLER */

#ifdef BFD_ASSEMBLER

/* Support for an AOUT emulation.  */

static void aout_pop_insert PARAMS ((void));
static int obj_aout_s_get_other PARAMS ((symbolS *));
static void obj_aout_s_set_other PARAMS ((symbolS *, int));
static int obj_aout_s_get_desc PARAMS ((symbolS *));
static void obj_aout_s_set_desc PARAMS ((symbolS *, int));
static int obj_aout_s_get_type PARAMS ((symbolS *));
static void obj_aout_s_set_type PARAMS ((symbolS *, int));
static int obj_aout_separate_stab_sections PARAMS ((void));
static int obj_aout_sec_sym_ok_for_reloc PARAMS ((asection *));
static void obj_aout_process_stab PARAMS ((segT, int, const char *, int, int, int));

static void
aout_pop_insert ()
{
  pop_insert (aout_pseudo_table);
}

static int
obj_aout_s_get_other (sym)
     symbolS *sym;
{
  return aout_symbol (symbol_get_bfdsym (sym))->other;
}

static void
obj_aout_s_set_other (sym, o)
     symbolS *sym;
     int o;
{
  aout_symbol (symbol_get_bfdsym (sym))->other = o;
}

static int
obj_aout_sec_sym_ok_for_reloc (sec)
     asection *sec ATTRIBUTE_UNUSED;
{
  return obj_sec_sym_ok_for_reloc (sec);
}

static void
obj_aout_process_stab (seg, w, s, t, o, d)
     segT seg ATTRIBUTE_UNUSED;
     int w;
     const char *s;
     int t;
     int o;
     int d;
{
  aout_process_stab (w, s, t, o, d);
}

static int
obj_aout_s_get_desc (sym)
     symbolS *sym;
{
  return aout_symbol (symbol_get_bfdsym (sym))->desc;
}

static void
obj_aout_s_set_desc (sym, d)
     symbolS *sym;
     int d;
{
  aout_symbol (symbol_get_bfdsym (sym))->desc = d;
}

static int
obj_aout_s_get_type (sym)
     symbolS *sym;
{
  return aout_symbol (symbol_get_bfdsym (sym))->type;
}

static void
obj_aout_s_set_type (sym, t)
     symbolS *sym;
     int t;
{
  aout_symbol (symbol_get_bfdsym (sym))->type = t;
}

static int
obj_aout_separate_stab_sections ()
{
  return 0;
}

/* When changed, make sure these table entries match the single-format
   definitions in obj-aout.h.  */
const struct format_ops aout_format_ops =
{
  bfd_target_aout_flavour,
  1,	/* dfl_leading_underscore */
  0,	/* emit_section_symbols */
  0,	/* begin */
  0,	/* app_file */
  obj_aout_frob_symbol,
  obj_aout_frob_file,
  0,	/* frob_file_before_adjust */
  0,	/* frob_file_after_relocs */
  0,	/* s_get_size */
  0,	/* s_set_size */
  0,	/* s_get_align */
  0,	/* s_set_align */
  obj_aout_s_get_other,
  obj_aout_s_set_other,
  obj_aout_s_get_desc,
  obj_aout_s_set_desc,
  obj_aout_s_get_type,
  obj_aout_s_set_type,
  0,	/* copy_symbol_attributes */
  0,	/* generate_asm_lineno */
  obj_aout_process_stab,
  obj_aout_separate_stab_sections,
  0,	/* init_stab_section */
  obj_aout_sec_sym_ok_for_reloc,
  aout_pop_insert,
  0,	/* ecoff_set_ext */
  0,	/* read_begin_hook */
  0 	/* symbol_new_hook */
};
#endif BFD_ASSEMBLER
