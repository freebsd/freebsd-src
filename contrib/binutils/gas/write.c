/* write.c - emit .o file
   Copyright 1986, 1987, 1990, 1991, 1992, 1993, 1994, 1995, 1996, 1997,
   1998, 1999, 2000, 2001
   Free Software Foundation, Inc.

   This file is part of GAS, the GNU Assembler.

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

/* This thing should be set up to do byteordering correctly.  But...  */

#include "as.h"
#include "subsegs.h"
#include "obstack.h"
#include "output-file.h"
#include "dwarf2dbg.h"

/* This looks like a good idea.  Let's try turning it on always, for now.  */
#undef  BFD_FAST_SECTION_FILL
#define BFD_FAST_SECTION_FILL

#ifndef TC_ADJUST_RELOC_COUNT
#define TC_ADJUST_RELOC_COUNT(FIXP,COUNT)
#endif

#ifndef TC_FORCE_RELOCATION
#define TC_FORCE_RELOCATION(FIXP) 0
#endif

#ifndef TC_FORCE_RELOCATION_SECTION
#define TC_FORCE_RELOCATION_SECTION(FIXP,SEG) TC_FORCE_RELOCATION(FIXP)
#endif

#ifndef TC_LINKRELAX_FIXUP
#define TC_LINKRELAX_FIXUP(SEG) 1
#endif

#ifndef TC_FIX_ADJUSTABLE
#define TC_FIX_ADJUSTABLE(fix) 1
#endif

#ifndef	MD_PCREL_FROM_SECTION
#define MD_PCREL_FROM_SECTION(FIXP, SEC) md_pcrel_from(FIXP)
#endif

#ifndef WORKING_DOT_WORD
extern CONST int md_short_jump_size;
extern CONST int md_long_jump_size;
#endif

int symbol_table_frozen;
void print_fixup PARAMS ((fixS *));

#ifdef BFD_ASSEMBLER
static void renumber_sections PARAMS ((bfd *, asection *, PTR));

/* We generally attach relocs to frag chains.  However, after we have
   chained these all together into a segment, any relocs we add after
   that must be attached to a segment.  This will include relocs added
   in md_estimate_size_for_relax, for example.  */
static int frags_chained = 0;
#endif

#ifndef BFD_ASSEMBLER

#ifndef MANY_SEGMENTS
struct frag *text_frag_root;
struct frag *data_frag_root;
struct frag *bss_frag_root;

struct frag *text_last_frag;	/* Last frag in segment.  */
struct frag *data_last_frag;	/* Last frag in segment.  */
static struct frag *bss_last_frag;	/* Last frag in segment.  */
#endif

#ifndef BFD
static object_headers headers;
#endif

long string_byte_count;
char *next_object_file_charP;	/* Tracks object file bytes.  */

#ifndef OBJ_VMS
int magic_number_for_object_file = DEFAULT_MAGIC_NUMBER_FOR_OBJECT_FILE;
#endif

#endif /* BFD_ASSEMBLER  */

static int n_fixups;

#ifdef BFD_ASSEMBLER
static fixS *fix_new_internal PARAMS ((fragS *, int where, int size,
				       symbolS *add, symbolS *sub,
				       offsetT offset, int pcrel,
				       bfd_reloc_code_real_type r_type));
#else
static fixS *fix_new_internal PARAMS ((fragS *, int where, int size,
				       symbolS *add, symbolS *sub,
				       offsetT offset, int pcrel,
				       int r_type));
#endif
#if defined (BFD_ASSEMBLER) || (!defined (BFD) && !defined (OBJ_VMS))
static long fixup_segment PARAMS ((fixS * fixP, segT this_segment_type));
#endif
static relax_addressT relax_align PARAMS ((relax_addressT addr, int align));
#if defined (BFD_ASSEMBLER) || ! defined (BFD)
static fragS *chain_frchains_together_1 PARAMS ((segT, struct frchain *));
#endif
#ifdef BFD_ASSEMBLER
static void chain_frchains_together PARAMS ((bfd *, segT, PTR));
static void cvt_frag_to_fill PARAMS ((segT, fragS *));
static void relax_seg PARAMS ((bfd *, asection *, PTR));
static void size_seg PARAMS ((bfd *, asection *, PTR));
static void adjust_reloc_syms PARAMS ((bfd *, asection *, PTR));
static void write_relocs PARAMS ((bfd *, asection *, PTR));
static void write_contents PARAMS ((bfd *, asection *, PTR));
static void set_symtab PARAMS ((void));
#endif
#if defined (BFD_ASSEMBLER) || (! defined (BFD) && ! defined (OBJ_AOUT))
static void merge_data_into_text PARAMS ((void));
#endif
#if ! defined (BFD_ASSEMBLER) && ! defined (BFD)
static void cvt_frag_to_fill PARAMS ((object_headers *, segT, fragS *));
static void remove_subsegs PARAMS ((frchainS *, int, fragS **, fragS **));
static void relax_and_size_all_segments PARAMS ((void));
#endif
#if defined (BFD_ASSEMBLER) && defined (OBJ_COFF) && defined (TE_GO32)
static void set_segment_vma PARAMS ((bfd *, asection *, PTR));
#endif

/* Create a fixS in obstack 'notes'.  */

static fixS *
fix_new_internal (frag, where, size, add_symbol, sub_symbol, offset, pcrel,
		  r_type)
     fragS *frag;		/* Which frag?  */
     int where;			/* Where in that frag?  */
     int size;			/* 1, 2, or 4 usually.  */
     symbolS *add_symbol;	/* X_add_symbol.  */
     symbolS *sub_symbol;	/* X_op_symbol.  */
     offsetT offset;		/* X_add_number.  */
     int pcrel;			/* TRUE if PC-relative relocation.  */
#ifdef BFD_ASSEMBLER
     bfd_reloc_code_real_type r_type; /* Relocation type.  */
#else
     int r_type;		/* Relocation type.  */
#endif
{
  fixS *fixP;

  n_fixups++;

  fixP = (fixS *) obstack_alloc (&notes, sizeof (fixS));

  fixP->fx_frag = frag;
  fixP->fx_where = where;
  fixP->fx_size = size;
  /* We've made fx_size a narrow field; check that it's wide enough.  */
  if (fixP->fx_size != size)
    {
      as_bad (_("field fx_size too small to hold %d"), size);
      abort ();
    }
  fixP->fx_addsy = add_symbol;
  fixP->fx_subsy = sub_symbol;
  fixP->fx_offset = offset;
  fixP->fx_pcrel = pcrel;
  fixP->fx_plt = 0;
#if defined(NEED_FX_R_TYPE) || defined (BFD_ASSEMBLER)
  fixP->fx_r_type = r_type;
#endif
  fixP->fx_im_disp = 0;
  fixP->fx_pcrel_adjust = 0;
  fixP->fx_bit_fixP = 0;
  fixP->fx_addnumber = 0;
  fixP->fx_tcbit = 0;
  fixP->fx_done = 0;
  fixP->fx_no_overflow = 0;
  fixP->fx_signed = 0;

#ifdef USING_CGEN
  fixP->fx_cgen.insn = NULL;
  fixP->fx_cgen.opinfo = 0;
#endif

#ifdef TC_FIX_TYPE
  TC_INIT_FIX_DATA (fixP);
#endif

  as_where (&fixP->fx_file, &fixP->fx_line);

  /* Usually, we want relocs sorted numerically, but while
     comparing to older versions of gas that have relocs
     reverse sorted, it is convenient to have this compile
     time option.  xoxorich.  */
  {

#ifdef BFD_ASSEMBLER
    fixS **seg_fix_rootP = (frags_chained
			    ? &seg_info (now_seg)->fix_root
			    : &frchain_now->fix_root);
    fixS **seg_fix_tailP = (frags_chained
			    ? &seg_info (now_seg)->fix_tail
			    : &frchain_now->fix_tail);
#endif

#ifdef REVERSE_SORT_RELOCS

    fixP->fx_next = *seg_fix_rootP;
    *seg_fix_rootP = fixP;

#else /* REVERSE_SORT_RELOCS  */

    fixP->fx_next = NULL;

    if (*seg_fix_tailP)
      (*seg_fix_tailP)->fx_next = fixP;
    else
      *seg_fix_rootP = fixP;
    *seg_fix_tailP = fixP;

#endif /* REVERSE_SORT_RELOCS  */
  }

  return fixP;
}

/* Create a fixup relative to a symbol (plus a constant).  */

fixS *
fix_new (frag, where, size, add_symbol, offset, pcrel, r_type)
     fragS *frag;		/* Which frag?  */
     int where;			/* Where in that frag?  */
     int size;			/* 1, 2, or 4 usually.  */
     symbolS *add_symbol;	/* X_add_symbol.  */
     offsetT offset;		/* X_add_number.  */
     int pcrel;			/* TRUE if PC-relative relocation.  */
#ifdef BFD_ASSEMBLER
     bfd_reloc_code_real_type r_type; /* Relocation type.  */
#else
     int r_type;		/* Relocation type.  */
#endif
{
  return fix_new_internal (frag, where, size, add_symbol,
			   (symbolS *) NULL, offset, pcrel, r_type);
}

/* Create a fixup for an expression.  Currently we only support fixups
   for difference expressions.  That is itself more than most object
   file formats support anyhow.  */

fixS *
fix_new_exp (frag, where, size, exp, pcrel, r_type)
     fragS *frag;		/* Which frag?  */
     int where;			/* Where in that frag?  */
     int size;			/* 1, 2, or 4 usually.  */
     expressionS *exp;		/* Expression.  */
     int pcrel;			/* TRUE if PC-relative relocation.  */
#ifdef BFD_ASSEMBLER
     bfd_reloc_code_real_type r_type; /* Relocation type.  */
#else
     int r_type;		/* Relocation type.  */
#endif
{
  symbolS *add = NULL;
  symbolS *sub = NULL;
  offsetT off = 0;

  switch (exp->X_op)
    {
    case O_absent:
      break;

    case O_register:
      as_bad (_("register value used as expression"));
      break;

    case O_add:
      /* This comes up when _GLOBAL_OFFSET_TABLE_+(.-L0) is read, if
	 the difference expression cannot immediately be reduced.  */
      {
	symbolS *stmp = make_expr_symbol (exp);

	exp->X_op = O_symbol;
	exp->X_op_symbol = 0;
	exp->X_add_symbol = stmp;
	exp->X_add_number = 0;

	return fix_new_exp (frag, where, size, exp, pcrel, r_type);
      }

    case O_symbol_rva:
      add = exp->X_add_symbol;
      off = exp->X_add_number;

#if defined(BFD_ASSEMBLER)
      r_type = BFD_RELOC_RVA;
#else
#if defined(TC_RVA_RELOC)
      r_type = TC_RVA_RELOC;
#else
      as_fatal (_("rva not supported"));
#endif
#endif
      break;

    case O_uminus:
      sub = exp->X_add_symbol;
      off = exp->X_add_number;
      break;

    case O_subtract:
      sub = exp->X_op_symbol;
      /* Fall through.  */
    case O_symbol:
      add = exp->X_add_symbol;
      /* Fall through.  */
    case O_constant:
      off = exp->X_add_number;
      break;

    default:
      add = make_expr_symbol (exp);
      break;
    }

  return fix_new_internal (frag, where, size, add, sub, off, pcrel, r_type);
}

/* Append a string onto another string, bumping the pointer along.  */
void
append (charPP, fromP, length)
     char **charPP;
     char *fromP;
     unsigned long length;
{
  /* Don't trust memcpy() of 0 chars.  */
  if (length == 0)
    return;

  memcpy (*charPP, fromP, length);
  *charPP += length;
}

#ifndef BFD_ASSEMBLER
int section_alignment[SEG_MAXIMUM_ORDINAL];
#endif

/* This routine records the largest alignment seen for each segment.
   If the beginning of the segment is aligned on the worst-case
   boundary, all of the other alignments within it will work.  At
   least one object format really uses this info.  */

void
record_alignment (seg, align)
     /* Segment to which alignment pertains.  */
     segT seg;
     /* Alignment, as a power of 2 (e.g., 1 => 2-byte boundary, 2 => 4-byte
	boundary, etc.)  */
     int align;
{
  if (seg == absolute_section)
    return;
#ifdef BFD_ASSEMBLER
  if ((unsigned int) align > bfd_get_section_alignment (stdoutput, seg))
    bfd_set_section_alignment (stdoutput, seg, align);
#else
  if (align > section_alignment[(int) seg])
    section_alignment[(int) seg] = align;
#endif
}

int
get_recorded_alignment (seg)
     segT seg;
{
  if (seg == absolute_section)
    return 0;
#ifdef BFD_ASSEMBLER
  return bfd_get_section_alignment (stdoutput, seg);
#else
  return section_alignment[(int) seg];
#endif
}

#ifdef BFD_ASSEMBLER

/* Reset the section indices after removing the gas created sections.  */

static void
renumber_sections (abfd, sec, countparg)
     bfd *abfd ATTRIBUTE_UNUSED;
     asection *sec;
     PTR countparg;
{
  int *countp = (int *) countparg;

  sec->index = *countp;
  ++*countp;
}

#endif /* defined (BFD_ASSEMBLER)  */

#if defined (BFD_ASSEMBLER) || ! defined (BFD)

static fragS *
chain_frchains_together_1 (section, frchp)
     segT section;
     struct frchain *frchp;
{
  fragS dummy, *prev_frag = &dummy;
#ifdef BFD_ASSEMBLER
  fixS fix_dummy, *prev_fix = &fix_dummy;
#endif

  for (; frchp && frchp->frch_seg == section; frchp = frchp->frch_next)
    {
      prev_frag->fr_next = frchp->frch_root;
      prev_frag = frchp->frch_last;
      assert (prev_frag->fr_type != 0);
#ifdef BFD_ASSEMBLER
      if (frchp->fix_root != (fixS *) NULL)
	{
	  if (seg_info (section)->fix_root == (fixS *) NULL)
	    seg_info (section)->fix_root = frchp->fix_root;
	  prev_fix->fx_next = frchp->fix_root;
	  seg_info (section)->fix_tail = frchp->fix_tail;
	  prev_fix = frchp->fix_tail;
	}
#endif
    }
  assert (prev_frag->fr_type != 0);
  prev_frag->fr_next = 0;
  return prev_frag;
}

#endif

#ifdef BFD_ASSEMBLER

static void
chain_frchains_together (abfd, section, xxx)
     bfd *abfd ATTRIBUTE_UNUSED;
     segT section;
     PTR xxx ATTRIBUTE_UNUSED;
{
  segment_info_type *info;

  /* BFD may have introduced its own sections without using
     subseg_new, so it is possible that seg_info is NULL.  */
  info = seg_info (section);
  if (info != (segment_info_type *) NULL)
    info->frchainP->frch_last
      = chain_frchains_together_1 (section, info->frchainP);

  /* Now that we've chained the frags together, we must add new fixups
     to the segment, not to the frag chain.  */
  frags_chained = 1;
}

#endif

#if !defined (BFD) && !defined (BFD_ASSEMBLER)

static void
remove_subsegs (head, seg, root, last)
     frchainS *head;
     int seg;
     fragS **root;
     fragS **last;
{
  *root = head->frch_root;
  *last = chain_frchains_together_1 (seg, head);
}

#endif /* BFD  */

#if defined (BFD_ASSEMBLER) || !defined (BFD)

#ifdef BFD_ASSEMBLER
static void
cvt_frag_to_fill (sec, fragP)
     segT sec ATTRIBUTE_UNUSED;
     fragS *fragP;
#else
static void
cvt_frag_to_fill (headersP, sec, fragP)
     object_headers *headersP;
     segT sec;
     fragS *fragP;
#endif
{
  switch (fragP->fr_type)
    {
    case rs_align:
    case rs_align_code:
    case rs_align_test:
    case rs_org:
    case rs_space:
#ifdef HANDLE_ALIGN
      HANDLE_ALIGN (fragP);
#endif
      know (fragP->fr_next != NULL);
      fragP->fr_offset = (fragP->fr_next->fr_address
			  - fragP->fr_address
			  - fragP->fr_fix) / fragP->fr_var;
      if (fragP->fr_offset < 0)
	{
	  as_bad_where (fragP->fr_file, fragP->fr_line,
			_("attempt to .org/.space backwards? (%ld)"),
			(long) fragP->fr_offset);
	}
      fragP->fr_type = rs_fill;
      break;

    case rs_fill:
      break;

    case rs_leb128:
      {
	valueT value = S_GET_VALUE (fragP->fr_symbol);
	int size;

	size = output_leb128 (fragP->fr_literal + fragP->fr_fix, value,
			      fragP->fr_subtype);

	fragP->fr_fix += size;
	fragP->fr_type = rs_fill;
	fragP->fr_var = 0;
	fragP->fr_offset = 0;
	fragP->fr_symbol = NULL;
      }
      break;

    case rs_cfa:
      eh_frame_convert_frag (fragP);
      break;

    case rs_dwarf2dbg:
      dwarf2dbg_convert_frag (fragP);
      break;

    case rs_machine_dependent:
#ifdef BFD_ASSEMBLER
      md_convert_frag (stdoutput, sec, fragP);
#else
      md_convert_frag (headersP, sec, fragP);
#endif

      assert (fragP->fr_next == NULL
	      || ((offsetT) (fragP->fr_next->fr_address - fragP->fr_address)
		  == fragP->fr_fix));

      /* After md_convert_frag, we make the frag into a ".space 0".
	 md_convert_frag() should set up any fixSs and constants
	 required.  */
      frag_wane (fragP);
      break;

#ifndef WORKING_DOT_WORD
    case rs_broken_word:
      {
	struct broken_word *lie;

	if (fragP->fr_subtype)
	  {
	    fragP->fr_fix += md_short_jump_size;
	    for (lie = (struct broken_word *) (fragP->fr_symbol);
		 lie && lie->dispfrag == fragP;
		 lie = lie->next_broken_word)
	      if (lie->added == 1)
		fragP->fr_fix += md_long_jump_size;
	  }
	frag_wane (fragP);
      }
      break;
#endif

    default:
      BAD_CASE (fragP->fr_type);
      break;
    }
}

#endif /* defined (BFD_ASSEMBLER) || !defined (BFD)  */

#ifdef BFD_ASSEMBLER
static void
relax_seg (abfd, sec, do_code)
     bfd *abfd;
     asection *sec;
     PTR do_code;
{
  flagword flags = bfd_get_section_flags (abfd, sec);
  segment_info_type *seginfo = seg_info (sec);

  if (!(flags & SEC_CODE) == !do_code
      && seginfo && seginfo->frchainP)
    relax_segment (seginfo->frchainP->frch_root, sec);
}

static void
size_seg (abfd, sec, xxx)
     bfd *abfd;
     asection *sec;
     PTR xxx ATTRIBUTE_UNUSED;
{
  flagword flags;
  fragS *fragp;
  segment_info_type *seginfo;
  int x;
  valueT size, newsize;

  subseg_change (sec, 0);

  seginfo = seg_info (sec);
  if (seginfo && seginfo->frchainP)
    {
      for (fragp = seginfo->frchainP->frch_root; fragp; fragp = fragp->fr_next)
	cvt_frag_to_fill (sec, fragp);
      for (fragp = seginfo->frchainP->frch_root;
	   fragp->fr_next;
	   fragp = fragp->fr_next)
	/* Walk to last elt.  */
	;
      size = fragp->fr_address + fragp->fr_fix;
    }
  else
    size = 0;

  flags = bfd_get_section_flags (abfd, sec);

  if (size > 0 && ! seginfo->bss)
    flags |= SEC_HAS_CONTENTS;

  /* @@ This is just an approximation.  */
  if (seginfo && seginfo->fix_root)
    flags |= SEC_RELOC;
  else
    flags &= ~SEC_RELOC;
  x = bfd_set_section_flags (abfd, sec, flags);
  assert (x == true);

  newsize = md_section_align (sec, size);
  x = bfd_set_section_size (abfd, sec, newsize);
  assert (x == true);

  /* If the size had to be rounded up, add some padding in the last
     non-empty frag.  */
  assert (newsize >= size);
  if (size != newsize)
    {
      fragS *last = seginfo->frchainP->frch_last;
      fragp = seginfo->frchainP->frch_root;
      while (fragp->fr_next != last)
	fragp = fragp->fr_next;
      last->fr_address = size;
      fragp->fr_offset += newsize - size;
    }

#ifdef tc_frob_section
  tc_frob_section (sec);
#endif
#ifdef obj_frob_section
  obj_frob_section (sec);
#endif
}

#ifdef DEBUG2
static void
dump_section_relocs (abfd, sec, stream_)
     bfd *abfd ATTRIBUTE_UNUSED;
     asection *sec;
     char *stream_;
{
  FILE *stream = (FILE *) stream_;
  segment_info_type *seginfo = seg_info (sec);
  fixS *fixp = seginfo->fix_root;

  if (!fixp)
    return;

  fprintf (stream, "sec %s relocs:\n", sec->name);
  while (fixp)
    {
      symbolS *s = fixp->fx_addsy;

      fprintf (stream, "  %08lx: type %d ", (unsigned long) fixp,
	       (int) fixp->fx_r_type);
      if (s == NULL)
	fprintf (stream, "no sym\n");
      else
	{
	  print_symbol_value_1 (stream, s);
	  fprintf (stream, "\n");
	}
      fixp = fixp->fx_next;
    }
}
#else
#define dump_section_relocs(ABFD,SEC,STREAM)	((void) 0)
#endif

#ifndef EMIT_SECTION_SYMBOLS
#define EMIT_SECTION_SYMBOLS 1
#endif

static void
adjust_reloc_syms (abfd, sec, xxx)
     bfd *abfd ATTRIBUTE_UNUSED;
     asection *sec;
     PTR xxx ATTRIBUTE_UNUSED;
{
  segment_info_type *seginfo = seg_info (sec);
  fixS *fixp;

  if (seginfo == NULL)
    return;

  dump_section_relocs (abfd, sec, stderr);

  for (fixp = seginfo->fix_root; fixp; fixp = fixp->fx_next)
    if (fixp->fx_done)
      /* Ignore it.  */
      ;
    else if (fixp->fx_addsy)
      {
	symbolS *sym;
	asection *symsec;

#ifdef DEBUG5
	fprintf (stderr, "\n\nadjusting fixup:\n");
	print_fixup (fixp);
#endif

	sym = fixp->fx_addsy;

	/* All symbols should have already been resolved at this
	   point.  It is possible to see unresolved expression
	   symbols, though, since they are not in the regular symbol
	   table.  */
	if (sym != NULL)
	  resolve_symbol_value (sym, 1);

	if (fixp->fx_subsy != NULL)
	  resolve_symbol_value (fixp->fx_subsy, 1);

	/* If this symbol is equated to an undefined symbol, convert
           the fixup to being against that symbol.  */
	if (sym != NULL && symbol_equated_p (sym)
	    && (! S_IS_DEFINED (sym) || S_IS_COMMON (sym)))
	  {
	    fixp->fx_offset += symbol_get_value_expression (sym)->X_add_number;
	    sym = symbol_get_value_expression (sym)->X_add_symbol;
	    fixp->fx_addsy = sym;
	  }

	if (sym != NULL && symbol_mri_common_p (sym))
	  {
	    /* These symbols are handled specially in fixup_segment.  */
	    goto done;
	  }

	symsec = S_GET_SEGMENT (sym);

	if (symsec == NULL)
	  abort ();

	if (bfd_is_abs_section (symsec))
	  {
	    /* The fixup_segment routine will not use this symbol in a
               relocation unless TC_FORCE_RELOCATION returns 1.  */
	    if (TC_FORCE_RELOCATION (fixp))
	      {
		symbol_mark_used_in_reloc (fixp->fx_addsy);
#ifdef UNDEFINED_DIFFERENCE_OK
		if (fixp->fx_subsy != NULL)
		  symbol_mark_used_in_reloc (fixp->fx_subsy);
#endif
	      }
	    goto done;
	  }

	/* If it's one of these sections, assume the symbol is
	   definitely going to be output.  The code in
	   md_estimate_size_before_relax in tc-mips.c uses this test
	   as well, so if you change this code you should look at that
	   code.  */
	if (bfd_is_und_section (symsec)
	    || bfd_is_com_section (symsec))
	  {
	    symbol_mark_used_in_reloc (fixp->fx_addsy);
#ifdef UNDEFINED_DIFFERENCE_OK
	    /* We have the difference of an undefined symbol and some
	       other symbol.  Make sure to mark the other symbol as used
	       in a relocation so that it will always be output.  */
	    if (fixp->fx_subsy)
	      symbol_mark_used_in_reloc (fixp->fx_subsy);
#endif
	    goto done;
	  }

	/* Don't try to reduce relocs which refer to non-local symbols
           in .linkonce sections.  It can lead to confusion when a
           debugging section refers to a .linkonce section.  I hope
           this will always be correct.  */
	if (symsec != sec && ! S_IS_LOCAL (sym))
	  {
	    boolean linkonce;

	    linkonce = false;
#ifdef BFD_ASSEMBLER
	    if ((bfd_get_section_flags (stdoutput, symsec) & SEC_LINK_ONCE)
		!= 0)
	      linkonce = true;
#endif
#ifdef OBJ_ELF
	    /* The GNU toolchain uses an extension for ELF: a section
               beginning with the magic string .gnu.linkonce is a
               linkonce section.  */
	    if (strncmp (segment_name (symsec), ".gnu.linkonce",
			 sizeof ".gnu.linkonce" - 1) == 0)
	      linkonce = true;
#endif

	    if (linkonce)
	      {
		symbol_mark_used_in_reloc (fixp->fx_addsy);
#ifdef UNDEFINED_DIFFERENCE_OK
		if (fixp->fx_subsy != NULL)
		  symbol_mark_used_in_reloc (fixp->fx_subsy);
#endif
		goto done;
	      }
	  }

	/* Since we're reducing to section symbols, don't attempt to reduce
	   anything that's already using one.  */
	if (symbol_section_p (sym))
	  {
	    symbol_mark_used_in_reloc (fixp->fx_addsy);
	    goto done;
	  }

#ifdef BFD_ASSEMBLER
	/* We can never adjust a reloc against a weak symbol.  If we
           did, and the weak symbol was overridden by a real symbol
           somewhere else, then our relocation would be pointing at
           the wrong area of memory.  */
	if (S_IS_WEAK (sym))
	  {
	    symbol_mark_used_in_reloc (fixp->fx_addsy);
	    goto done;
	  }
#endif

	/* Is there some other reason we can't adjust this one?  (E.g.,
	   call/bal links in i960-bout symbols.)  */
#ifdef obj_fix_adjustable
	if (! obj_fix_adjustable (fixp))
	  {
	    symbol_mark_used_in_reloc (fixp->fx_addsy);
	    goto done;
	  }
#endif

	/* Is there some other (target cpu dependent) reason we can't adjust
	   this one?  (E.g. relocations involving function addresses on
	   the PA.  */
#ifdef tc_fix_adjustable
	if (! tc_fix_adjustable (fixp))
	  {
	    symbol_mark_used_in_reloc (fixp->fx_addsy);
	    goto done;
	  }
#endif

	/* If the section symbol isn't going to be output, the relocs
	   at least should still work.  If not, figure out what to do
	   when we run into that case.

	   We refetch the segment when calling section_symbol, rather
	   than using symsec, because S_GET_VALUE may wind up changing
	   the section when it calls resolve_symbol_value.  */
	fixp->fx_offset += S_GET_VALUE (sym);
	fixp->fx_addsy = section_symbol (S_GET_SEGMENT (sym));
	symbol_mark_used_in_reloc (fixp->fx_addsy);
#ifdef DEBUG5
	fprintf (stderr, "\nadjusted fixup:\n");
	print_fixup (fixp);
#endif

      done:
	;
      }
#if 1 /* def RELOC_REQUIRES_SYMBOL  */
    else
      {
	/* There was no symbol required by this relocation.  However,
	   BFD doesn't really handle relocations without symbols well.
	   (At least, the COFF support doesn't.)  So for now we fake up
	   a local symbol in the absolute section.  */

	fixp->fx_addsy = section_symbol (absolute_section);
#if 0
	fixp->fx_addsy->sy_used_in_reloc = 1;
#endif
      }
#endif

  dump_section_relocs (abfd, sec, stderr);
}

static void
write_relocs (abfd, sec, xxx)
     bfd *abfd;
     asection *sec;
     PTR xxx ATTRIBUTE_UNUSED;
{
  segment_info_type *seginfo = seg_info (sec);
  unsigned int i;
  unsigned int n;
  arelent **relocs;
  fixS *fixp;
  char *err;

  /* If seginfo is NULL, we did not create this section; don't do
     anything with it.  */
  if (seginfo == NULL)
    return;

  fixup_segment (seginfo->fix_root, sec);

  n = 0;
  for (fixp = seginfo->fix_root; fixp; fixp = fixp->fx_next)
    n++;

#ifndef RELOC_EXPANSION_POSSIBLE
  /* Set up reloc information as well.  */
  relocs = (arelent **) xmalloc (n * sizeof (arelent *));
  memset ((char *) relocs, 0, n * sizeof (arelent *));

  i = 0;
  for (fixp = seginfo->fix_root; fixp != (fixS *) NULL; fixp = fixp->fx_next)
    {
      arelent *reloc;
      bfd_reloc_status_type s;
      symbolS *sym;

      if (fixp->fx_done)
	{
	  n--;
	  continue;
	}

      /* If this is an undefined symbol which was equated to another
         symbol, then use generate the reloc against the latter symbol
         rather than the former.  */
      sym = fixp->fx_addsy;
      while (symbol_equated_p (sym)
	     && (! S_IS_DEFINED (sym) || S_IS_COMMON (sym)))
	{
	  symbolS *n;

	  /* We must avoid looping, as that can occur with a badly
	     written program.  */
	  n = symbol_get_value_expression (sym)->X_add_symbol;
	  if (n == sym)
	    break;
	  fixp->fx_offset += symbol_get_value_expression (sym)->X_add_number;
	  sym = n;
	}
      fixp->fx_addsy = sym;

      reloc = tc_gen_reloc (sec, fixp);
      if (!reloc)
	{
	  n--;
	  continue;
	}

#if 0
      /* This test is triggered inappropriately for the SH.  */
      if (fixp->fx_where + fixp->fx_size
	  > fixp->fx_frag->fr_fix + fixp->fx_frag->fr_offset)
	abort ();
#endif

      s = bfd_install_relocation (stdoutput, reloc,
				  fixp->fx_frag->fr_literal,
				  fixp->fx_frag->fr_address,
				  sec, &err);
      switch (s)
	{
	case bfd_reloc_ok:
	  break;
	case bfd_reloc_overflow:
	  as_bad_where (fixp->fx_file, fixp->fx_line, _("relocation overflow"));
	  break;
	case bfd_reloc_outofrange:
	  as_bad_where (fixp->fx_file, fixp->fx_line, _("relocation out of range"));
	  break;
	default:
	  as_fatal (_("%s:%u: bad return from bfd_install_relocation: %x"),
		    fixp->fx_file, fixp->fx_line, s);
	}
      relocs[i++] = reloc;
    }
#else
  n = n * MAX_RELOC_EXPANSION;
  /* Set up reloc information as well.  */
  relocs = (arelent **) xmalloc (n * sizeof (arelent *));

  i = 0;
  for (fixp = seginfo->fix_root; fixp != (fixS *) NULL; fixp = fixp->fx_next)
    {
      arelent **reloc;
      char *data;
      bfd_reloc_status_type s;
      symbolS *sym;
      int j;

      if (fixp->fx_done)
	{
	  n--;
	  continue;
	}

      /* If this is an undefined symbol which was equated to another
         symbol, then generate the reloc against the latter symbol
         rather than the former.  */
      sym = fixp->fx_addsy;
      while (symbol_equated_p (sym)
	     && (! S_IS_DEFINED (sym) || S_IS_COMMON (sym)))
	sym = symbol_get_value_expression (sym)->X_add_symbol;
      fixp->fx_addsy = sym;

      reloc = tc_gen_reloc (sec, fixp);

      for (j = 0; reloc[j]; j++)
	{
	  relocs[i++] = reloc[j];
	  assert (i <= n);
	}
      data = fixp->fx_frag->fr_literal + fixp->fx_where;
      if (fixp->fx_where + fixp->fx_size
	  > fixp->fx_frag->fr_fix + fixp->fx_frag->fr_offset)
	as_bad_where (fixp->fx_file, fixp->fx_line,
		      _("internal error: fixup not contained within frag"));
      for (j = 0; reloc[j]; j++)
	{
	  s = bfd_install_relocation (stdoutput, reloc[j],
				      fixp->fx_frag->fr_literal,
				      fixp->fx_frag->fr_address,
				      sec, &err);
	  switch (s)
	    {
	    case bfd_reloc_ok:
	      break;
	    case bfd_reloc_overflow:
	      as_bad_where (fixp->fx_file, fixp->fx_line,
			    _("relocation overflow"));
	      break;
	    default:
	      as_fatal (_("%s:%u: bad return from bfd_install_relocation"),
			fixp->fx_file, fixp->fx_line);
	    }
	}
    }
  n = i;
#endif

#ifdef DEBUG4
  {
    int i, j, nsyms;
    asymbol **sympp;
    sympp = bfd_get_outsymbols (stdoutput);
    nsyms = bfd_get_symcount (stdoutput);
    for (i = 0; i < n; i++)
      if (((*relocs[i]->sym_ptr_ptr)->flags & BSF_SECTION_SYM) == 0)
	{
	  for (j = 0; j < nsyms; j++)
	    if (sympp[j] == *relocs[i]->sym_ptr_ptr)
	      break;
	  if (j == nsyms)
	    abort ();
	}
  }
#endif

  if (n)
    bfd_set_reloc (stdoutput, sec, relocs, n);
  else
    bfd_set_section_flags (abfd, sec,
			   (bfd_get_section_flags (abfd, sec)
			    & (flagword) ~SEC_RELOC));

#ifdef SET_SECTION_RELOCS
  SET_SECTION_RELOCS (sec, relocs, n);
#endif

#ifdef DEBUG3
  {
    int i;
    arelent *r;
    asymbol *s;
    fprintf (stderr, "relocs for sec %s\n", sec->name);
    for (i = 0; i < n; i++)
      {
	r = relocs[i];
	s = *r->sym_ptr_ptr;
	fprintf (stderr, "  reloc %2d @%08x off %4x : sym %-10s addend %x\n",
		 i, r, r->address, s->name, r->addend);
      }
  }
#endif
}

static void
write_contents (abfd, sec, xxx)
     bfd *abfd ATTRIBUTE_UNUSED;
     asection *sec;
     PTR xxx ATTRIBUTE_UNUSED;
{
  segment_info_type *seginfo = seg_info (sec);
  unsigned long offset = 0;
  fragS *f;

  /* Write out the frags.  */
  if (seginfo == NULL
      || !(bfd_get_section_flags (abfd, sec) & SEC_HAS_CONTENTS))
    return;

  for (f = seginfo->frchainP->frch_root;
       f;
       f = f->fr_next)
    {
      int x;
      unsigned long fill_size;
      char *fill_literal;
      long count;

      assert (f->fr_type == rs_fill);
      if (f->fr_fix)
	{
	  x = bfd_set_section_contents (stdoutput, sec,
					f->fr_literal, (file_ptr) offset,
					(bfd_size_type) f->fr_fix);
	  if (x == false)
	    {
	      bfd_perror (stdoutput->filename);
	      as_perror (_("FATAL: Can't write %s"), stdoutput->filename);
	      exit (EXIT_FAILURE);
	    }
	  offset += f->fr_fix;
	}
      fill_literal = f->fr_literal + f->fr_fix;
      fill_size = f->fr_var;
      count = f->fr_offset;
      assert (count >= 0);
      if (fill_size && count)
	{
	  char buf[256];
	  if (fill_size > sizeof (buf))
	    {
	      /* Do it the old way. Can this ever happen?  */
	      while (count--)
		{
		  x = bfd_set_section_contents (stdoutput, sec,
						fill_literal,
						(file_ptr) offset,
						(bfd_size_type) fill_size);
		  if (x == false)
		    {
		      bfd_perror (stdoutput->filename);
		      as_perror (_("FATAL: Can't write %s"),
				 stdoutput->filename);
		      exit (EXIT_FAILURE);
		    }
		  offset += fill_size;
		}
	    }
	  else
	    {
	      /* Build a buffer full of fill objects and output it as
		 often as necessary. This saves on the overhead of
		 potentially lots of bfd_set_section_contents calls.  */
	      int n_per_buf, i;
	      if (fill_size == 1)
		{
		  n_per_buf = sizeof (buf);
		  memset (buf, *fill_literal, n_per_buf);
		}
	      else
		{
		  char *bufp;
		  n_per_buf = sizeof (buf) / fill_size;
		  for (i = n_per_buf, bufp = buf; i; i--, bufp += fill_size)
		    memcpy (bufp, fill_literal, fill_size);
		}
	      for (; count > 0; count -= n_per_buf)
		{
		  n_per_buf = n_per_buf > count ? count : n_per_buf;
		  x = bfd_set_section_contents
		    (stdoutput, sec, buf, (file_ptr) offset,
		     (bfd_size_type) n_per_buf * fill_size);
		  if (x != true)
		    as_fatal (_("Cannot write to output file."));
		  offset += n_per_buf * fill_size;
		}
	    }
	}
    }
}
#endif

#if defined(BFD_ASSEMBLER) || (!defined (BFD) && !defined(OBJ_AOUT))
static void
merge_data_into_text ()
{
#if defined(BFD_ASSEMBLER) || defined(MANY_SEGMENTS)
  seg_info (text_section)->frchainP->frch_last->fr_next =
    seg_info (data_section)->frchainP->frch_root;
  seg_info (text_section)->frchainP->frch_last =
    seg_info (data_section)->frchainP->frch_last;
  seg_info (data_section)->frchainP = 0;
#else
  fixS *tmp;

  text_last_frag->fr_next = data_frag_root;
  text_last_frag = data_last_frag;
  data_last_frag = NULL;
  data_frag_root = NULL;
  if (text_fix_root)
    {
      for (tmp = text_fix_root; tmp->fx_next; tmp = tmp->fx_next);;
      tmp->fx_next = data_fix_root;
      text_fix_tail = data_fix_tail;
    }
  else
    text_fix_root = data_fix_root;
  data_fix_root = NULL;
#endif
}
#endif /* BFD_ASSEMBLER || (! BFD && ! OBJ_AOUT)  */

#if !defined (BFD_ASSEMBLER) && !defined (BFD)
static void
relax_and_size_all_segments ()
{
  fragS *fragP;

  relax_segment (text_frag_root, SEG_TEXT);
  relax_segment (data_frag_root, SEG_DATA);
  relax_segment (bss_frag_root, SEG_BSS);

  /* Now the addresses of frags are correct within the segment.  */
  know (text_last_frag->fr_type == rs_fill && text_last_frag->fr_offset == 0);
  H_SET_TEXT_SIZE (&headers, text_last_frag->fr_address);
  text_last_frag->fr_address = H_GET_TEXT_SIZE (&headers);

  /* Join the 2 segments into 1 huge segment.
     To do this, re-compute every rn_address in the SEG_DATA frags.
     Then join the data frags after the text frags.

     Determine a_data [length of data segment].  */
  if (data_frag_root)
    {
      register relax_addressT slide;

      know ((text_last_frag->fr_type == rs_fill)
	    && (text_last_frag->fr_offset == 0));

      H_SET_DATA_SIZE (&headers, data_last_frag->fr_address);
      data_last_frag->fr_address = H_GET_DATA_SIZE (&headers);
      slide = H_GET_TEXT_SIZE (&headers);	/* & in file of the data segment.  */
#ifdef OBJ_BOUT
#define RoundUp(N,S) (((N)+(S)-1)&-(S))
      /* For b.out: If the data section has a strict alignment
	 requirement, its load address in the .o file will be
	 rounded up from the size of the text section.  These
	 two values are *not* the same!  Similarly for the bss
	 section....  */
      slide = RoundUp (slide, 1 << section_alignment[SEG_DATA]);
#endif

      for (fragP = data_frag_root; fragP; fragP = fragP->fr_next)
	fragP->fr_address += slide;

      know (text_last_frag != 0);
      text_last_frag->fr_next = data_frag_root;
    }
  else
    {
      H_SET_DATA_SIZE (&headers, 0);
    }

#ifdef OBJ_BOUT
  /* See above comments on b.out data section address.  */
  {
    long bss_vma;
    if (data_last_frag == 0)
      bss_vma = H_GET_TEXT_SIZE (&headers);
    else
      bss_vma = data_last_frag->fr_address;
    bss_vma = RoundUp (bss_vma, 1 << section_alignment[SEG_BSS]);
    bss_address_frag.fr_address = bss_vma;
  }
#else /* ! OBJ_BOUT  */
  bss_address_frag.fr_address = (H_GET_TEXT_SIZE (&headers) +
				 H_GET_DATA_SIZE (&headers));

#endif /* ! OBJ_BOUT  */

  /* Slide all the frags.  */
  if (bss_frag_root)
    {
      relax_addressT slide = bss_address_frag.fr_address;

      for (fragP = bss_frag_root; fragP; fragP = fragP->fr_next)
	fragP->fr_address += slide;
    }

  if (bss_last_frag)
    H_SET_BSS_SIZE (&headers,
		    bss_last_frag->fr_address - bss_frag_root->fr_address);
  else
    H_SET_BSS_SIZE (&headers, 0);
}
#endif /* ! BFD_ASSEMBLER && ! BFD  */

#if defined (BFD_ASSEMBLER) || !defined (BFD)

#ifdef BFD_ASSEMBLER
static void
set_symtab ()
{
  int nsyms;
  asymbol **asympp;
  symbolS *symp;
  boolean result;
  extern PTR bfd_alloc PARAMS ((bfd *, size_t));

  /* Count symbols.  We can't rely on a count made by the loop in
     write_object_file, because *_frob_file may add a new symbol or
     two.  */
  nsyms = 0;
  for (symp = symbol_rootP; symp; symp = symbol_next (symp))
    nsyms++;

  if (nsyms)
    {
      int i;

      asympp = (asymbol **) bfd_alloc (stdoutput,
				       nsyms * sizeof (asymbol *));
      symp = symbol_rootP;
      for (i = 0; i < nsyms; i++, symp = symbol_next (symp))
	{
	  asympp[i] = symbol_get_bfdsym (symp);
	  symbol_mark_written (symp);
	}
    }
  else
    asympp = 0;
  result = bfd_set_symtab (stdoutput, asympp, nsyms);
  assert (result == true);
  symbol_table_frozen = 1;
}
#endif

#if defined (BFD_ASSEMBLER) && defined (OBJ_COFF) && defined (TE_GO32)
static void
set_segment_vma (abfd, sec, xxx)
     bfd *abfd;
     asection *sec;
     PTR xxx ATTRIBUTE_UNUSED;
{
  static bfd_vma addr = 0;

  bfd_set_section_vma (abfd, sec, addr);
  addr += bfd_section_size (abfd, sec);
}
#endif /* BFD_ASSEMBLER && OBJ_COFF && !TE_PE  */

/* Finish the subsegments.  After every sub-segment, we fake an
   ".align ...".  This conforms to BSD4.2 brane-damage.  We then fake
   ".fill 0" because that is the kind of frag that requires least
   thought.  ".align" frags like to have a following frag since that
   makes calculating their intended length trivial.  */

#ifndef SUB_SEGMENT_ALIGN
#ifdef BFD_ASSEMBLER
#define SUB_SEGMENT_ALIGN(SEG) (0)
#else
#define SUB_SEGMENT_ALIGN(SEG) (2)
#endif
#endif

void
subsegs_finish ()
{
  struct frchain *frchainP;

  for (frchainP = frchain_root; frchainP; frchainP = frchainP->frch_next)
    {
      int alignment;

      subseg_set (frchainP->frch_seg, frchainP->frch_subseg);

      /* This now gets called even if we had errors.  In that case,
         any alignment is meaningless, and, moreover, will look weird
         if we are generating a listing.  */
      alignment = had_errors () ? 0 : SUB_SEGMENT_ALIGN (now_seg);

      /* The last subsegment gets an aligment corresponding to the
	 alignment of the section.  This allows proper nop-filling
	 at the end of code-bearing sections.  */
      if (!frchainP->frch_next || frchainP->frch_next->frch_seg != now_seg)
	alignment = get_recorded_alignment (now_seg);

      if (subseg_text_p (now_seg))
	frag_align_code (alignment, 0);
      else
	frag_align (alignment, 0, 0);

      /* frag_align will have left a new frag.
	 Use this last frag for an empty ".fill".

	 For this segment ...
	 Create a last frag. Do not leave a "being filled in frag".  */
      frag_wane (frag_now);
      frag_now->fr_fix = 0;
      know (frag_now->fr_next == NULL);
    }
}

/* Write the object file.  */

void
write_object_file ()
{
#if ! defined (BFD_ASSEMBLER) || ! defined (WORKING_DOT_WORD)
  fragS *fragP;			/* Track along all frags.  */
#endif

  /* Do we really want to write it?  */
  {
    int n_warns, n_errs;
    n_warns = had_warnings ();
    n_errs = had_errors ();
    /* The -Z flag indicates that an object file should be generated,
       regardless of warnings and errors.  */
    if (flag_always_generate_output)
      {
	if (n_warns || n_errs)
	  as_warn (_("%d error%s, %d warning%s, generating bad object file.\n"),
		   n_errs, n_errs == 1 ? "" : "s",
		   n_warns, n_warns == 1 ? "" : "s");
      }
    else
      {
	if (n_errs)
	  as_fatal (_("%d error%s, %d warning%s, no object file generated.\n"),
		    n_errs, n_errs == 1 ? "" : "s",
		    n_warns, n_warns == 1 ? "" : "s");
      }
  }

#ifdef	OBJ_VMS
  /* Under VMS we try to be compatible with VAX-11 "C".  Thus, we call
     a routine to check for the definition of the procedure "_main",
     and if so -- fix it up so that it can be program entry point.  */
  vms_check_for_main ();
#endif /* OBJ_VMS  */

  /* From now on, we don't care about sub-segments.  Build one frag chain
     for each segment. Linked thru fr_next.  */

#ifdef BFD_ASSEMBLER
  /* Remove the sections created by gas for its own purposes.  */
  {
    asection **seclist, *sec;
    int i;

    seclist = &stdoutput->sections;
    while (seclist && *seclist)
      {
	sec = *seclist;
	while (sec == reg_section || sec == expr_section)
	  {
	    sec = sec->next;
	    *seclist = sec;
	    stdoutput->section_count--;
	    if (!sec)
	      break;
	  }
	if (*seclist)
	  seclist = &(*seclist)->next;
      }
    i = 0;
    bfd_map_over_sections (stdoutput, renumber_sections, &i);
  }

  bfd_map_over_sections (stdoutput, chain_frchains_together, (char *) 0);
#else
  remove_subsegs (frchain_root, SEG_TEXT, &text_frag_root, &text_last_frag);
  remove_subsegs (data0_frchainP, SEG_DATA, &data_frag_root, &data_last_frag);
  remove_subsegs (bss0_frchainP, SEG_BSS, &bss_frag_root, &bss_last_frag);
#endif

  /* We have two segments. If user gave -R flag, then we must put the
     data frags into the text segment. Do this before relaxing so
     we know to take advantage of -R and make shorter addresses.  */
#if !defined (OBJ_AOUT) || defined (BFD_ASSEMBLER)
  if (flag_readonly_data_in_text)
    {
      merge_data_into_text ();
    }
#endif

#ifdef BFD_ASSEMBLER
  bfd_map_over_sections (stdoutput, relax_seg, (char *) 1);
  bfd_map_over_sections (stdoutput, relax_seg, (char *) 0);
  bfd_map_over_sections (stdoutput, size_seg, (char *) 0);
#else
  relax_and_size_all_segments ();
#endif /* BFD_ASSEMBLER  */

#if defined (BFD_ASSEMBLER) && defined (OBJ_COFF) && defined (TE_GO32)
  /* Now that the segments have their final sizes, run through the
     sections and set their vma and lma. !BFD gas sets them, and BFD gas
     should too. Currently, only DJGPP uses this code, but other
     COFF targets may need to execute this too.  */
  bfd_map_over_sections (stdoutput, set_segment_vma, (char *) 0);
#endif

#ifndef BFD_ASSEMBLER
  /* Crawl the symbol chain.

     For each symbol whose value depends on a frag, take the address of
     that frag and subsume it into the value of the symbol.
     After this, there is just one way to lookup a symbol value.
     Values are left in their final state for object file emission.
     We adjust the values of 'L' local symbols, even if we do
     not intend to emit them to the object file, because their values
     are needed for fix-ups.

     Unless we saw a -L flag, remove all symbols that begin with 'L'
     from the symbol chain.  (They are still pointed to by the fixes.)

     Count the remaining symbols.
     Assign a symbol number to each symbol.
     Count the number of string-table chars we will emit.
     Put this info into the headers as appropriate.  */
  know (zero_address_frag.fr_address == 0);
  string_byte_count = sizeof (string_byte_count);

  obj_crawl_symbol_chain (&headers);

  if (string_byte_count == sizeof (string_byte_count))
    string_byte_count = 0;

  H_SET_STRING_SIZE (&headers, string_byte_count);

  /* Addresses of frags now reflect addresses we use in the object file.
     Symbol values are correct.
     Scan the frags, converting any ".org"s and ".align"s to ".fill"s.
     Also converting any machine-dependent frags using md_convert_frag();  */
  subseg_change (SEG_TEXT, 0);

  for (fragP = text_frag_root; fragP; fragP = fragP->fr_next)
    {
      /* At this point we have linked all the frags into a single
         chain.  However, cvt_frag_to_fill may call md_convert_frag
         which may call fix_new.  We need to ensure that fix_new adds
         the fixup to the right section.  */
      if (fragP == data_frag_root)
	subseg_change (SEG_DATA, 0);

      cvt_frag_to_fill (&headers, SEG_TEXT, fragP);

      /* Some assert macros don't work with # directives mixed in.  */
#ifndef NDEBUG
      if (!(fragP->fr_next == NULL
#ifdef OBJ_BOUT
	    || fragP->fr_next == data_frag_root
#endif
	    || ((fragP->fr_next->fr_address - fragP->fr_address)
		== (fragP->fr_fix + fragP->fr_offset * fragP->fr_var))))
	abort ();
#endif
    }
#endif /* ! BFD_ASSEMBLER  */

#ifndef WORKING_DOT_WORD
  {
    struct broken_word *lie;
    struct broken_word **prevP;

    prevP = &broken_words;
    for (lie = broken_words; lie; lie = lie->next_broken_word)
      if (!lie->added)
	{
	  expressionS exp;

	  subseg_change (lie->seg, lie->subseg);
	  exp.X_op = O_subtract;
	  exp.X_add_symbol = lie->add;
	  exp.X_op_symbol = lie->sub;
	  exp.X_add_number = lie->addnum;
#ifdef BFD_ASSEMBLER
#ifdef TC_CONS_FIX_NEW
	  TC_CONS_FIX_NEW (lie->frag,
			   lie->word_goes_here - lie->frag->fr_literal,
			   2, &exp);
#else
	  fix_new_exp (lie->frag,
		       lie->word_goes_here - lie->frag->fr_literal,
		       2, &exp, 0, BFD_RELOC_16);
#endif
#else
#if defined(TC_SPARC) || defined(TC_A29K) || defined(NEED_FX_R_TYPE)
	  fix_new_exp (lie->frag,
		       lie->word_goes_here - lie->frag->fr_literal,
		       2, &exp, 0, NO_RELOC);
#else
#ifdef TC_NS32K
	  fix_new_ns32k_exp (lie->frag,
			     lie->word_goes_here - lie->frag->fr_literal,
			     2, &exp, 0, 0, 2, 0, 0);
#else
	  fix_new_exp (lie->frag,
		       lie->word_goes_here - lie->frag->fr_literal,
		       2, &exp, 0, 0);
#endif /* TC_NS32K  */
#endif /* TC_SPARC|TC_A29K|NEED_FX_R_TYPE  */
#endif /* BFD_ASSEMBLER  */
	  *prevP = lie->next_broken_word;
	}
      else
	prevP = &(lie->next_broken_word);

    for (lie = broken_words; lie;)
      {
	struct broken_word *untruth;
	char *table_ptr;
	addressT table_addr;
	addressT from_addr, to_addr;
	int n, m;

	subseg_change (lie->seg, lie->subseg);
	fragP = lie->dispfrag;

	/* Find out how many broken_words go here.  */
	n = 0;
	for (untruth = lie;
	     untruth && untruth->dispfrag == fragP;
	     untruth = untruth->next_broken_word)
	  if (untruth->added == 1)
	    n++;

	table_ptr = lie->dispfrag->fr_opcode;
	table_addr = (lie->dispfrag->fr_address
		      + (table_ptr - lie->dispfrag->fr_literal));
	/* Create the jump around the long jumps.  This is a short
	   jump from table_ptr+0 to table_ptr+n*long_jump_size.  */
	from_addr = table_addr;
	to_addr = table_addr + md_short_jump_size + n * md_long_jump_size;
	md_create_short_jump (table_ptr, from_addr, to_addr, lie->dispfrag,
			      lie->add);
	table_ptr += md_short_jump_size;
	table_addr += md_short_jump_size;

	for (m = 0;
	     lie && lie->dispfrag == fragP;
	     m++, lie = lie->next_broken_word)
	  {
	    if (lie->added == 2)
	      continue;
	    /* Patch the jump table.  */
	    /* This is the offset from ??? to table_ptr+0.  */
	    to_addr = table_addr - S_GET_VALUE (lie->sub);
#ifdef BFD_ASSEMBLER
	    to_addr -= symbol_get_frag (lie->sub)->fr_address;
#endif
#ifdef TC_CHECK_ADJUSTED_BROKEN_DOT_WORD
	    TC_CHECK_ADJUSTED_BROKEN_DOT_WORD (to_addr, lie);
#endif
	    md_number_to_chars (lie->word_goes_here, to_addr, 2);
	    for (untruth = lie->next_broken_word;
		 untruth && untruth->dispfrag == fragP;
		 untruth = untruth->next_broken_word)
	      {
		if (untruth->use_jump == lie)
		  md_number_to_chars (untruth->word_goes_here, to_addr, 2);
	      }

	    /* Install the long jump.  */
	    /* This is a long jump from table_ptr+0 to the final target.  */
	    from_addr = table_addr;
	    to_addr = S_GET_VALUE (lie->add) + lie->addnum;
#ifdef BFD_ASSEMBLER
	    to_addr += symbol_get_frag (lie->add)->fr_address;
#endif
	    md_create_long_jump (table_ptr, from_addr, to_addr, lie->dispfrag,
				 lie->add);
	    table_ptr += md_long_jump_size;
	    table_addr += md_long_jump_size;
	  }
      }
  }
#endif /* not WORKING_DOT_WORD  */

#ifndef BFD_ASSEMBLER
#ifndef	OBJ_VMS
  {				/* not vms  */
    char *the_object_file;
    long object_file_size;
    /* Scan every FixS performing fixups. We had to wait until now to
       do this because md_convert_frag() may have made some fixSs.  */
    int trsize, drsize;

    subseg_change (SEG_TEXT, 0);
    trsize = md_reloc_size * fixup_segment (text_fix_root, SEG_TEXT);
    subseg_change (SEG_DATA, 0);
    drsize = md_reloc_size * fixup_segment (data_fix_root, SEG_DATA);
    H_SET_RELOCATION_SIZE (&headers, trsize, drsize);

    /* FIXME: Move this stuff into the pre-write-hook.  */
    H_SET_MAGIC_NUMBER (&headers, magic_number_for_object_file);
    H_SET_ENTRY_POINT (&headers, 0);

    obj_pre_write_hook (&headers);	/* Extra coff stuff.  */

    object_file_size = H_GET_FILE_SIZE (&headers);
    next_object_file_charP = the_object_file = xmalloc (object_file_size);

    output_file_create (out_file_name);

    obj_header_append (&next_object_file_charP, &headers);

    know ((next_object_file_charP - the_object_file)
	  == H_GET_HEADER_SIZE (&headers));

    /* Emit code.  */
    for (fragP = text_frag_root; fragP; fragP = fragP->fr_next)
      {
	register long count;
	register char *fill_literal;
	register long fill_size;

	PROGRESS (1);
	know (fragP->fr_type == rs_fill);
	append (&next_object_file_charP, fragP->fr_literal,
		(unsigned long) fragP->fr_fix);
	fill_literal = fragP->fr_literal + fragP->fr_fix;
	fill_size = fragP->fr_var;
	know (fragP->fr_offset >= 0);

	for (count = fragP->fr_offset; count; count--)
	  append (&next_object_file_charP, fill_literal,
		  (unsigned long) fill_size);
      }

    know ((next_object_file_charP - the_object_file)
	  == (H_GET_HEADER_SIZE (&headers)
	      + H_GET_TEXT_SIZE (&headers)
	      + H_GET_DATA_SIZE (&headers)));

    /* Emit relocations.  */
    obj_emit_relocations (&next_object_file_charP, text_fix_root,
			  (relax_addressT) 0);
    know ((next_object_file_charP - the_object_file)
	  == (H_GET_HEADER_SIZE (&headers)
	      + H_GET_TEXT_SIZE (&headers)
	      + H_GET_DATA_SIZE (&headers)
	      + H_GET_TEXT_RELOCATION_SIZE (&headers)));
#ifdef TC_I960
    /* Make addresses in data relocation directives relative to beginning of
       first data fragment, not end of last text fragment:  alignment of the
       start of the data segment may place a gap between the segments.  */
    obj_emit_relocations (&next_object_file_charP, data_fix_root,
			  data0_frchainP->frch_root->fr_address);
#else /* TC_I960  */
    obj_emit_relocations (&next_object_file_charP, data_fix_root,
			  text_last_frag->fr_address);
#endif /* TC_I960  */

    know ((next_object_file_charP - the_object_file)
	  == (H_GET_HEADER_SIZE (&headers)
	      + H_GET_TEXT_SIZE (&headers)
	      + H_GET_DATA_SIZE (&headers)
	      + H_GET_TEXT_RELOCATION_SIZE (&headers)
	      + H_GET_DATA_RELOCATION_SIZE (&headers)));

    /* Emit line number entries.  */
    OBJ_EMIT_LINENO (&next_object_file_charP, lineno_rootP, the_object_file);
    know ((next_object_file_charP - the_object_file)
	  == (H_GET_HEADER_SIZE (&headers)
	      + H_GET_TEXT_SIZE (&headers)
	      + H_GET_DATA_SIZE (&headers)
	      + H_GET_TEXT_RELOCATION_SIZE (&headers)
	      + H_GET_DATA_RELOCATION_SIZE (&headers)
	      + H_GET_LINENO_SIZE (&headers)));

    /* Emit symbols.  */
    obj_emit_symbols (&next_object_file_charP, symbol_rootP);
    know ((next_object_file_charP - the_object_file)
	  == (H_GET_HEADER_SIZE (&headers)
	      + H_GET_TEXT_SIZE (&headers)
	      + H_GET_DATA_SIZE (&headers)
	      + H_GET_TEXT_RELOCATION_SIZE (&headers)
	      + H_GET_DATA_RELOCATION_SIZE (&headers)
	      + H_GET_LINENO_SIZE (&headers)
	      + H_GET_SYMBOL_TABLE_SIZE (&headers)));

    /* Emit strings.  */
    if (string_byte_count > 0)
      obj_emit_strings (&next_object_file_charP);

#ifdef BFD_HEADERS
    bfd_seek (stdoutput, 0, 0);
    bfd_write (the_object_file, 1, object_file_size, stdoutput);
#else

    /* Write the data to the file.  */
    output_file_append (the_object_file, object_file_size, out_file_name);
    free (the_object_file);
#endif
  }
#else /* OBJ_VMS  */
  /* Now do the VMS-dependent part of writing the object file.  */
  vms_write_object_file (H_GET_TEXT_SIZE (&headers),
			 H_GET_DATA_SIZE (&headers),
			 H_GET_BSS_SIZE (&headers),
			 text_frag_root, data_frag_root);
#endif /* OBJ_VMS  */
#else /* BFD_ASSEMBLER  */

  /* Resolve symbol values.  This needs to be done before processing
     the relocations.  */
  if (symbol_rootP)
    {
      symbolS *symp;

      for (symp = symbol_rootP; symp; symp = symbol_next (symp))
	resolve_symbol_value (symp, 1);
    }
  resolve_local_symbol_values ();

  PROGRESS (1);

#ifdef tc_frob_file_before_adjust
  tc_frob_file_before_adjust ();
#endif
#ifdef obj_frob_file_before_adjust
  obj_frob_file_before_adjust ();
#endif

  bfd_map_over_sections (stdoutput, adjust_reloc_syms, (char *) 0);

  /* Set up symbol table, and write it out.  */
  if (symbol_rootP)
    {
      symbolS *symp;

      for (symp = symbol_rootP; symp; symp = symbol_next (symp))
	{
	  int punt = 0;
	  const char *name;

	  if (symbol_mri_common_p (symp))
	    {
	      if (S_IS_EXTERNAL (symp))
		as_bad (_("%s: global symbols not supported in common sections"),
			S_GET_NAME (symp));
	      symbol_remove (symp, &symbol_rootP, &symbol_lastP);
	      continue;
	    }

	  name = S_GET_NAME (symp);
	  if (name)
	    {
	      const char *name2 =
		decode_local_label_name ((char *) S_GET_NAME (symp));
	      /* They only differ if `name' is a fb or dollar local
		 label name.  */
	      if (name2 != name && ! S_IS_DEFINED (symp))
		as_bad (_("local label %s is not defined"), name2);
	    }

	  /* Do it again, because adjust_reloc_syms might introduce
	     more symbols.  They'll probably only be section symbols,
	     but they'll still need to have the values computed.  */
	  resolve_symbol_value (symp, 1);

	  /* Skip symbols which were equated to undefined or common
             symbols.  */
	  if (symbol_equated_p (symp)
	      && (! S_IS_DEFINED (symp) || S_IS_COMMON (symp)))
	    {
	      symbol_remove (symp, &symbol_rootP, &symbol_lastP);
	      continue;
	    }

	  /* So far, common symbols have been treated like undefined symbols.
	     Put them in the common section now.  */
	  if (S_IS_DEFINED (symp) == 0
	      && S_GET_VALUE (symp) != 0)
	    S_SET_SEGMENT (symp, bfd_com_section_ptr);
#if 0
	  printf ("symbol `%s'\n\t@%x: value=%d flags=%x seg=%s\n",
		  S_GET_NAME (symp), symp,
		  S_GET_VALUE (symp),
		  symbol_get_bfdsym (symp)->flags,
		  segment_name (S_GET_SEGMENT (symp)));
#endif

#ifdef obj_frob_symbol
	  obj_frob_symbol (symp, punt);
#endif
#ifdef tc_frob_symbol
	  if (! punt || symbol_used_in_reloc_p (symp))
	    tc_frob_symbol (symp, punt);
#endif

	  /* If we don't want to keep this symbol, splice it out of
	     the chain now.  If EMIT_SECTION_SYMBOLS is 0, we never
	     want section symbols.  Otherwise, we skip local symbols
	     and symbols that the frob_symbol macros told us to punt,
	     but we keep such symbols if they are used in relocs.  */
	  if ((! EMIT_SECTION_SYMBOLS
	       && symbol_section_p (symp))
	      /* Note that S_IS_EXTERN and S_IS_LOCAL are not always
		 opposites.  Sometimes the former checks flags and the
		 latter examines the name...  */
	      || (!S_IS_EXTERN (symp)
		  && (S_IS_LOCAL (symp) || punt)
		  && ! symbol_used_in_reloc_p (symp)))
	    {
	      symbol_remove (symp, &symbol_rootP, &symbol_lastP);

	      /* After symbol_remove, symbol_next(symp) still returns
		 the one that came after it in the chain.  So we don't
		 need to do any extra cleanup work here.  */
	      continue;
	    }

	  /* Make sure we really got a value for the symbol.  */
	  if (! symbol_resolved_p (symp))
	    {
	      as_bad (_("can't resolve value for symbol \"%s\""),
		      S_GET_NAME (symp));
	      symbol_mark_resolved (symp);
	    }

	  /* Set the value into the BFD symbol.  Up til now the value
	     has only been kept in the gas symbolS struct.  */
	  symbol_get_bfdsym (symp)->value = S_GET_VALUE (symp);
	}
    }

  PROGRESS (1);

  /* Now do any format-specific adjustments to the symbol table, such
     as adding file symbols.  */
#ifdef tc_adjust_symtab
  tc_adjust_symtab ();
#endif
#ifdef obj_adjust_symtab
  obj_adjust_symtab ();
#endif

  /* Now that all the sizes are known, and contents correct, we can
     start writing to the file.  */
  set_symtab ();

  /* If *_frob_file changes the symbol value at this point, it is
     responsible for moving the changed value into symp->bsym->value
     as well.  Hopefully all symbol value changing can be done in
     *_frob_symbol.  */
#ifdef tc_frob_file
  tc_frob_file ();
#endif
#ifdef obj_frob_file
  obj_frob_file ();
#endif

  bfd_map_over_sections (stdoutput, write_relocs, (char *) 0);

#ifdef tc_frob_file_after_relocs
  tc_frob_file_after_relocs ();
#endif
#ifdef obj_frob_file_after_relocs
  obj_frob_file_after_relocs ();
#endif

  bfd_map_over_sections (stdoutput, write_contents, (char *) 0);
#endif /* BFD_ASSEMBLER  */
}
#endif /* ! BFD  */

#ifdef TC_GENERIC_RELAX_TABLE

/* Relax a fragment by scanning TC_GENERIC_RELAX_TABLE.  */

long
relax_frag (segment, fragP, stretch)
     segT segment;
     fragS *fragP;
     long stretch;
{
  const relax_typeS *this_type;
  const relax_typeS *start_type;
  relax_substateT next_state;
  relax_substateT this_state;
  long growth;
  offsetT aim;
  addressT target;
  addressT address;
  symbolS *symbolP;
  const relax_typeS *table;

  target = fragP->fr_offset;
  address = fragP->fr_address;
  table = TC_GENERIC_RELAX_TABLE;
  this_state = fragP->fr_subtype;
  start_type = this_type = table + this_state;
  symbolP = fragP->fr_symbol;

  if (symbolP)
    {
      fragS *sym_frag;

      sym_frag = symbol_get_frag (symbolP);

#ifndef DIFF_EXPR_OK
#if !defined (MANY_SEGMENTS) && !defined (BFD_ASSEMBLER)
      know ((S_GET_SEGMENT (symbolP) == SEG_ABSOLUTE)
	    || (S_GET_SEGMENT (symbolP) == SEG_DATA)
	    || (S_GET_SEGMENT (symbolP) == SEG_BSS)
	    || (S_GET_SEGMENT (symbolP) == SEG_TEXT));
#endif
      know (sym_frag != NULL);
#endif
      know (!(S_GET_SEGMENT (symbolP) == absolute_section)
	    || sym_frag == &zero_address_frag);
      target += S_GET_VALUE (symbolP) + sym_frag->fr_address;

      /* If frag has yet to be reached on this pass,
	 assume it will move by STRETCH just as we did.
	 If this is not so, it will be because some frag
	 between grows, and that will force another pass.  */

      if (stretch != 0
	  && sym_frag->relax_marker != fragP->relax_marker
	  && S_GET_SEGMENT (symbolP) == segment)
	{
	  target += stretch;
	}
    }

  aim = target - address - fragP->fr_fix;
#ifdef TC_PCREL_ADJUST
  /* Currently only the ns32k family needs this.  */
  aim += TC_PCREL_ADJUST (fragP);
/* #else */
  /* This machine doesn't want to use pcrel_adjust.
     In that case, pcrel_adjust should be zero.  */
#if 0
  assert (fragP->fr_targ.ns32k.pcrel_adjust == 0);
#endif
#endif
#ifdef md_prepare_relax_scan /* formerly called M68K_AIM_KLUDGE  */
  md_prepare_relax_scan (fragP, address, aim, this_state, this_type);
#endif

  if (aim < 0)
    {
      /* Look backwards.  */
      for (next_state = this_type->rlx_more; next_state;)
	if (aim >= this_type->rlx_backward)
	  next_state = 0;
	else
	  {
	    /* Grow to next state.  */
	    this_state = next_state;
	    this_type = table + this_state;
	    next_state = this_type->rlx_more;
	  }
    }
  else
    {
      /* Look forwards.  */
      for (next_state = this_type->rlx_more; next_state;)
	if (aim <= this_type->rlx_forward)
	  next_state = 0;
	else
	  {
	    /* Grow to next state.  */
	    this_state = next_state;
	    this_type = table + this_state;
	    next_state = this_type->rlx_more;
	  }
    }

  growth = this_type->rlx_length - start_type->rlx_length;
  if (growth != 0)
    fragP->fr_subtype = this_state;
  return growth;
}

#endif /* defined (TC_GENERIC_RELAX_TABLE)  */

/* Relax_align. Advance location counter to next address that has 'alignment'
   lowest order bits all 0s, return size of adjustment made.  */
static relax_addressT
relax_align (address, alignment)
     register relax_addressT address;	/* Address now.  */
     register int alignment;	/* Alignment (binary).  */
{
  relax_addressT mask;
  relax_addressT new_address;

  mask = ~((~0) << alignment);
  new_address = (address + mask) & (~mask);
#ifdef LINKER_RELAXING_SHRINKS_ONLY
  if (linkrelax)
    /* We must provide lots of padding, so the linker can discard it
       when needed.  The linker will not add extra space, ever.  */
    new_address += (1 << alignment);
#endif
  return (new_address - address);
}

/* Now we have a segment, not a crowd of sub-segments, we can make
   fr_address values.

   Relax the frags.

   After this, all frags in this segment have addresses that are correct
   within the segment. Since segments live in different file addresses,
   these frag addresses may not be the same as final object-file
   addresses.  */

void
relax_segment (segment_frag_root, segment)
     struct frag *segment_frag_root;
     segT segment;
{
  register struct frag *fragP;
  register relax_addressT address;
#if !defined (MANY_SEGMENTS) && !defined (BFD_ASSEMBLER)
  know (segment == SEG_DATA || segment == SEG_TEXT || segment == SEG_BSS);
#endif
  /* In case md_estimate_size_before_relax() wants to make fixSs.  */
  subseg_change (segment, 0);

  /* For each frag in segment: count and store  (a 1st guess of)
     fr_address.  */
  address = 0;
  for (fragP = segment_frag_root; fragP; fragP = fragP->fr_next)
    {
      fragP->relax_marker = 0;
      fragP->fr_address = address;
      address += fragP->fr_fix;

      switch (fragP->fr_type)
	{
	case rs_fill:
	  address += fragP->fr_offset * fragP->fr_var;
	  break;

	case rs_align:
	case rs_align_code:
	case rs_align_test:
	  {
	    addressT offset = relax_align (address, (int) fragP->fr_offset);

	    if (fragP->fr_subtype != 0 && offset > fragP->fr_subtype)
	      offset = 0;

	    if (offset % fragP->fr_var != 0)
	      {
		as_bad (_("alignment padding (%lu bytes) not a multiple of %ld"),
			(unsigned long) offset, (long) fragP->fr_var);
		offset -= (offset % fragP->fr_var);
	      }

	    address += offset;
	  }
	  break;

	case rs_org:
	case rs_space:
	  /* Assume .org is nugatory. It will grow with 1st relax.  */
	  break;

	case rs_machine_dependent:
	  address += md_estimate_size_before_relax (fragP, segment);
	  break;

#ifndef WORKING_DOT_WORD
	  /* Broken words don't concern us yet.  */
	case rs_broken_word:
	  break;
#endif

	case rs_leb128:
	  /* Initial guess is always 1; doing otherwise can result in
	     stable solutions that are larger than the minimum.  */
	  address += fragP->fr_offset = 1;
	  break;

	case rs_cfa:
	  address += eh_frame_estimate_size_before_relax (fragP);
	  break;

	case rs_dwarf2dbg:
	  address += dwarf2dbg_estimate_size_before_relax (fragP);
	  break;

	default:
	  BAD_CASE (fragP->fr_type);
	  break;
	}
    }

  /* Do relax().  */
  {
    long stretch;	/* May be any size, 0 or negative.  */
    /* Cumulative number of addresses we have relaxed this pass.
       We may have relaxed more than one address.  */
    int stretched;	/* Have we stretched on this pass?  */
    /* This is 'cuz stretch may be zero, when, in fact some piece of code
       grew, and another shrank.  If a branch instruction doesn't fit anymore,
       we could be scrod.  */

    do
      {
	stretch = 0;
	stretched = 0;

	for (fragP = segment_frag_root; fragP; fragP = fragP->fr_next)
	  {
	    long growth = 0;
	    addressT was_address;
	    offsetT offset;
	    symbolS *symbolP;

	    fragP->relax_marker ^= 1;
	    was_address = fragP->fr_address;
	    address = fragP->fr_address += stretch;
	    symbolP = fragP->fr_symbol;
	    offset = fragP->fr_offset;

	    switch (fragP->fr_type)
	      {
	      case rs_fill:	/* .fill never relaxes.  */
		growth = 0;
		break;

#ifndef WORKING_DOT_WORD
		/* JF:  This is RMS's idea.  I do *NOT* want to be blamed
		   for it I do not want to write it.  I do not want to have
		   anything to do with it.  This is not the proper way to
		   implement this misfeature.  */
	      case rs_broken_word:
		{
		  struct broken_word *lie;
		  struct broken_word *untruth;

		  /* Yes this is ugly (storing the broken_word pointer
		     in the symbol slot).  Still, this whole chunk of
		     code is ugly, and I don't feel like doing anything
		     about it.  Think of it as stubbornness in action.  */
		  growth = 0;
		  for (lie = (struct broken_word *) (fragP->fr_symbol);
		       lie && lie->dispfrag == fragP;
		       lie = lie->next_broken_word)
		    {

		      if (lie->added)
			continue;

		      offset = (symbol_get_frag (lie->add)->fr_address
				+ S_GET_VALUE (lie->add)
				+ lie->addnum
				- (symbol_get_frag (lie->sub)->fr_address
				   + S_GET_VALUE (lie->sub)));
		      if (offset <= -32768 || offset >= 32767)
			{
			  if (flag_warn_displacement)
			    {
			      char buf[50];
			      sprint_value (buf, (addressT) lie->addnum);
			      as_warn (_(".word %s-%s+%s didn't fit"),
				       S_GET_NAME (lie->add),
				       S_GET_NAME (lie->sub),
				       buf);
			    }
			  lie->added = 1;
			  if (fragP->fr_subtype == 0)
			    {
			      fragP->fr_subtype++;
			      growth += md_short_jump_size;
			    }
			  for (untruth = lie->next_broken_word;
			       untruth && untruth->dispfrag == lie->dispfrag;
			       untruth = untruth->next_broken_word)
			    if ((symbol_get_frag (untruth->add)
				 == symbol_get_frag (lie->add))
				&& (S_GET_VALUE (untruth->add)
				    == S_GET_VALUE (lie->add)))
			      {
				untruth->added = 2;
				untruth->use_jump = lie;
			      }
			  growth += md_long_jump_size;
			}
		    }

		  break;
		}		/* case rs_broken_word  */
#endif
	      case rs_align:
	      case rs_align_code:
	      case rs_align_test:
		{
		  addressT oldoff, newoff;

		  oldoff = relax_align (was_address + fragP->fr_fix,
					(int) offset);
		  newoff = relax_align (address + fragP->fr_fix,
					(int) offset);

		  if (fragP->fr_subtype != 0)
		    {
		      if (oldoff > fragP->fr_subtype)
			oldoff = 0;
		      if (newoff > fragP->fr_subtype)
			newoff = 0;
		    }

		  growth = newoff - oldoff;
		}
		break;

	      case rs_org:
		{
		  addressT target = offset;
		  addressT after;

		  if (symbolP)
		    {
#if !defined (MANY_SEGMENTS) && !defined (BFD_ASSEMBLER)
		      know ((S_GET_SEGMENT (symbolP) == SEG_ABSOLUTE)
			    || (S_GET_SEGMENT (symbolP) == SEG_DATA)
			    || (S_GET_SEGMENT (symbolP) == SEG_TEXT)
			    || S_GET_SEGMENT (symbolP) == SEG_BSS);
		      know (symbolP->sy_frag);
		      know (!(S_GET_SEGMENT (symbolP) == SEG_ABSOLUTE)
			    || (symbolP->sy_frag == &zero_address_frag));
#endif
		      target += (S_GET_VALUE (symbolP)
				 + symbol_get_frag (symbolP)->fr_address);
		    }		/* if we have a symbol  */

		  know (fragP->fr_next);
		  after = fragP->fr_next->fr_address;
		  growth = target - after;
		  if (growth < 0)
		    {
		      /* Growth may be negative, but variable part of frag
			 cannot have fewer than 0 chars.  That is, we can't
			 .org backwards.  */
		      as_bad_where (fragP->fr_file, fragP->fr_line,
				    _("attempt to .org backwards ignored"));

		      /* We've issued an error message.  Change the
                         frag to avoid cascading errors.  */
		      fragP->fr_type = rs_align;
		      fragP->fr_subtype = 0;
		      fragP->fr_offset = 0;
		      fragP->fr_fix = after - address;
		      growth = stretch;
		    }

		  /* This is an absolute growth factor  */
		  growth -= stretch;
		  break;
		}

	      case rs_space:
		if (symbolP)
		  {
		    growth = S_GET_VALUE (symbolP);
		    if (symbol_get_frag (symbolP) != &zero_address_frag
			|| S_IS_COMMON (symbolP)
			|| ! S_IS_DEFINED (symbolP))
		      as_bad_where (fragP->fr_file, fragP->fr_line,
				    _(".space specifies non-absolute value"));
		    fragP->fr_symbol = 0;
		    if (growth < 0)
		      {
			as_warn (_(".space or .fill with negative value, ignored"));
			growth = 0;
		      }
		  }
		else
		  growth = 0;
		break;

	      case rs_machine_dependent:
#ifdef md_relax_frag
		growth = md_relax_frag (segment, fragP, stretch);
#else
#ifdef TC_GENERIC_RELAX_TABLE
		/* The default way to relax a frag is to look through
		   TC_GENERIC_RELAX_TABLE.  */
		growth = relax_frag (segment, fragP, stretch);
#endif /* TC_GENERIC_RELAX_TABLE  */
#endif
		break;

	      case rs_leb128:
		{
		  valueT value;
		  int size;

		  value = resolve_symbol_value (fragP->fr_symbol, 0);
		  size = sizeof_leb128 (value, fragP->fr_subtype);
		  growth = size - fragP->fr_offset;
		  fragP->fr_offset = size;
		}
		break;

	      case rs_cfa:
		growth = eh_frame_relax_frag (fragP);
		break;

	      case rs_dwarf2dbg:
		growth = dwarf2dbg_relax_frag (fragP);
		break;

	      default:
		BAD_CASE (fragP->fr_type);
		break;
	      }
	    if (growth)
	      {
		stretch += growth;
		stretched = 1;
	      }
	  }			/* For each frag in the segment.  */
      }
    while (stretched);		/* Until nothing further to relax.  */
  }				/* do_relax  */

  /* We now have valid fr_address'es for each frag.  */

  /* All fr_address's are correct, relative to their own segment.
     We have made all the fixS we will ever make.  */
}

#if defined (BFD_ASSEMBLER) || (!defined (BFD) && !defined (OBJ_VMS))

#ifndef TC_RELOC_RTSYM_LOC_FIXUP
#define TC_RELOC_RTSYM_LOC_FIXUP(X) (1)
#endif

/* fixup_segment()

   Go through all the fixS's in a segment and see which ones can be
   handled now.  (These consist of fixS where we have since discovered
   the value of a symbol, or the address of the frag involved.)
   For each one, call md_apply_fix to put the fix into the frag data.

   Result is a count of how many relocation structs will be needed to
   handle the remaining fixS's that we couldn't completely handle here.
   These will be output later by emit_relocations().  */

static long
fixup_segment (fixP, this_segment_type)
     register fixS *fixP;
     segT this_segment_type;	/* N_TYPE bits for segment.  */
{
  long seg_reloc_count = 0;
  symbolS *add_symbolP;
  symbolS *sub_symbolP;
  valueT add_number;
  int size;
  char *place;
  long where;
  int pcrel, plt;
  fragS *fragP;
  segT add_symbol_segment = absolute_section;

  /* If the linker is doing the relaxing, we must not do any fixups.

     Well, strictly speaking that's not true -- we could do any that are
     PC-relative and don't cross regions that could change size.  And for the
     i960 (the only machine for which we've got a relaxing linker right now),
     we might be able to turn callx/callj into bal anyways in cases where we
     know the maximum displacement.  */
  if (linkrelax && TC_LINKRELAX_FIXUP (this_segment_type))
    {
      for (; fixP; fixP = fixP->fx_next)
	seg_reloc_count++;
      TC_ADJUST_RELOC_COUNT (fixP, seg_reloc_count);
      return seg_reloc_count;
    }

  for (; fixP; fixP = fixP->fx_next)
    {
#ifdef DEBUG5
      fprintf (stderr, "\nprocessing fixup:\n");
      print_fixup (fixP);
#endif

      fragP = fixP->fx_frag;
      know (fragP);
      where = fixP->fx_where;
      place = fragP->fr_literal + where;
      size = fixP->fx_size;
      add_symbolP = fixP->fx_addsy;
#ifdef TC_VALIDATE_FIX
      TC_VALIDATE_FIX (fixP, this_segment_type, skip);
#endif
      sub_symbolP = fixP->fx_subsy;
      add_number = fixP->fx_offset;
      pcrel = fixP->fx_pcrel;
      plt = fixP->fx_plt;

      if (add_symbolP != NULL
	  && symbol_mri_common_p (add_symbolP))
	{
	  know (add_symbolP->sy_value.X_op == O_symbol);
	  add_number += S_GET_VALUE (add_symbolP);
	  fixP->fx_offset = add_number;
	  add_symbolP = fixP->fx_addsy =
	    symbol_get_value_expression (add_symbolP)->X_add_symbol;
	}

      if (add_symbolP)
	add_symbol_segment = S_GET_SEGMENT (add_symbolP);

      if (sub_symbolP)
	{
	  resolve_symbol_value (sub_symbolP, 1);
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
		  fixP->fx_subsy = NULL;
		}
	      else if (pcrel
		       && S_GET_SEGMENT (sub_symbolP) == this_segment_type)
		{
		  /* Should try converting to a constant.  */
		  goto bad_sub_reloc;
		}
	      else
	      bad_sub_reloc:
		as_bad_where (fixP->fx_file, fixP->fx_line,
			      _("Negative of non-absolute symbol %s"),
			      S_GET_NAME (sub_symbolP));
	    }
	  else if (S_GET_SEGMENT (sub_symbolP) == add_symbol_segment
		   && SEG_NORMAL (add_symbol_segment))
	    {
	      /* Difference of 2 symbols from same segment.
		 Can't make difference of 2 undefineds: 'value' means
		 something different for N_UNDF.  */
#ifdef TC_I960
	      /* Makes no sense to use the difference of 2 arbitrary symbols
		 as the target of a call instruction.  */
	      if (fixP->fx_tcbit)
		as_bad_where (fixP->fx_file, fixP->fx_line,
			      _("callj to difference of 2 symbols"));
#endif /* TC_I960  */
	      add_number += S_GET_VALUE (add_symbolP) -
		S_GET_VALUE (sub_symbolP);

	      add_symbolP = NULL;
	      pcrel = 0;	/* No further pcrel processing.  */

	      /* Let the target machine make the final determination
		 as to whether or not a relocation will be needed to
		 handle this fixup.  */
	      if (!TC_FORCE_RELOCATION_SECTION (fixP, this_segment_type))
		{
		  fixP->fx_pcrel = 0;
		  fixP->fx_addsy = NULL;
		  fixP->fx_subsy = NULL;
		}
	    }
	  else
	    {
	      /* Different segments in subtraction.  */
	      know (!(S_IS_EXTERNAL (sub_symbolP)
		      && (S_GET_SEGMENT (sub_symbolP) == absolute_section)));

	      if ((S_GET_SEGMENT (sub_symbolP) == absolute_section))
		add_number -= S_GET_VALUE (sub_symbolP);

#ifdef DIFF_EXPR_OK
	      else if (S_GET_SEGMENT (sub_symbolP) == this_segment_type
#if 0
		       /* Do this even if it's already described as
			  pc-relative.  For example, on the m68k, an
			  operand of "pc@(foo-.-2)" should address
			  "foo" in a pc-relative mode.  */
		       && pcrel
#endif
		       )
		{
		  /* Make it pc-relative.  */
		  add_number += (MD_PCREL_FROM_SECTION (fixP, this_segment_type)
				 - S_GET_VALUE (sub_symbolP));
		  pcrel = 1;
		  fixP->fx_pcrel = 1;
		  sub_symbolP = 0;
		  fixP->fx_subsy = 0;
		}
#endif
#ifdef UNDEFINED_DIFFERENCE_OK
	      /* The PA needs this for PIC code generation.  We basically
		 don't want to do anything if we have the difference of two
		 symbols at this point.  */
	      else if (1)
		{
		  /* Leave it alone.  */
		}
#endif
#ifdef BFD_ASSEMBLER
	      else if (fixP->fx_r_type == BFD_RELOC_GPREL32
		       || fixP->fx_r_type == BFD_RELOC_GPREL16)
		{
		  /* Leave it alone.  */
		}
#endif
	      else
		{
		  char buf[50];
		  sprint_value (buf, fragP->fr_address + where);
		  as_bad_where (fixP->fx_file, fixP->fx_line,
				_("Subtraction of two symbols in different sections \"%s\" {%s section} - \"%s\" {%s section} at file address %s."),
				S_GET_NAME (add_symbolP),
				segment_name (S_GET_SEGMENT (add_symbolP)),
				S_GET_NAME (sub_symbolP),
				segment_name (S_GET_SEGMENT (sub_symbolP)),
				buf);
		}
	    }
	}

      if (add_symbolP)
	{
	  if (add_symbol_segment == this_segment_type && pcrel && !plt
	      && TC_RELOC_RTSYM_LOC_FIXUP (fixP))
	    {
	      /* This fixup was made when the symbol's segment was
		 SEG_UNKNOWN, but it is now in the local segment.
		 So we know how to do the address without relocation.  */
#ifdef TC_I960
	      /* reloc_callj() may replace a 'call' with a 'calls' or a
		 'bal', in which cases it modifies *fixP as appropriate.
		 In the case of a 'calls', no further work is required,
		 and *fixP has been set up to make the rest of the code
		 below a no-op.  */
	      reloc_callj (fixP);
#endif /* TC_I960  */

	      add_number += S_GET_VALUE (add_symbolP);
	      add_number -= MD_PCREL_FROM_SECTION (fixP, this_segment_type);
	      /* Lie.  Don't want further pcrel processing.  */
	      pcrel = 0;

	      /* Let the target machine make the final determination
		 as to whether or not a relocation will be needed to
		 handle this fixup.  */
	      if (!TC_FORCE_RELOCATION (fixP))
		{
		  fixP->fx_pcrel = 0;
		  fixP->fx_addsy = NULL;
		}
	    }
	  else
	    {
	      if (add_symbol_segment == absolute_section
		  && ! pcrel)
		{
#ifdef TC_I960
		  /* See comment about reloc_callj() above.  */
		  reloc_callj (fixP);
#endif /* TC_I960  */
		  add_number += S_GET_VALUE (add_symbolP);

		  /* Let the target machine make the final determination
		     as to whether or not a relocation will be needed to
		     handle this fixup.  */

		  if (!TC_FORCE_RELOCATION (fixP))
		    {
		      fixP->fx_addsy = NULL;
		      add_symbolP = NULL;
		    }
		}
	      else if (add_symbol_segment == undefined_section
#ifdef BFD_ASSEMBLER
		       || bfd_is_com_section (add_symbol_segment)
#endif
		       )
		{
#ifdef TC_I960
		  if ((int) fixP->fx_bit_fixP == 13)
		    {
		      /* This is a COBR instruction.  They have only a
			 13-bit displacement and are only to be used
			 for local branches: flag as error, don't generate
			 relocation.  */
		      as_bad_where (fixP->fx_file, fixP->fx_line,
				    _("can't use COBR format with external label"));
		      fixP->fx_addsy = NULL;
		      fixP->fx_done = 1;
		      continue;
		    }		/* COBR.  */
#endif /* TC_I960  */

#ifdef OBJ_COFF
#ifdef TE_I386AIX
		  if (S_IS_COMMON (add_symbolP))
		    add_number += S_GET_VALUE (add_symbolP);
#endif /* TE_I386AIX  */
#endif /* OBJ_COFF  */
		  ++seg_reloc_count;
		}
	      else
		{
		  seg_reloc_count++;
		  if (TC_FIX_ADJUSTABLE (fixP))
		    add_number += S_GET_VALUE (add_symbolP);
		}
	    }
	}

      if (pcrel)
	{
	  add_number -= MD_PCREL_FROM_SECTION (fixP, this_segment_type);
	  if (add_symbolP == 0)
	    {
#ifndef BFD_ASSEMBLER
	      fixP->fx_addsy = &abs_symbol;
#else
	      fixP->fx_addsy = section_symbol (absolute_section);
#endif
	      symbol_mark_used_in_reloc (fixP->fx_addsy);
	      ++seg_reloc_count;
	    }
	}

      if (!fixP->fx_done)
	{
#ifdef MD_APPLY_FIX3
	  md_apply_fix3 (fixP, &add_number, this_segment_type);
#else
#ifdef BFD_ASSEMBLER
	  md_apply_fix (fixP, &add_number);
#else
	  md_apply_fix (fixP, add_number);
#endif
#endif

#ifndef TC_HANDLES_FX_DONE
	  /* If the tc-* files haven't been converted, assume it's handling
	     it the old way, where a null fx_addsy means that the fix has
	     been applied completely, and no further work is needed.  */
	  if (fixP->fx_addsy == 0 && fixP->fx_pcrel == 0)
	    fixP->fx_done = 1;
#endif
	}

      if (!fixP->fx_bit_fixP && !fixP->fx_no_overflow && size > 0)
	{
	  if ((size_t) size < sizeof (valueT))
	    {
	      valueT mask;

	      mask = 0;
	      mask--;		/* Set all bits to one.  */
	      mask <<= size * 8 - (fixP->fx_signed ? 1 : 0);
	      if ((add_number & mask) != 0 && (add_number & mask) != mask)
		{
		  char buf[50], buf2[50];
		  sprint_value (buf, fragP->fr_address + where);
		  if (add_number > 1000)
		    sprint_value (buf2, add_number);
		  else
		    sprintf (buf2, "%ld", (long) add_number);
		  as_bad_where (fixP->fx_file, fixP->fx_line,
				_("Value of %s too large for field of %d bytes at %s"),
				buf2, size, buf);
		} /* Generic error checking.  */
	    }
#ifdef WARN_SIGNED_OVERFLOW_WORD
	  /* Warn if a .word value is too large when treated as a signed
	     number.  We already know it is not too negative.  This is to
	     catch over-large switches generated by gcc on the 68k.  */
	  if (!flag_signed_overflow_ok
	      && size == 2
	      && add_number > 0x7fff)
	    as_bad_where (fixP->fx_file, fixP->fx_line,
			  _("Signed .word overflow; switch may be too large; %ld at 0x%lx"),
			  (long) add_number,
			  (unsigned long) (fragP->fr_address + where));
#endif
	}			/* Not a bit fix.  */

#ifdef TC_VALIDATE_FIX
    skip:  ATTRIBUTE_UNUSED_LABEL
      ;
#endif
#ifdef DEBUG5
      fprintf (stderr, "result:\n");
      print_fixup (fixP);
#endif
    }				/* For each fixS in this segment.  */

  TC_ADJUST_RELOC_COUNT (fixP, seg_reloc_count);
  return seg_reloc_count;
}

#endif /* defined (BFD_ASSEMBLER) || (!defined (BFD) && !defined (OBJ_VMS)) */

void
number_to_chars_bigendian (buf, val, n)
     char *buf;
     valueT val;
     int n;
{
  if ((size_t) n > sizeof (val) || n <= 0)
    abort ();
  while (n--)
    {
      buf[n] = val & 0xff;
      val >>= 8;
    }
}

void
number_to_chars_littleendian (buf, val, n)
     char *buf;
     valueT val;
     int n;
{
  if ((size_t) n > sizeof (val) || n <= 0)
    abort ();
  while (n--)
    {
      *buf++ = val & 0xff;
      val >>= 8;
    }
}

void
write_print_statistics (file)
     FILE *file;
{
  fprintf (file, "fixups: %d\n", n_fixups);
}

/* For debugging.  */
extern int indent_level;

void
print_fixup (fixp)
     fixS *fixp;
{
  indent_level = 1;
  fprintf (stderr, "fix %lx %s:%d", (long) fixp, fixp->fx_file, fixp->fx_line);
  if (fixp->fx_pcrel)
    fprintf (stderr, " pcrel");
  if (fixp->fx_pcrel_adjust)
    fprintf (stderr, " pcrel_adjust=%d", fixp->fx_pcrel_adjust);
  if (fixp->fx_im_disp)
    {
#ifdef TC_NS32K
      fprintf (stderr, " im_disp=%d", fixp->fx_im_disp);
#else
      fprintf (stderr, " im_disp");
#endif
    }
  if (fixp->fx_tcbit)
    fprintf (stderr, " tcbit");
  if (fixp->fx_done)
    fprintf (stderr, " done");
  fprintf (stderr, "\n    size=%d frag=%lx where=%ld offset=%lx addnumber=%lx",
	   fixp->fx_size, (long) fixp->fx_frag, (long) fixp->fx_where,
	   (long) fixp->fx_offset, (long) fixp->fx_addnumber);
#ifdef BFD_ASSEMBLER
  fprintf (stderr, "\n    %s (%d)", bfd_get_reloc_code_name (fixp->fx_r_type),
	   fixp->fx_r_type);
#else
#ifdef NEED_FX_R_TYPE
  fprintf (stderr, " r_type=%d", fixp->fx_r_type);
#endif
#endif
  if (fixp->fx_addsy)
    {
      fprintf (stderr, "\n   +<");
      print_symbol_value_1 (stderr, fixp->fx_addsy);
      fprintf (stderr, ">");
    }
  if (fixp->fx_subsy)
    {
      fprintf (stderr, "\n   -<");
      print_symbol_value_1 (stderr, fixp->fx_subsy);
      fprintf (stderr, ">");
    }
  fprintf (stderr, "\n");
#ifdef TC_FIX_DATA_PRINT
  TC_FIX_DATA_PRINT (stderr, fixp);
#endif
}
