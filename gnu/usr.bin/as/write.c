/* write.c - emit .o file

   Copyright (C) 1986, 1987, 1990, 1991, 1992 Free Software Foundation, Inc.

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
   along with GAS; see the file COPYING.  If not, write to
   the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.  */

/* This thing should be set up to do byteordering correctly.  But... */

#ifndef lint
static char rcsid[] = "$Id: write.c,v 1.7 1995/05/30 04:46:40 rgrimes Exp $";
#endif

#include "as.h"
#include "subsegs.h"
#include "obstack.h"
#include "output-file.h"

/* The NOP_OPCODE is for the alignment fill value.
 * fill it a nop instruction so that the disassembler does not choke
 * on it
 */
#ifndef NOP_OPCODE
#define NOP_OPCODE 0x00
#endif

#ifndef MANY_SEGMENTS
static struct frag *text_frag_root;
static struct frag *data_frag_root;

static struct frag *text_last_frag;	/* Last frag in segment. */
static struct frag *data_last_frag;	/* Last frag in segment. */
#endif

#ifndef WORKING_DOT_WORD
extern const int md_short_jump_size;
extern const int md_long_jump_size;
#endif

static object_headers headers;

long string_byte_count;

static char *the_object_file;

char *next_object_file_charP;	/* Tracks object file bytes. */

/* static long		length; JF unused */	/* String length, including trailing '\0'. */


#if __STDC__ == 1

static int is_dnrange(struct frag *f1, struct frag *f2);
static long fixup_segment(fixS *fixP, segT this_segment_type);
static relax_addressT relax_align(relax_addressT address, long alignment);
void relax_segment(struct frag *segment_frag_root, segT segment_type);

#else

static int is_dnrange();
static long fixup_segment();
static relax_addressT relax_align();
void relax_segment();

#endif /* not __STDC__ */

/*
 *			fix_new()
 *
 * Create a fixS in obstack 'notes'.
 */
#ifdef PIC
fixS *fix_new(frag, where, size, add_symbol, sub_symbol, offset, pcrel, r_type, got_symbol)
#else
fixS *fix_new(frag, where, size, add_symbol, sub_symbol, offset, pcrel, r_type)
#endif
fragS *frag;		/* Which frag? */
int where;		/* Where in that frag? */
short int size;		/* 1, 2, or 4 usually. */
symbolS *add_symbol;	/* X_add_symbol. */
symbolS *sub_symbol;	/* X_subtract_symbol. */
#ifdef PIC
symbolS *got_symbol;	/* X_got. */
#endif
long offset;		/* X_add_number. */
int pcrel;		/* TRUE if PC-relative relocation. */
enum reloc_type	r_type;	/* Relocation type */
{
	fixS *fixP;

	fixP = (fixS *) obstack_alloc(&notes, sizeof(fixS));

	fixP->fx_frag	= frag;
	fixP->fx_where	= where;
	fixP->fx_size	= size;
	fixP->fx_addsy	= add_symbol;
	fixP->fx_subsy	= sub_symbol;
#ifdef PIC
	fixP->fx_gotsy	= got_symbol;
	if (got_symbol)
		pcrel = 1;
#endif
	fixP->fx_offset	= offset;
	fixP->fx_pcrel	= pcrel;
	fixP->fx_r_type	= r_type;

	/* JF these 'cuz of the NS32K stuff */
	fixP->fx_im_disp = 0;
	fixP->fx_pcrel_adjust = 0;
	fixP->fx_bsr = 0;
	fixP->fx_bit_fixP = 0;

	/* usually, we want relocs sorted numerically, but while
	   comparing to older versions of gas that have relocs
	   reverse sorted, it is convenient to have this compile
	   time option.  xoxorich. */

#ifdef REVERSE_SORT_RELOCS

	fixP->fx_next = *seg_fix_rootP;
	*seg_fix_rootP = fixP;

#else /* REVERSE_SORT_RELOCS */

	fixP->fx_next	= NULL;

	if (*seg_fix_tailP)
	    (*seg_fix_tailP)->fx_next = fixP;
	else
	    *seg_fix_rootP = fixP;
	*seg_fix_tailP = fixP;

#endif /* REVERSE_SORT_RELOCS */

	fixP->fx_callj = 0;
	return(fixP);
} /* fix_new() */

#ifndef BFD
void write_object_file()
{
	register struct frchain *	frchainP; /* Track along all frchains. */
	register fragS *		fragP;	/* Track along all frags. */
	register struct frchain *	next_frchainP;
	register fragS * *		prev_fragPP;
	/*  register char *		name; */
	/*  symbolS *symbolP; */
	/*  register symbolS **		symbolPP; */
	/* register fixS *		fixP; JF unused */
	unsigned int data_siz;

	long object_file_size;

#ifdef	OBJ_VMS
	/*
	 *	Under VMS we try to be compatible with VAX-11 "C".  Thus, we
	 *	call a routine to check for the definition of the procedure
	 *	"_main", and if so -- fix it up so that it can be program
	 *	entry point.
	 */
	VMS_Check_For_Main();
#endif /* OBJ_VMS */
	/*
	 * After every sub-segment, we fake an ".align ...". This conforms to BSD4.2
	 * brane-damage. We then fake ".fill 0" because that is the kind of frag
	 * that requires least thought. ".align" frags like to have a following
	 * frag since that makes calculating their intended length trivial.
	 */
#ifndef SUB_SEGMENT_ALIGN
#define SUB_SEGMENT_ALIGN (2)
#endif
	for (frchainP = frchain_root; frchainP; frchainP = frchainP->frch_next) {
#ifdef	OBJ_VMS
		/*
		 *	Under VAX/VMS, the linker (and PSECT specifications)
		 *	take care of correctly aligning the segments.
		 *	Doing the alignment here (on initialized data) can
		 *	mess up the calculation of global data PSECT sizes.
		 */
#undef	SUB_SEGMENT_ALIGN
#define	SUB_SEGMENT_ALIGN ((frchainP->frch_seg != SEG_DATA) ? 2 : 0)
#endif	/* OBJ_VMS */
		subseg_new (frchainP->frch_seg, frchainP->frch_subseg);
		frag_align (SUB_SEGMENT_ALIGN, NOP_OPCODE);
		/* frag_align will have left a new frag. */
		/* Use this last frag for an empty ".fill". */
		/*
		 * For this segment ...
		 * Create a last frag. Do not leave a "being filled in frag".
		 */
		frag_wane (frag_now);
		frag_now->fr_fix	= 0;
		know( frag_now->fr_next == NULL );
		/* know(frags.obstack_c_base == frags.obstack_c_next_free); */
		/* Above shows we haven't left a half-completed object on obstack. */
	} /* walk the frag chain */

	/*
	 * From now on, we don't care about sub-segments.
	 * Build one frag chain for each segment. Linked thru fr_next.
	 * We know that there is at least 1 text frchain & at least 1 data frchain.
	 */
	prev_fragPP = &text_frag_root;
	for (frchainP = frchain_root; frchainP; frchainP = next_frchainP) {
		know( frchainP->frch_root );
		*prev_fragPP = frchainP->frch_root;
		prev_fragPP = & frchainP->frch_last->fr_next;

		if (((next_frchainP = frchainP->frch_next) == NULL)
		    || next_frchainP == data0_frchainP) {
			prev_fragPP = &data_frag_root;
			if (next_frchainP) {
				text_last_frag = frchainP->frch_last;
			} else {
				data_last_frag = frchainP->frch_last;
			}
		}
	} /* walk the frag chain */

	/*
	 * We have two segments. If user gave -R flag, then we must put the
	 * data frags into the text segment. Do this before relaxing so
	 * we know to take advantage of -R and make shorter addresses.
	 */
	if (flagseen[ 'R' ]) {
		fixS *tmp;

		text_last_frag->fr_next = data_frag_root;
		text_last_frag = data_last_frag;
		data_last_frag = NULL;
		data_frag_root = NULL;
		if (text_fix_root) {
			for (tmp = text_fix_root; tmp->fx_next; tmp = tmp->fx_next) ;;
			tmp->fx_next = data_fix_root;
		} else
		    text_fix_root = data_fix_root;
		data_fix_root = NULL;
	}

	relax_segment(text_frag_root, SEG_TEXT);
	relax_segment(data_frag_root, SEG_DATA);
	/*
	 * Now the addresses of frags are correct within the segment.
	 */

	know(text_last_frag->fr_type == rs_fill && text_last_frag->fr_offset == 0);
	H_SET_TEXT_SIZE(&headers, text_last_frag->fr_address);
	text_last_frag->fr_address = H_GET_TEXT_SIZE(&headers);

	/*
	 * Join the 2 segments into 1 huge segment.
	 * To do this, re-compute every rn_address in the SEG_DATA frags.
	 * Then join the data frags after the text frags.
	 *
	 * Determine a_data [length of data segment].
	 */
	if (data_frag_root) {
		register relax_addressT slide;

		know((text_last_frag->fr_type == rs_fill) && (text_last_frag->fr_offset == 0));

		H_SET_DATA_SIZE(&headers, data_last_frag->fr_address);
		data_last_frag->fr_address = H_GET_DATA_SIZE(&headers);
		slide = H_GET_TEXT_SIZE(&headers); /* & in file of the data segment. */

		for (fragP = data_frag_root; fragP; fragP = fragP->fr_next) {
			fragP->fr_address += slide;
		} /* for each data frag */

		know(text_last_frag != 0);
		text_last_frag->fr_next = data_frag_root;
	} else {
		H_SET_DATA_SIZE(&headers,0);
		data_siz = 0;
	}

	bss_address_frag.fr_address = (H_GET_TEXT_SIZE(&headers) +
				       H_GET_DATA_SIZE(&headers));

	H_SET_BSS_SIZE(&headers,local_bss_counter);

	/*
	 *
	 * Crawl the symbol chain.
	 *
	 * For each symbol whose value depends on a frag, take the address of
	 * that frag and subsume it into the value of the symbol.
	 * After this, there is just one way to lookup a symbol value.
	 * Values are left in their final state for object file emission.
	 * We adjust the values of 'L' local symbols, even if we do
	 * not intend to emit them to the object file, because their values
	 * are needed for fix-ups.
	 *
	 * Unless we saw a -L flag, remove all symbols that begin with 'L'
	 * from the symbol chain.  (They are still pointed to by the fixes.)
	 *
	 * Count the remaining symbols.
	 * Assign a symbol number to each symbol.
	 * Count the number of string-table chars we will emit.
	 * Put this info into the headers as appropriate.
	 *
	 */
	know(zero_address_frag.fr_address == 0);
	string_byte_count = sizeof(string_byte_count);

	obj_crawl_symbol_chain(&headers);

	if (string_byte_count == sizeof(string_byte_count)) {
		string_byte_count = 0;
	} /* if no strings, then no count. */

	H_SET_STRING_SIZE(&headers, string_byte_count);

	/*
	 * Addresses of frags now reflect addresses we use in the object file.
	 * Symbol values are correct.
	 * Scan the frags, converting any ".org"s and ".align"s to ".fill"s.
	 * Also converting any machine-dependent frags using md_convert_frag();
	 */
	subseg_change(SEG_TEXT, 0);

	for (fragP = text_frag_root;  fragP;  fragP = fragP->fr_next) {
		switch (fragP->fr_type) {
		case rs_align:
		case rs_org:
			fragP->fr_type = rs_fill;
			know(fragP->fr_var == 1);
			know(fragP->fr_next != NULL);

			fragP->fr_offset = (fragP->fr_next->fr_address
					    - fragP->fr_address
					    - fragP->fr_fix);
			break;

		case rs_fill:
			break;

		case rs_machine_dependent:
			md_convert_frag(&headers, fragP);

			know((fragP->fr_next == NULL) || ((fragP->fr_next->fr_address - fragP->fr_address) == fragP->fr_fix));

			/*
			 * After md_convert_frag, we make the frag into a ".space 0".
			 * Md_convert_frag() should set up any fixSs and constants
			 * required.
			 */
			frag_wane(fragP);
			break;

#ifndef WORKING_DOT_WORD
		case rs_broken_word: {
			struct broken_word *lie;

			if (fragP->fr_subtype) {
				fragP->fr_fix+=md_short_jump_size;
				for (lie=(struct broken_word *)(fragP->fr_symbol);lie && lie->dispfrag == fragP;lie=lie->next_broken_word)
				    if (lie->added == 1)
					fragP->fr_fix+=md_long_jump_size;
			}
			frag_wane(fragP);
		}
			break;
#endif

		default:
			BAD_CASE( fragP->fr_type );
			break;
		} /* switch (fr_type) */

		know((fragP->fr_next == NULL)
		     || ((fragP->fr_next->fr_address - fragP->fr_address)
			 == (fragP->fr_fix + (fragP->fr_offset * fragP->fr_var))));
	} /* for each frag. */

#ifndef WORKING_DOT_WORD
	{
		struct broken_word *lie;
		struct broken_word **prevP;

		prevP = &broken_words;
		for (lie = broken_words; lie; lie = lie->next_broken_word)
		    if (!lie->added) {
#ifdef TC_NS32K
			    fix_new_ns32k(lie->frag,
					  lie->word_goes_here - lie->frag->fr_literal,
					  2,
					  lie->add,
					  lie->sub,
					  lie->addnum,
					  0, 0, 2, 0, 0, NO_RELOC);
#else /* TC_NS32K */
#ifdef PIC
			    fix_new(lie->frag, lie->word_goes_here - lie->frag->fr_literal,
				    2, lie->add,
				    lie->sub, lie->addnum,
				    0, NO_RELOC, (symbolS *)0);
#else
			    fix_new(lie->frag, lie->word_goes_here - lie->frag->fr_literal,
				    2, lie->add,
				    lie->sub, lie->addnum,
				    0, NO_RELOC);
#endif
#endif /* TC_NS32K */
			    /* md_number_to_chars(lie->word_goes_here,
			       S_GET_VALUE(lie->add)
			       + lie->addnum
			       - S_GET_VALUE(lie->sub),
			       2); */
			    *prevP = lie->next_broken_word;
		    } else
			prevP = &(lie->next_broken_word);

		for (lie = broken_words; lie;) {
			struct broken_word *untruth;
			char *table_ptr;
			long table_addr;
			long from_addr,
			to_addr;
			int n,
			m;

			fragP = lie->dispfrag;

			/* Find out how many broken_words go here */
			n=0;
			for (untruth = lie; untruth && untruth->dispfrag == fragP; untruth = untruth->next_broken_word)
			    if (untruth->added == 1)
				n++;

			table_ptr = lie->dispfrag->fr_opcode;
			table_addr = lie->dispfrag->fr_address + (table_ptr - lie->dispfrag->fr_literal);
			/* Create the jump around the long jumps */
			/* This is a short jump from table_ptr+0 to table_ptr+n*long_jump_size */
			from_addr = table_addr;
			to_addr = table_addr + md_short_jump_size + n * md_long_jump_size;
			md_create_short_jump(table_ptr, from_addr, to_addr, lie->dispfrag, lie->add);
			table_ptr += md_short_jump_size;
			table_addr += md_short_jump_size;

			for (m = 0; lie && lie->dispfrag == fragP; m++, lie = lie->next_broken_word) {
				if (lie->added == 2)
				    continue;
				/* Patch the jump table */
				/* This is the offset from ??? to table_ptr+0 */
				to_addr = table_addr
				    - S_GET_VALUE(lie->sub);
				md_number_to_chars(lie->word_goes_here, to_addr, 2);
				for (untruth = lie->next_broken_word;
				     untruth && untruth->dispfrag == fragP;
				     untruth = untruth->next_broken_word) {
					if (untruth->use_jump == lie)
					    md_number_to_chars(untruth->word_goes_here, to_addr, 2);
				}

				/* Install the long jump */
				/* this is a long jump from table_ptr+0 to the final target */
				from_addr = table_addr;
				to_addr = S_GET_VALUE(lie->add) + lie->addnum;
				md_create_long_jump(table_ptr, from_addr, to_addr, lie->dispfrag, lie->add);
				table_ptr += md_long_jump_size;
				table_addr += md_long_jump_size;
			}
		}
	}
#endif /* not WORKING_DOT_WORD */

#ifndef	OBJ_VMS
	{ /* not vms */
		/*
		 * Scan every FixS performing fixups. We had to wait until now to do
		 * this because md_convert_frag() may have made some fixSs.
		 */

		H_SET_RELOCATION_SIZE(&headers,
				      md_reloc_size * fixup_segment(text_fix_root, SEG_TEXT),
				      md_reloc_size * fixup_segment(data_fix_root, SEG_DATA));


		obj_pre_write_hook(&headers);

		if ((had_warnings() && flagseen['Z'])
		    || had_errors() > 0) {
			if (flagseen['Z']) {
				as_warn("%d error%s, %d warning%s, generating bad object file.\n",
					had_errors(), had_errors() == 1 ? "" : "s",
					had_warnings(), had_warnings() == 1 ? "" : "s");
			} else {
				as_fatal("%d error%s, %d warning%s, no object file generated.\n",
					 had_errors(), had_errors() == 1 ? "" : "s",
					 had_warnings(), had_warnings() == 1 ? "" : "s");
			} /* on want output */
		} /* on error condition */

		object_file_size = H_GET_FILE_SIZE(&headers);
		next_object_file_charP = the_object_file = xmalloc(object_file_size);

		output_file_create(out_file_name);

		obj_header_append(&next_object_file_charP, &headers);

		know((next_object_file_charP - the_object_file) == H_GET_HEADER_SIZE(&headers));

		/*
		 * Emit code.
		 */
		for (fragP = text_frag_root;  fragP;  fragP = fragP->fr_next) {
			register long count;
			register char *fill_literal;
			register long fill_size;

			know(fragP->fr_type == rs_fill);
			append(&next_object_file_charP, fragP->fr_literal, (unsigned long) fragP->fr_fix);
			fill_literal = fragP->fr_literal + fragP->fr_fix;
			fill_size = fragP->fr_var;
			know(fragP->fr_offset >= 0);

			for (count = fragP->fr_offset; count; count--) {
				append(&next_object_file_charP, fill_literal, (unsigned long) fill_size);
			} /* for each  */

		} /* for each code frag. */

		know((next_object_file_charP - the_object_file) == (H_GET_HEADER_SIZE(&headers) + H_GET_TEXT_SIZE(&headers) + H_GET_DATA_SIZE(&headers)));

		/*
		 * Emit relocations.
		 */
		obj_emit_relocations(&next_object_file_charP, text_fix_root, (relax_addressT)0);
		know((next_object_file_charP - the_object_file) == (H_GET_HEADER_SIZE(&headers) + H_GET_TEXT_SIZE(&headers) + H_GET_DATA_SIZE(&headers) + H_GET_TEXT_RELOCATION_SIZE(&headers)));
#ifdef TC_I960
		/* Make addresses in data relocation directives relative to beginning of
		 * first data fragment, not end of last text fragment:  alignment of the
		 * start of the data segment may place a gap between the segments.
		 */
		obj_emit_relocations(&next_object_file_charP, data_fix_root, data0_frchainP->frch_root->fr_address);
#else /* TC_I960 */
		obj_emit_relocations(&next_object_file_charP, data_fix_root, text_last_frag->fr_address);
#endif /* TC_I960 */

		know((next_object_file_charP - the_object_file) == (H_GET_HEADER_SIZE(&headers) + H_GET_TEXT_SIZE(&headers) + H_GET_DATA_SIZE(&headers) + H_GET_TEXT_RELOCATION_SIZE(&headers) + H_GET_DATA_RELOCATION_SIZE(&headers)));

		/*
		 * Emit line number entries.
		 */
		OBJ_EMIT_LINENO(&next_object_file_charP, lineno_rootP, the_object_file);
		know((next_object_file_charP - the_object_file) == (H_GET_HEADER_SIZE(&headers) + H_GET_TEXT_SIZE(&headers) + H_GET_DATA_SIZE(&headers) + H_GET_TEXT_RELOCATION_SIZE(&headers) + H_GET_DATA_RELOCATION_SIZE(&headers) + H_GET_LINENO_SIZE(&headers)));

		/*
		 * Emit symbols.
		 */
		obj_emit_symbols(&next_object_file_charP, symbol_rootP);
		know((next_object_file_charP - the_object_file) == (H_GET_HEADER_SIZE(&headers) + H_GET_TEXT_SIZE(&headers) + H_GET_DATA_SIZE(&headers) + H_GET_TEXT_RELOCATION_SIZE(&headers) + H_GET_DATA_RELOCATION_SIZE(&headers) + H_GET_LINENO_SIZE(&headers) + H_GET_SYMBOL_TABLE_SIZE(&headers)));

		/*
		 * Emit strings.
		 */

		if (string_byte_count > 0) {
			obj_emit_strings(&next_object_file_charP);
		} /* only if we have a string table */

		/*	  know((next_object_file_charP - the_object_file) == (H_GET_HEADER_SIZE(&headers) + H_GET_TEXT_SIZE(&headers) + H_GET_DATA_SIZE(&headers) + H_GET_TEXT_RELOCATION_SIZE(&headers) + H_GET_DATA_RELOCATION_SIZE(&headers) + H_GET_LINENO_SIZE(&headers) + H_GET_SYMBOL_TABLE_SIZE(&headers) + H_GET_STRING_SIZE(&headers)));
		 */
		/*	  know(next_object_file_charP == the_object_file + object_file_size);*/

#ifdef BFD_HEADERS
		bfd_seek(stdoutput, 0, 0);
		bfd_write(the_object_file, 1, object_file_size,  stdoutput);
#else

		/* Write the data to the file */
		output_file_append(the_object_file, object_file_size, out_file_name);
#endif

		output_file_close(out_file_name);
	} /* non vms output */
#else	/* OBJ_VMS */
	/*
	 *	Now do the VMS-dependent part of writing the object file
	 */
  VMS_write_object_file(H_GET_TEXT_SIZE(&headers), H_GET_DATA_SIZE(&headers),
		 text_frag_root, data_frag_root);
#endif	/* OBJ_VMS */
} /* write_object_file() */
#else
#endif

/*
 *			relax_segment()
 *
 * Now we have a segment, not a crowd of sub-segments, we can make fr_address
 * values.
 *
 * Relax the frags.
 *
 * After this, all frags in this segment have addresses that are correct
 * within the segment. Since segments live in different file addresses,
 * these frag addresses may not be the same as final object-file addresses.
 */



void relax_segment(segment_frag_root, segment)
struct frag *	segment_frag_root;
segT		segment; /* SEG_DATA or SEG_TEXT */
{
	register struct frag *	fragP;
	register relax_addressT	address;
	/* register relax_addressT	old_address; JF unused */
	/* register relax_addressT	new_address; JF unused */
#ifndef MANY_SEGMENTS
	know(segment == SEG_DATA || segment == SEG_TEXT);
#endif
	/* In case md_estimate_size_before_relax() wants to make fixSs. */
	subseg_change(segment, 0);

	/*
	 * For each frag in segment: count and store  (a 1st guess of) fr_address.
	 */
	address = 0;
	for (fragP = segment_frag_root; fragP; fragP = fragP->fr_next) {
		fragP->fr_address = address;
		address += fragP->fr_fix;

		switch (fragP->fr_type) {
		case rs_fill:
			address += fragP->fr_offset * fragP->fr_var ;
			break;

		case rs_align:
			address += relax_align(address, fragP->fr_offset);
			break;

		case rs_org:
			/*
			 * Assume .org is nugatory. It will grow with 1st relax.
			 */
			break;

		case rs_machine_dependent:
			address += md_estimate_size_before_relax(fragP, segment);
			break;

#ifndef WORKING_DOT_WORD
			/* Broken words don't concern us yet */
		case rs_broken_word:
			break;
#endif

		default:
			BAD_CASE(fragP->fr_type);
			break;
		} /* switch (fr_type) */
	} /* for each frag in the segment */

	/*
	 * Do relax().
	 */
	{
		register long stretch; /* May be any size, 0 or negative. */
		/* Cumulative number of addresses we have */
		/* relaxed this pass. */
		/* We may have relaxed more than one address. */
		register long stretched;  /* Have we stretched on this pass? */
		/* This is 'cuz stretch may be zero, when,
		   in fact some piece of code grew, and
		   another shrank.  If a branch instruction
		   doesn't fit anymore, we could be scrod */

		do {
			stretch = stretched = 0;
			for (fragP = segment_frag_root;  fragP;  fragP = fragP->fr_next) {
				register long growth = 0;
				register unsigned long was_address;
				register long offset;
				register symbolS *symbolP;
				register long target;
				register long after;
				register long aim;

				was_address = fragP->fr_address;
				address = fragP->fr_address += stretch;
				symbolP = fragP->fr_symbol;
				offset = fragP->fr_offset;

				switch (fragP->fr_type) {
				case rs_fill:	/* .fill never relaxes. */
					growth = 0;
					break;

#ifndef WORKING_DOT_WORD
					/* JF:  This is RMS's idea.  I do *NOT* want to be blamed
					   for it I do not want to write it.  I do not want to have
					   anything to do with it.  This is not the proper way to
					   implement this misfeature. */
				case rs_broken_word: {
					struct broken_word *lie;
					struct broken_word *untruth;

					/* Yes this is ugly (storing the broken_word pointer
					   in the symbol slot).  Still, this whole chunk of
					   code is ugly, and I don't feel like doing anything
					   about it.  Think of it as stubbornness in action */
					growth=0;
					for (lie=(struct broken_word *)(fragP->fr_symbol);
					     lie && lie->dispfrag == fragP;
					     lie=lie->next_broken_word) {

						if (lie->added)
						    continue;

						offset = lie->add->sy_frag->fr_address+ S_GET_VALUE(lie->add) + lie->addnum -
						    (lie->sub->sy_frag->fr_address+ S_GET_VALUE(lie->sub));
						if (offset <= -32768 || offset >= 32767) {
#if 0
							if (flagseen['K'])
							    as_warn(".word %s-%s+%ld didn't fit",
								    S_GET_NAME(lie->add),
								    S_GET_NAME(lie->sub),
								    lie->addnum);
#endif
							lie->added=1;
							if (fragP->fr_subtype == 0) {
								fragP->fr_subtype++;
								growth+=md_short_jump_size;
							}
							for (untruth=lie->next_broken_word;untruth && untruth->dispfrag == lie->dispfrag;untruth=untruth->next_broken_word)
							    if ((untruth->add->sy_frag == lie->add->sy_frag)
								&& S_GET_VALUE(untruth->add) == S_GET_VALUE(lie->add)) {
								    untruth->added=2;
								    untruth->use_jump=lie;
							    }
							growth += md_long_jump_size;
						}
					}

					break;
				} /* case rs_broken_word */
#endif
				case rs_align:
					growth = relax_align((relax_addressT) (address + fragP->fr_fix), offset)
					    - relax_align((relax_addressT) (was_address + fragP->fr_fix), offset);
					break;

				case rs_org:
					target = offset;

					if (symbolP) {
#ifdef MANY_SEGMENTS
#else
						know((S_GET_SEGMENT(symbolP) == SEG_ABSOLUTE) || (S_GET_SEGMENT(symbolP) == SEG_DATA) || (S_GET_SEGMENT(symbolP) == SEG_TEXT));
						know(symbolP->sy_frag);
						know(!(S_GET_SEGMENT(symbolP) == SEG_ABSOLUTE) || (symbolP->sy_frag == &zero_address_frag));
#endif
						target += S_GET_VALUE(symbolP)
						    + symbolP->sy_frag->fr_address;
					} /* if we have a symbol */

					know(fragP->fr_next);
					after = fragP->fr_next->fr_address;
					growth = ((target - after ) > 0) ? (target - after) : 0;
					/* Growth may be -ve, but variable part */
					/* of frag cannot have < 0 chars. */
					/* That is, we can't .org backwards. */

					growth -= stretch;	/* This is an absolute growth factor */
					break;

				case rs_machine_dependent: {
					register const relax_typeS *	this_type;
					register const relax_typeS *	start_type;
					register relax_substateT	next_state;
					register relax_substateT	this_state;

					start_type = this_type = md_relax_table + (this_state = fragP->fr_subtype);
					target = offset;

					if (symbolP) {
#ifndef MANY_SEGMENTS
						know((S_GET_SEGMENT(symbolP) == SEG_ABSOLUTE) || (S_GET_SEGMENT(symbolP) == SEG_DATA) || (S_GET_SEGMENT(symbolP) == SEG_TEXT));
#endif
						know(symbolP->sy_frag);
						know(!(S_GET_SEGMENT(symbolP) == SEG_ABSOLUTE) || symbolP->sy_frag == &zero_address_frag );
						target +=
						    S_GET_VALUE(symbolP)
							+ symbolP->sy_frag->fr_address;

						/* If frag has yet to be reached on this pass,
						   assume it will move by STRETCH just as we did.
						   If this is not so, it will be because some frag
						   between grows, and that will force another pass.  */

						/* JF was just address */
						/* JF also added is_dnrange hack */
						/* There's gotta be a better/faster/etc way
						   to do this... */
						/* gnu@cygnus.com:  I changed this from > to >=
						   because I ran into a zero-length frag (fr_fix=0)
						   which was created when the obstack needed a new
						   chunk JUST AFTER the opcode of a branch.  Since
						   fr_fix is zero, fr_address of this frag is the same
						   as fr_address of the next frag.  This
						   zero-length frag was variable and jumped to .+2
						   (in the next frag), but since the > comparison
						   below failed (the two were =, not >), "stretch"
						   was not added to the target.  Stretch was 178, so
						   the offset appeared to be .-176 instead, which did
						   not fit into a byte branch, so the assembler
						   relaxed the branch to a word.  This didn't compare
						   with what happened when the same source file was
						   assembled on other machines, which is how I found it.
						   You might want to think about what other places have
						   trouble with zero length frags... */

						if (symbolP->sy_frag->fr_address >= was_address
						    && is_dnrange(fragP,symbolP->sy_frag)) {
							target += stretch;
						} /*  */

					} /* if there's a symbol attached */

					aim = target - address - fragP->fr_fix;
					/* The displacement is affected by the instruction size
					 * for the 32k architecture. I think we ought to be able
					 * to add fragP->fr_pcrel_adjust in all cases (it should be
					 * zero if not used), but just in case it breaks something
					 * else we'll put this inside #ifdef NS32K ... #endif
					 */
#ifdef TC_NS32K
					aim += fragP->fr_pcrel_adjust;
#endif /* TC_NS32K */

					if (aim < 0) {
						/* Look backwards. */
						for (next_state = this_type->rlx_more; next_state; ) {
							if (aim >= this_type->rlx_backward) {
								next_state = 0;
							} else { /* Grow to next state. */
								this_type = md_relax_table + (this_state = next_state);
								next_state = this_type->rlx_more;
							}
						}
					} else {
#ifdef DONTDEF
						/* JF these next few lines of code are for the mc68020 which can't handle short
						   offsets of zero in branch instructions.  What a kludge! */
						if (aim == 0 && this_state == (1<<2+0)) { /* FOO hard encoded from m.c */
							aim=this_type->rlx_forward+1; /* Force relaxation into word mode */
						}
#endif
#ifdef M68K_AIM_KLUDGE
						M68K_AIM_KLUDGE(aim, this_state, this_type);
#endif
						/* JF end of 68020 code */
						/* Look forwards. */
						for (next_state = this_type->rlx_more; next_state; ) {
							if (aim <= this_type->rlx_forward) {
								next_state = 0;
							} else { /* Grow to next state. */
								this_type = md_relax_table + (this_state = next_state);
								next_state = this_type->rlx_more;
							}
						}
					}

					if ((growth = this_type->rlx_length - start_type->rlx_length) != 0)
					    fragP->fr_subtype = this_state;

					break;
				} /* case rs_machine_dependent */

				default:
					BAD_CASE( fragP->fr_type );
					break;
				}
				if (growth) {
					stretch += growth;
					stretched++;
				}
			} /* For each frag in the segment. */
		} while (stretched);	/* Until nothing further to relax. */
	} /* do_relax */

	/*
	 * We now have valid fr_address'es for each frag.
	 */

	/*
	 * All fr_address's are correct, relative to their own segment.
	 * We have made all the fixS we will ever make.
	 */
} /* relax_segment() */

/*
 * Relax_align. Advance location counter to next address that has 'alignment'
 * lowest order bits all 0s.
 */

/* How many addresses does the .align take? */
static relax_addressT relax_align(address, alignment)
register relax_addressT address; /* Address now. */
register long alignment; /* Alignment (binary). */
{
	relax_addressT	mask;
	relax_addressT	new_address;

	mask = ~ ( (~0) << alignment );
	new_address = (address + mask) & (~ mask);
	return (new_address - address);
} /* relax_align() */

/* fixup_segment()

   Go through all the fixS's in a segment and see which ones can be
   handled now.  (These consist of fixS where we have since discovered
   the value of a symbol, or the address of the frag involved.)
   For each one, call md_apply_fix to put the fix into the frag data.

   Result is a count of how many relocation structs will be needed to
   handle the remaining fixS's that we couldn't completely handle here.
   These will be output later by emit_relocations().  */

static long fixup_segment(fixP, this_segment_type)
register fixS *fixP;
segT this_segment_type; /* N_TYPE bits for segment. */
{
	register long seg_reloc_count;
	register symbolS *add_symbolP;
	register symbolS *sub_symbolP;
	register long add_number;
	register int size;
	register char *place;
	register long where;
	register char pcrel;
	register fragS *fragP;
	register segT add_symbol_segment = SEG_ABSOLUTE;

	/* FIXME: remove this line */ /*	fixS *orig = fixP; */
	seg_reloc_count = 0;

	for ( ;  fixP;  fixP = fixP->fx_next) {
		fragP = fixP->fx_frag;
		know(fragP);
		where = fixP->fx_where;
		place = fragP->fr_literal + where;
		size = fixP->fx_size;
		add_symbolP = fixP->fx_addsy;
#ifdef TC_I960
		if (fixP->fx_callj && TC_S_IS_CALLNAME(add_symbolP)) {
			/* Relocation should be done via the
			   associated 'bal' entry point
			   symbol. */

			if (!TC_S_IS_BALNAME(tc_get_bal_of_call(add_symbolP))) {
				as_bad("No 'bal' entry point for leafproc %s",
				       S_GET_NAME(add_symbolP));
				continue;
			}
			fixP->fx_addsy = add_symbolP = tc_get_bal_of_call(add_symbolP);
		}	/* callj relocation */
#endif
		sub_symbolP = fixP->fx_subsy;
		add_number = fixP->fx_offset;
		pcrel = fixP->fx_pcrel;

		if (add_symbolP) {
			add_symbol_segment = S_GET_SEGMENT(add_symbolP);
		}	/* if there is an addend */

		if (sub_symbolP) {
			if (!add_symbolP) {
				/* Its just -sym */
				if (S_GET_SEGMENT(sub_symbolP) != SEG_ABSOLUTE) {
					as_bad("Negative of non-absolute symbol %s", S_GET_NAME(sub_symbolP));
				} /* not absolute */

				add_number -= S_GET_VALUE(sub_symbolP);

				/* if sub_symbol is in the same segment that add_symbol
				   and add_symbol is either in DATA, TEXT, BSS or ABSOLUTE */
			} else if ((S_GET_SEGMENT(sub_symbolP) == add_symbol_segment)
				   && (SEG_NORMAL(add_symbol_segment)
				       || (add_symbol_segment == SEG_ABSOLUTE))) {
				/* Difference of 2 symbols from same segment. */
				/* Can't make difference of 2 undefineds: 'value' means */
				/* something different for N_UNDF. */
#ifdef TC_I960
				/* Makes no sense to use the difference of 2 arbitrary symbols
				 * as the target of a call instruction.
				 */
				if (fixP->fx_callj) {
					as_bad("callj to difference of 2 symbols");
				}
#endif	/* TC_I960 */
#ifdef PIC
				if (picmode &&
						S_IS_EXTERNAL(add_symbolP)) {
					as_bad("Can't reduce difference of external symbols in PIC code");
				}
#endif
				add_number += S_GET_VALUE(add_symbolP) -
				    S_GET_VALUE(sub_symbolP);

				add_symbolP = NULL;
				fixP->fx_addsy = NULL;
			} else {
				/* Different segments in subtraction. */
				know(!(S_IS_EXTERNAL(sub_symbolP) && (S_GET_SEGMENT(sub_symbolP) == SEG_ABSOLUTE)));

				if ((S_GET_SEGMENT(sub_symbolP) == SEG_ABSOLUTE)) {
					add_number -= S_GET_VALUE(sub_symbolP);
				} else {
					as_bad("Can't emit reloc {- %s-seg symbol \"%s\"} @ file address %d.",
					       segment_name(S_GET_SEGMENT(sub_symbolP)),
					       S_GET_NAME(sub_symbolP), fragP->fr_address + where);
				} /* if absolute */
			}
		} /* if sub_symbolP */

#ifdef PIC
		/*
		 * Bring _GLOBAL_OFFSET_TABLE_ forward, now we've had the
		 * chance to collapse any accompanying symbols into a number.
		 * This is the sequel of the hack in expr.c to parse operands
		 * of the form `_GLOBAL_OFFSET_TABLE_+(L1-L2)'. Note that
		 * _GLOBAL_OFFSET_TABLE_ can only be an "add symbol".
		 */
		if (add_symbolP == NULL && fixP->fx_gotsy != NULL) {
			add_symbolP = fixP->fx_addsy = fixP->fx_gotsy;
			add_symbol_segment = S_GET_SEGMENT(add_symbolP);
		}
#endif

		if (add_symbolP) {
			if (add_symbol_segment == this_segment_type && pcrel) {
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
				reloc_callj(fixP);
#endif /* TC_I960 */

				add_number += S_GET_VALUE(add_symbolP);
				add_number -= md_pcrel_from(fixP);
				pcrel = 0; /* Lie. Don't want further pcrel processing. */
				fixP->fx_addsy = NULL; /* No relocations please. */
			} else {
				switch (add_symbol_segment) {
				case SEG_ABSOLUTE:
#ifdef TC_I960
					reloc_callj(fixP); /* See comment about reloc_callj() above*/
#endif /* TC_I960 */
					add_number += S_GET_VALUE(add_symbolP);
					fixP->fx_addsy = NULL;
					add_symbolP = NULL;
					break;
 			        default:
					seg_reloc_count ++;
#ifdef PIC
					/*
					 * Do not fixup refs to global data
					 * even if defined here.
					 */
					if (!picmode ||
#ifdef TC_NS32K
					   fixP->fx_pcrel ||
#endif
					   (fixP->fx_r_type != RELOC_GLOB_DAT &&
#ifdef TC_I386
/* XXX - This must be rationalized */
					    fixP->fx_r_type != RELOC_GOT &&
					    fixP->fx_r_type != RELOC_GOTOFF &&
#endif
						(fixP->fx_r_type != RELOC_32 ||
						!S_IS_EXTERNAL(add_symbolP))))
#endif
						add_number += S_GET_VALUE(add_symbolP);
					break;

				case SEG_UNKNOWN:
#ifdef TC_I960
					if ((int)fixP->fx_bit_fixP == 13) {
						/* This is a COBR instruction.  They have only a
						 * 13-bit displacement and are only to be used
						 * for local branches: flag as error, don't generate
						 * relocation.
						 */
						as_bad("can't use COBR format with external label");
						fixP->fx_addsy = NULL;	/* No relocations please. */
						continue;
					} /* COBR */
#endif /* TC_I960 */

#ifdef OBJ_COFF
#ifdef TE_I386AIX
					if (S_IS_COMMON(add_symbolP))
					    add_number += S_GET_VALUE(add_symbolP);
#endif /* TE_I386AIX */
#endif /* OBJ_COFF */
					++seg_reloc_count;

					break;


				} /* switch on symbol seg */
			} /* if not in local seg */
		} /* if there was a + symbol */

		if (pcrel) {
			add_number -= md_pcrel_from(fixP);
			if (add_symbolP == 0) {
				fixP->fx_addsy = & abs_symbol;
				++seg_reloc_count;
			} /* if there's an add_symbol */
		} /* if pcrel */

		if (!fixP->fx_bit_fixP) {
			if ((size == 1 &&
			     (add_number& ~0xFF)   && (add_number & ~0xFF != (-1 & ~0xFF))) ||
			    (size == 2 &&
			     (add_number& ~0xFFFF) && (add_number & ~0xFFFF != (-1 & ~0xFFFF)))) {
				as_bad("Value of %d too large for field of %d bytes at 0x%x",
				       add_number, size, fragP->fr_address + where);
			} /* generic error checking */
		} /* not a bit fix */

		md_apply_fix(fixP, add_number);
	} /* For each fixS in this segment. */

#ifdef OBJ_COFF
#ifdef TC_I960
	{
		fixS *topP = fixP;

		/* two relocs per callj under coff. */
		for (fixP = topP; fixP; fixP = fixP->fx_next) {
			if (fixP->fx_callj && fixP->fx_addsy != 0) {
				++seg_reloc_count;
			} /* if callj and not already fixed. */
		} /* for each fix */
	}
#endif /* TC_I960 */

#endif /* OBJ_COFF */
	return(seg_reloc_count);
} /* fixup_segment() */


static int is_dnrange(f1,f2)
struct frag *f1;
struct frag *f2;
{
	while (f1) {
		if (f1->fr_next == f2)
		    return 1;
		f1=f1->fr_next;
	}
	return 0;
} /* is_dnrange() */

/* Append a string onto another string, bumping the pointer along.  */
void
    append (charPP, fromP, length)
char	**charPP;
char	*fromP;
unsigned long length;
{
	if (length) {		/* Don't trust memcpy() of 0 chars. */
		memcpy(*charPP, fromP, (int) length);
		*charPP += length;
	}
}

int section_alignment[SEG_MAXIMUM_ORDINAL];

/*
 * This routine records the largest alignment seen for each segment.
 * If the beginning of the segment is aligned on the worst-case
 * boundary, all of the other alignments within it will work.  At
 * least one object format really uses this info.
 */
void record_alignment(seg, align)
segT seg;	/* Segment to which alignment pertains */
int align;	/* Alignment, as a power of 2
		 *	(e.g., 1 => 2-byte boundary, 2 => 4-byte boundary, etc.)
		 */
{

	if ( align > section_alignment[(int) seg] ){
		section_alignment[(int) seg] = align;
	} /* if highest yet */

	return;
} /* record_alignment() */

/*
 * Local Variables:
 * comment-column: 0
 * fill-column: 131
 * End:
 */

/* end of write.c */
