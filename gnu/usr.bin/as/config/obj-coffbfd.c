/* coff object file format with bfd
   Copyright (C) 1989, 1990, 1991, 1992 Free Software Foundation, Inc.

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
   along with GAS; see the file COPYING.  If not, write to
   the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.  */

/*

  How does this releate to the rest of GAS ?

  Well, all the other files in gas are more or less a black box. It
  takes care of opening files, parsing command lines, stripping blanks
  etc etc. This module gets a chance to register what it wants to do by
  saying that it is interested in various pseduo ops. The other big
  change is write_object_file. This runs through all the data
  structures that gas builds, and outputs the file in the format of our
  choice.

  Hacked for BFDness by steve chamberlain

  This object module now supports the Hitachi H8/300 and the AMD 29k

  sac@cygnus.com
  */

#include "as.h"
#include "obstack.h"
#include "subsegs.h"
#include "frags.h"
#include "../bfd/libbfd.h"


/* This vector is used to turn an internal segment into a section #
   suitable for insertion into a coff symbol table
   */

const short seg_N_TYPE[] = { /* in: segT   out: N_TYPE bits */
	C_ABS_SECTION,
	1,
	2,
	3,
	4,
	5,
	6,
	7,
	8,
	9,
	10,
	C_UNDEF_SECTION,		/* SEG_UNKNOWN */
	C_UNDEF_SECTION,		/* SEG_ABSENT */
	C_UNDEF_SECTION,		/* SEG_PASS1 */
	C_UNDEF_SECTION,		/* SEG_GOOF */
	C_UNDEF_SECTION,		/* SEG_BIG */
	C_UNDEF_SECTION,		/* SEG_DIFFERENCE */
	C_DEBUG_SECTION,		/* SEG_DEBUG */
	C_NTV_SECTION,		/* SEG_NTV */
	C_PTV_SECTION,		/* SEG_PTV */
	C_REGISTER_SECTION,	/* SEG_REGISTER */
};


int function_lineoff = -1;	/* Offset in line#s where the last function
				   started (the odd entry for line #0) */

int our_lineno_number = 0;	/* we use this to build pointers from .bf's
				   into the linetable.  It should match
				   exactly the values that are later
				   assigned in text_lineno_number by
				   write.c. */

int text_lineno_number = 0;

/* Add 4 to the real value to get the index and compensate the
   negatives. This vector is used by S_GET_SEGMENT to turn a coff
   section number into a segment number
   */
static symbolS *previous_file_symbol = NULL;
void c_symbol_merge();
static int line_base;

symbolS *c_section_symbol();
bfd *abfd;
void EXFUN(bfd_as_write_hook,(struct internal_filehdr *,
			      bfd *abfd));

static void EXFUN(fixup_segment,(fixS *	fixP,
				 segT	this_segment_type));

static void EXFUN(fill_section,(bfd *abfd ,
				struct internal_filehdr *f, unsigned
				long *));


char *EXFUN(s_get_name,(symbolS *s));
static symbolS *EXFUN(tag_find_or_make,(char *name));
static symbolS* EXFUN(tag_find,(char *name));


static int
    EXFUN(c_line_new,(
		      symbolS *symbol,
		      long paddr,
		      unsigned short line_number,
		      fragS* frag));


static void EXFUN(w_symbols,
		  (bfd *abfd ,
		   char *where ,
		   symbolS *symbol_rootP));



static void EXFUN( obj_coff_def,(int what));
static void EXFUN( obj_coff_lcomm,(void));
static void EXFUN( obj_coff_dim,(void));
static void EXFUN( obj_coff_text,(void));
static void EXFUN( obj_coff_data,(void));
static void EXFUN( obj_coff_endef,(void));
static void EXFUN( obj_coff_line,(void));
static void EXFUN( obj_coff_ln,(void));
static void EXFUN( obj_coff_scl,(void));
static void EXFUN( obj_coff_size,(void));
static void EXFUN( obj_coff_tag,(void));
static void EXFUN( obj_coff_type,(void));
static void EXFUN( obj_coff_val,(void));
static void EXFUN( obj_coff_section,(void));
static void EXFUN( tag_init,(void));
static void EXFUN( tag_insert,(char *name, symbolS *symbolP));


static struct hash_control *tag_hash;
static symbolS *def_symbol_in_progress = NULL;

const pseudo_typeS obj_pseudo_table[] = {
	{ "def",	obj_coff_def,		0	},
	{ "dim",	obj_coff_dim,		0	},
	{ "endef",	obj_coff_endef,		0	},
	{ "line",	obj_coff_line,		0	},
	{ "ln",		obj_coff_ln,		0	},
	{ "scl",	obj_coff_scl,		0	},
	{ "size",	obj_coff_size,		0	},
	{ "tag",	obj_coff_tag,		0	},
	{ "type",	obj_coff_type,		0	},
	{ "val",	obj_coff_val,		0	},
        { "section",    obj_coff_section,       0       },
        { "text",       obj_coff_text,          0       },
        { "data",       obj_coff_data,          0       },
	/* we don't yet handle this. */
	{ "ident",      s_ignore,		0       },
	{ "ABORT",      s_abort,		0 },
	{ "lcomm",	obj_coff_lcomm,	0},
	{ NULL}	/* end sentinel */
}; /* obj_pseudo_table */



/* Section stuff

   We allow more than just the standard 3 sections, infact, we allow
   10 sections, (though the usual three have to be there).

   This structure performs the mappings for us:

   */

/* OBS stuff
   static struct internal_scnhdr bss_section_header;
   struct internal_scnhdr data_section_header;
   struct internal_scnhdr text_section_header;

   const segT N_TYPE_seg[32] =
   {

   };

   */

#define N_SEG 32
typedef struct
{
	segT seg_t;
	int i;
} seg_info_type;

seg_info_type seg_info_off_by_4[N_SEG] =
{
	{SEG_PTV,  },
	{SEG_NTV,  },
	{SEG_DEBUG, },
	{SEG_ABSOLUTE,  },
	{SEG_UNKNOWN,	 },
	{SEG_E0},
	{SEG_E1},
	{SEG_E2},
	{SEG_E3},
	{SEG_E4},
	{SEG_E5},
	{SEG_E6},
	{SEG_E7},
	{SEG_E8},
	{SEG_E9},
	{15},
	{16},
	{17},
	{18},
	{19},
	{20},
	{0},
	{0},
	{0},
	{SEG_REGISTER},0,0,0,0};

#define SEG_INFO_FROM_SECTION_NUMBER(x) (seg_info_off_by_4[(x)+4])
#define SEG_INFO_FROM_SEG_NUMBER(x) (seg_info_off_by_4[(x)])


relax_addressT
    DEFUN(relax_align,(address, alignment),
	  register relax_addressT address AND
	  register long alignment )
{
	relax_addressT	mask;
	relax_addressT	new_address;

	mask = ~ ( (~0) << alignment );
	new_address = (address + mask) & (~ mask);
	return (new_address - address);
} /* relax_align() */


segT
    DEFUN(s_get_segment,(x) ,
	  symbolS* x)
{
	return SEG_INFO_FROM_SECTION_NUMBER(x->sy_symbol.ost_entry.n_scnum).seg_t;
}



/* calculate the size of the frag chain and fill in the section header
   to contain all of it, also fill in the addr of the sections */
static unsigned int  DEFUN(size_section,(abfd, idx),
			   bfd *abfd AND
			   unsigned int idx)
{

	unsigned int size = 0;
	fragS *frag = segment_info[idx].frchainP->frch_root;
	while (frag) {
		if (frag->fr_address != size) {
			printf("Out of step\n");
			size = frag->fr_address;
		}
		size += frag->fr_fix;
		switch (frag->fr_type) {
		case rs_fill:
		case rs_org:
			size  +=    frag->fr_offset * frag->fr_var;
			break;
		case rs_align:
			size  +=   relax_align(size, frag->fr_offset);
		}
		frag = frag->fr_next;
	}
	segment_info[idx].scnhdr.s_size = size;
	return size;
}


static unsigned int DEFUN(count_entries_in_chain,(idx),
			  unsigned int idx)
{
	unsigned int nrelocs;
	fixS *fixup_ptr;

	/* Count the relocations */
	fixup_ptr = segment_info[idx].fix_root;
	nrelocs = 0;
	while (fixup_ptr != (fixS *)NULL)
	    {
		    if (TC_COUNT_RELOC(fixup_ptr))
			{

#ifdef TC_A29K

				if (fixup_ptr->fx_r_type == RELOC_CONSTH)
				    nrelocs+=2;
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

/* output all the relocations for a section */
void DEFUN(do_relocs_for,(abfd, file_cursor),
	   bfd *abfd AND
	   unsigned long *file_cursor)
{
	unsigned int nrelocs;
	unsigned int idx;

	for (idx = SEG_E0; idx < SEG_E9; idx++)
	    {
		    if (segment_info[idx].scnhdr.s_name[0])
			{

				struct external_reloc *ext_ptr;
				struct external_reloc *external_reloc_vec;
				unsigned int external_reloc_size;
				unsigned int count = 0;
				unsigned int base  = segment_info[idx].scnhdr.s_paddr;
				fixS *   fix_ptr = segment_info[idx].fix_root;
				nrelocs = count_entries_in_chain(idx);
				external_reloc_size = nrelocs * RELSZ;
				external_reloc_vec =
				    (struct external_reloc*)malloc(external_reloc_size);



				ext_ptr = external_reloc_vec;

				/* Fill in the internal coff style reloc struct from the
				   internal fix list */
				while (fix_ptr)
				    {
					    symbolS *symbol_ptr;
					    struct internal_reloc intr;

					    /* Only output some of the relocations */
					    if (TC_COUNT_RELOC(fix_ptr))
						{
#ifdef TC_RELOC_MANGLE
							TC_RELOC_MANGLE(fix_ptr, &intr, base);

#else
							symbolS *dot;
							symbol_ptr = fix_ptr->fx_addsy;

							intr.r_type = TC_COFF_FIX2RTYPE(fix_ptr);
							intr.r_vaddr =
							    base + fix_ptr->fx_frag->fr_address + fix_ptr->fx_where ;

							intr.r_offset = fix_ptr->fx_offset;

							intr.r_offset = 0;

							/* Turn the segment of the symbol into an offset
							 */
							if (symbol_ptr)
							    {
								    dot = segment_info[S_GET_SEGMENT(symbol_ptr)].dot;
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

							(void)bfd_coff_swap_reloc_out(abfd, &intr, ext_ptr);
							ext_ptr++;

#if defined(TC_A29K)
							/* The 29k has a special kludge for the high 16 bit reloc.
							   Two relocations are emmited, R_IHIHALF, and
							   R_IHCONST. The second one doesn't contain a symbol,
							   but uses the value for offset */

							if (intr.r_type == R_IHIHALF)
							    {
								    /* now emit the second bit */
								    intr.r_type = R_IHCONST;
								    intr.r_symndx = fix_ptr->fx_addnumber;
								    (void)bfd_coff_swap_reloc_out(abfd,&intr,ext_ptr);
								    ext_ptr++;
							    }
#endif
						}

					    fix_ptr = fix_ptr->fx_next;
				    }

				/* Write out the reloc table */
				segment_info[idx].scnhdr.s_relptr = *file_cursor;
				segment_info[idx].scnhdr.s_nreloc = nrelocs;
				bfd_write((PTR)external_reloc_vec, 1, external_reloc_size, abfd);
				*file_cursor += external_reloc_size;
				free( external_reloc_vec);
			}
	    }
}


/* run through a frag chain and write out the data to go with it, fill
   in the scnhdrs with the info on the file postions
   */
static void DEFUN(fill_section,(abfd, filehdr, file_cursor),
		  bfd *abfd AND
		  struct internal_filehdr *filehdr AND
		  unsigned long *file_cursor)
{

	unsigned int i;
	unsigned int paddr = 0;

	for (i = SEG_E0; i < SEG_UNKNOWN; i++)
	    {
		    unsigned int offset = 0;

		    struct internal_scnhdr *s = &( segment_info[i].scnhdr);

		    if (s->s_name[0])
			{
				fragS *frag = segment_info[i].frchainP->frch_root;
				char *buffer =  malloc(s->s_size);
				s->s_scnptr = *file_cursor;
				s->s_paddr =  paddr;
				s->s_vaddr =  paddr;

				s->s_flags = STYP_REG;
				if (strcmp(s->s_name,".text") == 0)
				    s->s_flags |= STYP_TEXT;
				else if (strcmp(s->s_name,".data") == 0)
				    s->s_flags |= STYP_DATA;
				else if (strcmp(s->s_name,".bss") == 0)
				    s->s_flags |= STYP_BSS | STYP_NOLOAD;

				while (frag) {
					unsigned int fill_size;
					switch (frag->fr_type) {

					case rs_fill:
					case rs_align:
					case rs_org:
						if (frag->fr_fix)
						    {
							    memcpy(buffer + frag->fr_address,
								   frag->fr_literal,
								   frag->fr_fix);
							    offset += frag->fr_fix;
						    }

						fill_size = frag->fr_var;
						if (fill_size)
						    {
							    unsigned int count ;
							    unsigned int off = frag->fr_fix;
							    for (count = frag->fr_offset; count;  count--)
								{
									memcpy(buffer +  frag->fr_address  + off,
									       frag->fr_literal + frag->fr_fix,
									       fill_size);
									off += fill_size;
									offset += fill_size;

								}

						    }
						break;
					default:
						abort();
					}
					frag = frag->fr_next;
				}


				bfd_write(buffer, s->s_size,1,abfd);
				free(buffer);

				*file_cursor += s->s_size;
				paddr += s->s_size;
			}
	    }

}



/* Coff file generation & utilities */


static void
    DEFUN(coff_header_append,(abfd, filehdr, aouthdr),
	  bfd *abfd AND
	  struct internal_filehdr *filehdr AND
	  struct internal_aouthdr *aouthdr)
{
	unsigned int i;
	char buffer[1000];
	char buffero[1000];

	bfd_seek(abfd, 0, 0);
#if 0
	filehdr.f_opthdr = bfd_coff_swap_aouthdr_out(abfd, aouthdr,
						     buffero);
#else
	filehdr->f_opthdr = 0;
#endif
	i = bfd_coff_swap_filehdr_out(abfd, filehdr, buffer);

	bfd_write(buffer, i ,1, abfd);
	bfd_write(buffero, filehdr->f_opthdr, 1, abfd);

	for (i = SEG_E0; i < SEG_E9; i++)
	    {
		    if (segment_info[i].scnhdr.s_name[0])
			{
				unsigned int size =
				    bfd_coff_swap_scnhdr_out(abfd,
							     &(segment_info[i].scnhdr),
							     buffer);
				bfd_write(buffer, size, 1, abfd);
			}
	    }
}


char *
    DEFUN(symbol_to_chars,(abfd, where, symbolP),
	  bfd*abfd AND
	  char *where AND
	  symbolS *symbolP)
{
	unsigned int numaux = symbolP->sy_symbol.ost_entry.n_numaux;
	unsigned int i;

	/* Turn any symbols with register attributes into abs symbols */
	if (S_GET_SEGMENT(symbolP) == SEG_REGISTER)
	    {
		    S_SET_SEGMENT(symbolP, SEG_ABSOLUTE);
	    }
	/* At the same time, relocate all symbols to their output value */

	S_SET_VALUE(symbolP,
		    segment_info[S_GET_SEGMENT(symbolP)].scnhdr.s_paddr
		    + S_GET_VALUE(symbolP));

	where += bfd_coff_swap_sym_out(abfd, &symbolP->sy_symbol.ost_entry,
				       where);

	for (i = 0; i < numaux; i++)
	    {
		    where += bfd_coff_swap_aux_out(abfd,
						   &symbolP->sy_symbol.ost_auxent[i],
						   S_GET_DATA_TYPE(symbolP),
						   S_GET_STORAGE_CLASS(symbolP),
						   where);
	    }
	return where;

}




void obj_symbol_new_hook(symbolP)
symbolS *symbolP;
{
	char underscore = 0;      /* Symbol has leading _ */

	/* Effective symbol */
	/* Store the pointer in the offset. */
	S_SET_ZEROES(symbolP, 0L);
	S_SET_DATA_TYPE(symbolP, T_NULL);
	S_SET_STORAGE_CLASS(symbolP, 0);
	S_SET_NUMBER_AUXILIARY(symbolP, 0);
	/* Additional information */
	symbolP->sy_symbol.ost_flags = 0;
	/* Auxiliary entries */
	memset((char*) &symbolP->sy_symbol.ost_auxent[0], '\0', AUXESZ);

#ifdef STRIP_UNDERSCORE
	/* Remove leading underscore at the beginning of the symbol.
	 * This is to be compatible with the standard librairies.
	 */
	if (*S_GET_NAME(symbolP) == '_') {
		underscore = 1;
		S_SET_NAME(symbolP, S_GET_NAME(symbolP) + 1);
	} /* strip underscore */
#endif /* STRIP_UNDERSCORE */

	if (S_IS_STRING(symbolP))
	    SF_SET_STRING(symbolP);
	if (!underscore && S_IS_LOCAL(symbolP))
	    SF_SET_LOCAL(symbolP);

	return;
} /* obj_symbol_new_hook() */

/* stack stuff */
stack* stack_init(chunk_size, element_size)
unsigned long chunk_size;
unsigned long element_size;
{
	stack* st;

	if ((st = (stack*)malloc(sizeof(stack))) == (stack*)0)
	    return (stack*)0;
	if ((st->data = malloc(chunk_size)) == (char*)0) {
		free(st);
		return (stack*)0;
	}
	st->pointer = 0;
	st->size = chunk_size;
	st->chunk_size = chunk_size;
	st->element_size = element_size;
	return st;
} /* stack_init() */

void stack_delete(st)
stack* st;
{
	free(st->data);
	free(st);
}

char *stack_push(st, element)
stack *st;
char *element;
{
	if (st->pointer + st->element_size >= st->size) {
		st->size += st->chunk_size;
		if ((st->data = xrealloc(st->data, st->size)) == (char*)0)
		    return (char*)0;
	}
	memcpy(st->data + st->pointer, element, st->element_size);
	st->pointer += st->element_size;
	return st->data + st->pointer;
} /* stack_push() */

char* stack_pop(st)
stack* st;
{
	if ((st->pointer -= st->element_size) < 0) {
		st->pointer = 0;
		return (char*)0;
	}

	return st->data + st->pointer;
}

char* stack_top(st)
stack* st;
{
	return st->data + st->pointer - st->element_size;
}


/*
 * Handle .ln directives.
 */

static void obj_coff_ln()
{
	int l;

	if (def_symbol_in_progress != NULL) {
		as_warn(".ln pseudo-op inside .def/.endef: ignored.");
		demand_empty_rest_of_line();
		return;
	}				/* wrong context */

	c_line_new(0,
		   obstack_next_free(&frags) - frag_now->fr_literal,
		   l = get_absolute_expression(),
		   frag_now);
#ifndef NO_LISTING
	{
		extern int listing;

		if (listing)
		    {
			    listing_source_line(l + line_base - 1);
		    }

	}
#endif
	demand_empty_rest_of_line();
	return;
}				/* obj_coff_line() */

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
    DEFUN(obj_coff_def,(what),
	  int what)
{
	char name_end;		/* Char after the end of name */
	char *symbol_name;		/* Name of the debug symbol */
	char *symbol_name_copy;	/* Temporary copy of the name */
	unsigned int symbol_name_length;
	/*$char*	directiveP;$ */		/* Name of the pseudo opcode */
	/*$char directive[MAX_DIRECTIVE];$ */ /* Backup of the directive */
	/*$char end = 0;$ */ /* If 1, stop parsing */

	if (def_symbol_in_progress != NULL) {
		as_warn(".def pseudo-op used inside of .def/.endef: ignored.");
		demand_empty_rest_of_line();
		return;
	}			/* if not inside .def/.endef */

	SKIP_WHITESPACES();

	def_symbol_in_progress = (symbolS *) obstack_alloc(&notes, sizeof(*def_symbol_in_progress));
	memset(def_symbol_in_progress, '\0', sizeof(*def_symbol_in_progress));

	symbol_name = input_line_pointer;
	name_end = get_symbol_end();
	symbol_name_length = strlen(symbol_name);
	symbol_name_copy = xmalloc(symbol_name_length + 1);
	strcpy(symbol_name_copy, symbol_name);

	/* Initialize the new symbol */
#ifdef STRIP_UNDERSCORE
	S_SET_NAME(def_symbol_in_progress, (*symbol_name_copy == '_'
					    ? symbol_name_copy + 1
					    : symbol_name_copy));
#else				/* STRIP_UNDERSCORE */
	S_SET_NAME(def_symbol_in_progress, symbol_name_copy);
#endif				/* STRIP_UNDERSCORE */
	/* free(symbol_name_copy); */
	def_symbol_in_progress->sy_name_offset = ~0;
	def_symbol_in_progress->sy_number = ~0;
	def_symbol_in_progress->sy_frag = &zero_address_frag;

	if (S_IS_STRING(def_symbol_in_progress)) {
		SF_SET_STRING(def_symbol_in_progress);
	}			/* "long" name */

	*input_line_pointer = name_end;

	demand_empty_rest_of_line();
	return;
}				/* obj_coff_def() */

unsigned int dim_index;
static void
    DEFUN_VOID(obj_coff_endef)
{
	symbolS *symbolP = 0;
	/* DIM BUG FIX sac@cygnus.com */
	dim_index =0;
	if (def_symbol_in_progress == NULL) {
		as_warn(".endef pseudo-op used outside of .def/.endef: ignored.");
		demand_empty_rest_of_line();
		return;
	}			/* if not inside .def/.endef */

	/* Set the section number according to storage class. */
	switch (S_GET_STORAGE_CLASS(def_symbol_in_progress)) {
	case C_STRTAG:
	case C_ENTAG:
	case C_UNTAG:
		SF_SET_TAG(def_symbol_in_progress);
		/* intentional fallthrough */
	case C_FILE:
	case C_TPDEF:
		SF_SET_DEBUG(def_symbol_in_progress);
		S_SET_SEGMENT(def_symbol_in_progress, SEG_DEBUG);
		break;

	case C_EFCN:
		SF_SET_LOCAL(def_symbol_in_progress); /* Do not emit this symbol. */
		/* intentional fallthrough */
	case C_BLOCK:
		SF_SET_PROCESS(def_symbol_in_progress); /* Will need processing before writing */
		/* intentional fallthrough */
	case C_FCN:
		S_SET_SEGMENT(def_symbol_in_progress, SEG_E0);

		if (def_symbol_in_progress->sy_symbol.ost_entry.n_name[1] == 'b') { /* .bf */
			if (function_lineoff < 0) {
				fprintf(stderr, "`.bf' symbol without preceding function\n");
			}	/* missing function symbol */
			SA_GET_SYM_LNNOPTR(def_symbol_in_progress) = function_lineoff;
			SF_SET_PROCESS(def_symbol_in_progress); /* Will need relocating */
			function_lineoff = -1;
		}
		break;

#ifdef C_AUTOARG
	case C_AUTOARG:
#endif				/* C_AUTOARG */
	case C_AUTO:
	case C_REG:
	case C_MOS:
	case C_MOE:
	case C_MOU:
	case C_ARG:
	case C_REGPARM:
	case C_FIELD:
	case C_EOS:
		SF_SET_DEBUG(def_symbol_in_progress);
		S_SET_SEGMENT(def_symbol_in_progress, SEG_ABSOLUTE);
		break;

	case C_EXT:
	case C_STAT:
	case C_LABEL:
		/* Valid but set somewhere else (s_comm, s_lcomm, colon) */
		break;

	case C_USTATIC:
	case C_EXTDEF:
	case C_ULABEL:
		as_warn("unexpected storage class %d", S_GET_STORAGE_CLASS(def_symbol_in_progress));
		break;
	}			/* switch on storage class */

	/* Now that we have built a debug symbol, try to
	   find if we should merge with an existing symbol
	   or not.  If a symbol is C_EFCN or SEG_ABSOLUTE or
	   untagged SEG_DEBUG it never merges. */

	/* Two cases for functions.  Either debug followed
	   by definition or definition followed by debug.
	   For definition first, we will merge the debug
	   symbol into the definition.  For debug first, the
	   lineno entry MUST point to the definition
	   function or else it will point off into space
	   when crawl_symbols() merges the debug
	   symbol into the real symbol.  Therefor, let's
	   presume the debug symbol is a real function
	   reference. */

	/* FIXME-SOON If for some reason the definition
	   label/symbol is never seen, this will probably
	   leave an undefined symbol at link time. */

	if (S_GET_STORAGE_CLASS(def_symbol_in_progress) == C_EFCN
	    || (S_GET_SEGMENT(def_symbol_in_progress) == SEG_DEBUG
		&& !SF_GET_TAG(def_symbol_in_progress))
	    || S_GET_SEGMENT(def_symbol_in_progress) == SEG_ABSOLUTE
	    || (symbolP = symbol_find_base(S_GET_NAME(def_symbol_in_progress), DO_NOT_STRIP)) == NULL) {

		symbol_append(def_symbol_in_progress, symbol_lastP, &symbol_rootP, &symbol_lastP);

	} else {
		/* This symbol already exists, merge the
		   newly created symbol into the old one.
		   This is not mandatory. The linker can
		   handle duplicate symbols correctly. But I
		   guess that it save a *lot* of space if
		   the assembly file defines a lot of
		   symbols. [loic] */

		/* The debug entry (def_symbol_in_progress)
		   is merged into the previous definition. */

		c_symbol_merge(def_symbol_in_progress, symbolP);
		/* FIXME-SOON Should *def_symbol_in_progress be free'd? xoxorich. */
		def_symbol_in_progress = symbolP;

		if (SF_GET_FUNCTION(def_symbol_in_progress)
		    || SF_GET_TAG(def_symbol_in_progress)) {
			/* For functions, and tags, the symbol *must* be where the debug symbol
			   appears.  Move the existing symbol to the current place. */
			/* If it already is at the end of the symbol list, do nothing */
			if (def_symbol_in_progress != symbol_lastP) {
				symbol_remove(def_symbol_in_progress, &symbol_rootP, &symbol_lastP);
				symbol_append(def_symbol_in_progress, symbol_lastP, &symbol_rootP, &symbol_lastP);
			}	/* if not already in place */
		}		/* if function */
	}			/* normal or mergable */

	if (SF_GET_TAG(def_symbol_in_progress)
	    && symbol_find_base(S_GET_NAME(def_symbol_in_progress), DO_NOT_STRIP) == NULL) {
		tag_insert(S_GET_NAME(def_symbol_in_progress), def_symbol_in_progress);
	}			/* If symbol is a {structure,union} tag, associate symbol to its name. */

	if (SF_GET_FUNCTION(def_symbol_in_progress)) {
		know(sizeof(def_symbol_in_progress) <= sizeof(long));
		function_lineoff
		    = c_line_new(def_symbol_in_progress,0, 0, &zero_address_frag);



		SF_SET_PROCESS(def_symbol_in_progress);

		if (symbolP == NULL) {
			/* That is, if this is the first
			   time we've seen the function... */
			symbol_table_insert(def_symbol_in_progress);
		}		/* definition follows debug */
	}			/* Create the line number entry pointing to the function being defined */

	def_symbol_in_progress = NULL;
	demand_empty_rest_of_line();
	return;
}				/* obj_coff_endef() */

static void
    DEFUN_VOID(obj_coff_dim)
{
	register int dim_index;

	if (def_symbol_in_progress == NULL)
	    {
		    as_warn(".dim pseudo-op used outside of .def/.endef: ignored.");
		    demand_empty_rest_of_line();
		    return;
	    }				/* if not inside .def/.endef */

	S_SET_NUMBER_AUXILIARY(def_symbol_in_progress, 1);

	for (dim_index = 0; dim_index < DIMNUM; dim_index++)
	    {
		    SKIP_WHITESPACES();
		    SA_SET_SYM_DIMEN(def_symbol_in_progress, dim_index, get_absolute_expression());

		    switch (*input_line_pointer)
			{

			case ',':
				input_line_pointer++;
				break;

			default:
				as_warn("badly formed .dim directive ignored");
				/* intentional fallthrough */
			case '\n':
			case ';':
				dim_index = DIMNUM;
				break;
			}			/* switch on following character */
	    }				/* for each dimension */

	demand_empty_rest_of_line();
	return;
}				/* obj_coff_dim() */

static void obj_coff_line()
{
	int this_base;

	if (def_symbol_in_progress == NULL) {
		obj_coff_ln();
		return;
	}				/* if it looks like a stabs style line */

	this_base = get_absolute_expression();
	if (this_base > line_base)
	    {
		    line_base = this_base;
	    }


#ifndef NO_LISTING
	{
		extern int listing;
		if (listing && 0) {
			listing_source_line(line_base);
		}
	}
#endif
	S_SET_NUMBER_AUXILIARY(def_symbol_in_progress, 1);
	SA_SET_SYM_LNNO(def_symbol_in_progress, line_base);

	demand_empty_rest_of_line();
	return;
}				/* obj_coff_line() */

static void obj_coff_size() {
	if (def_symbol_in_progress == NULL) {
		as_warn(".size pseudo-op used outside of .def/.endef ignored.");
		demand_empty_rest_of_line();
		return;
	} /* if not inside .def/.endef */

	S_SET_NUMBER_AUXILIARY(def_symbol_in_progress, 1);
	SA_SET_SYM_SIZE(def_symbol_in_progress, get_absolute_expression());
	demand_empty_rest_of_line();
	return;
} /* obj_coff_size() */

static void obj_coff_scl() {
	if (def_symbol_in_progress == NULL) {
		as_warn(".scl pseudo-op used outside of .def/.endef ignored.");
		demand_empty_rest_of_line();
		return;
	} /* if not inside .def/.endef */

	S_SET_STORAGE_CLASS(def_symbol_in_progress, get_absolute_expression());
	demand_empty_rest_of_line();
	return;
} /* obj_coff_scl() */

static void obj_coff_tag() {
	char *symbol_name;
	char name_end;

	if (def_symbol_in_progress == NULL) {
		as_warn(".tag pseudo-op used outside of .def/.endef ignored.");
		demand_empty_rest_of_line();
		return;
	} /* if not inside .def/.endef */

	S_SET_NUMBER_AUXILIARY(def_symbol_in_progress, 1);
	symbol_name = input_line_pointer;
	name_end = get_symbol_end();

	/* Assume that the symbol referred to by .tag is always defined. */
	/* This was a bad assumption.  I've added find_or_make. xoxorich. */
	SA_SET_SYM_TAGNDX(def_symbol_in_progress, (long) tag_find_or_make(symbol_name));
	if (SA_GET_SYM_TAGNDX(def_symbol_in_progress) == 0L) {
		as_warn("tag not found for .tag %s", symbol_name);
	} /* not defined */

	SF_SET_TAGGED(def_symbol_in_progress);
	*input_line_pointer = name_end;

	demand_empty_rest_of_line();
	return;
} /* obj_coff_tag() */

static void obj_coff_type() {
	if (def_symbol_in_progress == NULL) {
		as_warn(".type pseudo-op used outside of .def/.endef ignored.");
		demand_empty_rest_of_line();
		return;
	} /* if not inside .def/.endef */

	S_SET_DATA_TYPE(def_symbol_in_progress, get_absolute_expression());

	if (ISFCN(S_GET_DATA_TYPE(def_symbol_in_progress)) &&
	    S_GET_STORAGE_CLASS(def_symbol_in_progress) != C_TPDEF) {
		SF_SET_FUNCTION(def_symbol_in_progress);
	} /* is a function */

	demand_empty_rest_of_line();
	return;
} /* obj_coff_type() */

static void obj_coff_val() {
	if (def_symbol_in_progress == NULL) {
		as_warn(".val pseudo-op used outside of .def/.endef ignored.");
		demand_empty_rest_of_line();
		return;
	} /* if not inside .def/.endef */

	if (is_name_beginner(*input_line_pointer)) {
		char *symbol_name = input_line_pointer;
		char name_end = get_symbol_end();

		if (!strcmp(symbol_name, ".")) {
			def_symbol_in_progress->sy_frag = frag_now;
			S_SET_VALUE(def_symbol_in_progress, obstack_next_free(&frags) - frag_now->fr_literal);
			/* If the .val is != from the .def (e.g. statics) */
		} else if (strcmp(S_GET_NAME(def_symbol_in_progress), symbol_name)) {
			def_symbol_in_progress->sy_forward = symbol_find_or_make(symbol_name);

			/* If the segment is undefined when the forward
			   reference is solved, then copy the segment id
			   from the forward symbol. */
			SF_SET_GET_SEGMENT(def_symbol_in_progress);
		}
		/* Otherwise, it is the name of a non debug symbol and its value will be calculated later. */
		*input_line_pointer = name_end;
	} else {
		S_SET_VALUE(def_symbol_in_progress, get_absolute_expression());
	} /* if symbol based */

	demand_empty_rest_of_line();
	return;
} /* obj_coff_val() */

/*
 * Maintain a list of the tagnames of the structres.
 */

static void tag_init() {
	tag_hash = hash_new();
	return ;
} /* tag_init() */

static void tag_insert(name, symbolP)
char *name;
symbolS *symbolP;
{
	register char *	error_string;

	if (*(error_string = hash_jam(tag_hash, name, (char *)symbolP))) {
		as_fatal("Inserting \"%s\" into structure table failed: %s",
			 name, error_string);
	}
	return ;
} /* tag_insert() */

static symbolS *tag_find_or_make(name)
char *name;
{
	symbolS *symbolP;

	if ((symbolP = tag_find(name)) == NULL) {
		symbolP = symbol_new(name,
				     SEG_UNKNOWN,
				     0,
				     &zero_address_frag);

		tag_insert(S_GET_NAME(symbolP), symbolP);
		symbol_table_insert(symbolP);
	} /* not found */

	return(symbolP);
} /* tag_find_or_make() */

static symbolS *tag_find(name)
char *name;
{
#ifdef STRIP_UNDERSCORE
	if (*name == '_') name++;
#endif /* STRIP_UNDERSCORE */
	return((symbolS*)hash_find(tag_hash, name));
} /* tag_find() */

void obj_read_begin_hook() {
	/* These had better be the same.  Usually 18 bytes. */
#ifndef BFD_HEADERS
	know(sizeof(SYMENT) == sizeof(AUXENT));
	know(SYMESZ == AUXESZ);
#endif
	tag_init();

	return;
} /* obj_read_begin_hook() */

/* This function runs through the symbol table and puts all the
   externals onto another chain */

/* The chain of externals */
symbolS *symbol_externP = NULL;
symbolS *symbol_extern_lastP = NULL;

stack*block_stack;
symbolS *last_functionP = NULL;
symbolS *last_tagP;


static unsigned int DEFUN_VOID(yank_symbols)
{
	symbolS *symbolP;
	unsigned int symbol_number =0;

	for (symbolP = symbol_rootP;
	     symbolP;
	     symbolP = symbolP ? symbol_next(symbolP) : symbol_rootP) {
		if (!SF_GET_DEBUG(symbolP)) {
			/* Debug symbols do not need all this rubbish */
			symbolS* real_symbolP;

			/* L* and C_EFCN symbols never merge. */
			if (!SF_GET_LOCAL(symbolP)
			    && (real_symbolP = symbol_find_base(S_GET_NAME(symbolP), DO_NOT_STRIP))
			    && real_symbolP != symbolP) {
				/* FIXME-SOON: where do dups come from?
				   Maybe tag references before definitions? xoxorich. */
				/* Move the debug data from the debug symbol to the
				   real symbol. Do NOT do the oposite (i.e. move from
				   real symbol to debug symbol and remove real symbol from the
				   list.) Because some pointers refer to the real symbol
				   whereas no pointers refer to the debug symbol. */
				c_symbol_merge(symbolP, real_symbolP);
				/* Replace the current symbol by the real one */
				/* The symbols will never be the last or the first
				   because : 1st symbol is .file and 3 last symbols are
				   .text, .data, .bss */
				symbol_remove(real_symbolP, &symbol_rootP, &symbol_lastP);
				symbol_insert(real_symbolP, symbolP, &symbol_rootP, &symbol_lastP);
				symbol_remove(symbolP, &symbol_rootP, &symbol_lastP);
				symbolP = real_symbolP;
			}			/* if not local but dup'd */

			if (flagseen['R'] && (S_GET_SEGMENT(symbolP) == SEG_E1)) {
				S_SET_SEGMENT(symbolP, SEG_E0);
			}			/* push data into text */

			S_SET_VALUE(symbolP,
				    S_GET_VALUE(symbolP) + symbolP->sy_frag->fr_address);

			if (!S_IS_DEFINED(symbolP) && !SF_GET_LOCAL(symbolP))
			    {
				    S_SET_EXTERNAL(symbolP);
			    }
			else if (S_GET_STORAGE_CLASS(symbolP) == C_NULL)
			    {
				    if (S_GET_SEGMENT(symbolP) == SEG_E0)
					{
						S_SET_STORAGE_CLASS(symbolP, C_LABEL);
					}
				    else
					{
						S_SET_STORAGE_CLASS(symbolP, C_STAT);
					}
			    }

			/* Mainly to speed up if not -g */
			if (SF_GET_PROCESS(symbolP))
			    {
				    /* Handle the nested blocks auxiliary info. */
				    if (S_GET_STORAGE_CLASS(symbolP) == C_BLOCK) {
					    if (!strcmp(S_GET_NAME(symbolP), ".bb"))
						stack_push(block_stack, (char *) &symbolP);
					    else {		/* .eb */
						    register symbolS* begin_symbolP;
						    begin_symbolP = *(symbolS**)stack_pop(block_stack);
						    if (begin_symbolP == (symbolS*)0)
							as_warn("mismatched .eb");
						    else
							SA_SET_SYM_ENDNDX(begin_symbolP, symbol_number+2);
					    }
				    }
				    /* If we are able to identify the type of a function, and we
				       are out of a function (last_functionP == 0) then, the
				       function symbol will be associated with an auxiliary
				       entry. */
				    if (last_functionP == (symbolS*)0 &&
					SF_GET_FUNCTION(symbolP)) {
					    last_functionP = symbolP;

					    if (S_GET_NUMBER_AUXILIARY(symbolP) < 1) {
						    S_SET_NUMBER_AUXILIARY(symbolP, 1);
					    }		/* make it at least 1 */

					    /* Clobber possible stale .dim information. */
					    memset(symbolP->sy_symbol.ost_auxent[0].x_sym.x_fcnary.x_ary.x_dimen,
						  '\0', sizeof(symbolP->sy_symbol.ost_auxent[0].x_sym.x_fcnary.x_ary.x_dimen));
				    }
				    /* The C_FCN doesn't need any additional information.
				       I don't even know if this is needed for sdb. But the
				       standard assembler generates it, so...
				       */
				    if (S_GET_STORAGE_CLASS(symbolP) == C_EFCN) {
					    if (last_functionP == (symbolS*)0)
						as_fatal("C_EFCN symbol out of scope");
					    SA_SET_SYM_FSIZE(last_functionP,
							     (long)(S_GET_VALUE(symbolP) -
								    S_GET_VALUE(last_functionP)));
					    SA_SET_SYM_ENDNDX(last_functionP, symbol_number);
					    last_functionP = (symbolS*)0;
				    }
			    }
		} else if (SF_GET_TAG(symbolP)) {
			/* First descriptor of a structure must point to
			   the first slot after the structure description. */
			last_tagP = symbolP;

		} else if (S_GET_STORAGE_CLASS(symbolP) == C_EOS) {
			/* +2 take in account the current symbol */
			SA_SET_SYM_ENDNDX(last_tagP, symbol_number + 2);
		} else if (S_GET_STORAGE_CLASS(symbolP) == C_FILE) {
			if (S_GET_VALUE(symbolP)) {
				S_SET_VALUE((symbolS *) S_GET_VALUE(symbolP), symbol_number);
				S_SET_VALUE(symbolP, 0);
			}		/* no one points at the first .file symbol */
		}			/* if debug or tag or eos or file */

		/* We must put the external symbols apart. The loader
		   does not bomb if we do not. But the references in
		   the endndx field for a .bb symbol are not corrected
		   if an external symbol is removed between .bb and .be.
		   I.e in the following case :
		   [20] .bb endndx = 22
		   [21] foo external
		   [22] .be
		   ld will move the symbol 21 to the end of the list but
		   endndx will still be 22 instead of 21. */


		if (SF_GET_LOCAL(symbolP)) {
			/* remove C_EFCN and LOCAL (L...) symbols */
			/* next pointer remains valid */
			symbol_remove(symbolP, &symbol_rootP, &symbol_lastP);

		}
		else if (!S_IS_DEFINED(symbolP)
			 && !S_IS_DEBUG(symbolP)
			 && !SF_GET_STATICS(symbolP) &&
			 S_GET_STORAGE_CLASS(symbolP) == C_EXT)
		    { /* C_EXT && !SF_GET_FUNCTION(symbolP))  */
			    /* if external, Remove from the list */
			    symbolS *hold = symbol_previous(symbolP);

			    symbol_remove(symbolP, &symbol_rootP, &symbol_lastP);
			    symbol_clear_list_pointers(symbolP);
			    symbol_append(symbolP, symbol_extern_lastP, &symbol_externP, &symbol_extern_lastP);
			    symbolP = hold;
		    } else {
			    if (SF_GET_STRING(symbolP)) {
				    symbolP->sy_name_offset = string_byte_count;
				    string_byte_count += strlen(S_GET_NAME(symbolP)) + 1;
			    } else {
				    symbolP->sy_name_offset = 0;
			    }		/* fix "long" names */

			    symbolP->sy_number = symbol_number;
			    symbol_number += 1 + S_GET_NUMBER_AUXILIARY(symbolP);
		    }			/* if local symbol */
	}				/* traverse the symbol list */
	return symbol_number;

}


static unsigned int DEFUN_VOID(glue_symbols)
{
	unsigned int symbol_number = 0;
	symbolS *symbolP;
	for (symbolP = symbol_externP; symbol_externP;) {
		symbolS *tmp = symbol_externP;

		/* append */
		symbol_remove(tmp, &symbol_externP, &symbol_extern_lastP);
		symbol_append(tmp, symbol_lastP, &symbol_rootP, &symbol_lastP);

		/* and process */
		if (SF_GET_STRING(tmp)) {
			tmp->sy_name_offset = string_byte_count;
			string_byte_count += strlen(S_GET_NAME(tmp)) + 1;
		} else {
			tmp->sy_name_offset = 0;
		}			/* fix "long" names */

		tmp->sy_number = symbol_number;
		symbol_number += 1 + S_GET_NUMBER_AUXILIARY(tmp);
	}				/* append the entire extern chain */
	return symbol_number;

}

static unsigned int DEFUN_VOID(tie_tags)
{
	unsigned int symbol_number = 0;

	symbolS*symbolP;
	for (symbolP = symbol_rootP; symbolP; symbolP =
	     symbol_next(symbolP))
	    {
		    symbolP->sy_number = symbol_number;



		    if (SF_GET_TAGGED(symbolP))
			{
				SA_SET_SYM_TAGNDX
				    (symbolP,
				     ((symbolS*) SA_GET_SYM_TAGNDX(symbolP))->sy_number);
			}

		    symbol_number += 1 + S_GET_NUMBER_AUXILIARY(symbolP);
	    }
	return symbol_number;

}

static void
    DEFUN(crawl_symbols,(headers, abfd),
	  struct    internal_filehdr *headers AND
	  bfd *abfd)
{

	unsigned int i;
	unsigned int ptr = 0;


	symbolS *symbolP;

	/* Initialize the stack used to keep track of the matching .bb .be */

	block_stack = stack_init(512, sizeof(symbolS*));
	/* JF deal with forward references first... */
	for (symbolP = symbol_rootP;
	     symbolP;
	     symbolP = symbol_next(symbolP))
	    {

		    if (symbolP->sy_forward) {
			    S_SET_VALUE(symbolP, (S_GET_VALUE(symbolP)
						  + S_GET_VALUE(symbolP->sy_forward)
						  + symbolP->sy_forward->sy_frag->fr_address));

			    if (SF_GET_GET_SEGMENT(symbolP)) {
				    S_SET_SEGMENT(symbolP, S_GET_SEGMENT(symbolP->sy_forward));
			    }			/* forward segment also */

			    symbolP->sy_forward=0;
		    }				/* if it has a forward reference */
	    }				/* walk the symbol chain */


	/* The symbol list should be ordered according to the following sequence
	 * order :
	 * . .file symbol
	 * . debug entries for functions
	 * . fake symbols for the sections, including.text .data and .bss
	 * . defined symbols
	 * . undefined symbols
	 * But this is not mandatory. The only important point is to put the
	 * undefined symbols at the end of the list.
	 */

	if (symbol_rootP == NULL
	    || S_GET_STORAGE_CLASS(symbol_rootP) != C_FILE) {
		c_dot_file_symbol("fake");
	}
	/* Is there a .file symbol ? If not insert one at the beginning. */

	/*
	 * Build up static symbols for the sections, they are filled in later
	 */


	for (i = SEG_E0; i < SEG_E9; i++)
	    {
		    if (segment_info[i].scnhdr.s_name[0])
			{
				segment_info[i].dot =
				    c_section_symbol(segment_info[i].scnhdr.s_name,
						     i-SEG_E0+1);

			}
	    }


	/* Take all the externals out and put them into another chain */
	headers->f_nsyms =   yank_symbols();
	/* Take the externals and glue them onto the end.*/
	headers->f_nsyms +=  glue_symbols();

	headers->f_nsyms =   tie_tags();
	know(symbol_externP == NULL);
	know(symbol_extern_lastP == NULL);

	return;
}

/*
 * Find strings by crawling along symbol table chain.
 */

void DEFUN(w_strings,(where),
	   char *where)
{
	symbolS *symbolP;

	/* Gotta do md_ byte-ordering stuff for string_byte_count first - KWK */
	md_number_to_chars(where, string_byte_count, sizeof(string_byte_count));
	where += sizeof(string_byte_count);
	for (symbolP = symbol_rootP;
	     symbolP;
	     symbolP = symbol_next(symbolP))
	    {
		    unsigned int size;

		    if (SF_GET_STRING(symbolP)) {
			    size = strlen(S_GET_NAME(symbolP)) + 1;

			    memcpy(where,  S_GET_NAME(symbolP),size);
			    where += size;

		    }
	    }

}





static void
    DEFUN(do_linenos_for,(abfd, file_cursor),
	  bfd *abfd AND
	  unsigned long *file_cursor)
{
	unsigned int idx;

	for (idx = SEG_E0;  idx < SEG_E9; idx++)
	    {
		    segment_info_type *s = segment_info + idx;


		    if (s->scnhdr.s_nlnno != 0)
			{
				struct lineno_list *line_ptr ;

				struct external_lineno *buffer =
				    (struct external_lineno *)xmalloc(s->scnhdr.s_nlnno * LINESZ);

				struct external_lineno *dst= buffer;

				/* Run through the table we've built and turn it into its external
				   form, take this chance to remove duplicates */

				for (line_ptr = s->lineno_list_head;
				     line_ptr != (struct lineno_list *)NULL;
				     line_ptr = line_ptr->next)
				    {

					    if (line_ptr->line.l_lnno == 0)
						{
							/* Turn a pointer to a symbol into the symbols' index */
							line_ptr->line.l_addr.l_symndx =
							    ( (symbolS *)line_ptr->line.l_addr.l_symndx)->sy_number;
						}
					    else
						{
							line_ptr->line.l_addr.l_paddr += ((struct frag * )(line_ptr->frag))->fr_address;
						}


					    (void)  bfd_coff_swap_lineno_out(abfd, &(line_ptr->line), dst);
					    dst++;

				    }

				s->scnhdr.s_lnnoptr = *file_cursor;

				bfd_write(buffer, 1, s->scnhdr.s_nlnno* LINESZ, abfd);
				free(buffer);

				*file_cursor += s->scnhdr.s_nlnno * LINESZ;
			}
	    }
}


/* Now we run through the list of frag chains in a segment and
   make all the subsegment frags appear at the end of the
   list, as if the seg 0 was extra long */

static void DEFUN_VOID(remove_subsegs)
{
	unsigned int i;

	for (i = SEG_E0; i < SEG_UNKNOWN; i++)
	    {
		    frchainS *head =  segment_info[i].frchainP;
		    fragS dummy;
		    fragS * prev_frag = &dummy;

		    while (head && head->frch_seg == i)
			{
				prev_frag->fr_next = head->frch_root;
				prev_frag = head->frch_last;
				head = head->frch_next;
			}
		    prev_frag->fr_next = 0;
	    }
}


extern void DEFUN_VOID(write_object_file)
{
	int i;
	struct frchain *frchain_ptr;

	struct internal_filehdr filehdr;
	struct internal_aouthdr aouthdr;
	unsigned long file_cursor;
	bfd *abfd;
	unsigned int addr = 0;
	abfd = bfd_openw(out_file_name, TARGET_FORMAT);


	if (abfd == 0) {
		as_perror ("FATAL: Can't create %s", out_file_name);
		exit(42);
	}
	bfd_set_format(abfd, bfd_object);
	bfd_set_arch_mach(abfd, BFD_ARCH, 0);



	string_byte_count = 4;

	for (frchain_ptr = frchain_root;
	     frchain_ptr != (struct frchain *)NULL;
	     frchain_ptr = frchain_ptr->frch_next) {
		/* Run through all the sub-segments and align them up. Also close any
		   open frags. We tack a .fill onto the end of the frag chain so
		   that any .align's size can be worked by looking at the next
		   frag */

		subseg_new(frchain_ptr->frch_seg, frchain_ptr->frch_subseg);
#define SUB_SEGMENT_ALIGN 1
		frag_align(SUB_SEGMENT_ALIGN,0);
		frag_wane(frag_now);
		frag_now->fr_fix = 0;
		know( frag_now->fr_next == NULL );
	}


	remove_subsegs();


	for (i = SEG_E0; i < SEG_UNKNOWN; i++)
	    {
		    relax_segment(segment_info[i].frchainP->frch_root, i);
	    }





	filehdr.f_nscns = 0;

	/* Find out how big the sections are */
	for (i = SEG_E0; i < SEG_UNKNOWN; i++)
	    {

		    if (segment_info[i].scnhdr.s_name[0])
			{
				filehdr.f_nscns++;
			}
		    segment_info[i].scnhdr.s_paddr = addr;
		    if (i == SEG_E2) {
			    /* THis is a special case, we leave the size alone, which will have */
			    /* been made up from all and any lcomms seen */
		    }
		    else {
			    addr += size_section(abfd, i);
		    }
	    }



	/* Turn the gas native symbol table shape into a coff symbol table */
	crawl_symbols(&filehdr, abfd);
#ifndef TC_H8300
	for (i = SEG_E0; i < SEG_UNKNOWN; i++)
	    {
		    fixup_segment(segment_info[i].fix_root, i);
	    }
#endif

	file_cursor =   FILHSZ + SCNHSZ * filehdr.f_nscns ;

	bfd_seek(abfd, file_cursor, 0);


	do_relocs_for(abfd, &file_cursor);

	do_linenos_for(abfd, &file_cursor);


	/* Plant the data */

	fill_section(abfd,&filehdr, &file_cursor);

	filehdr.f_magic = COFF_MAGIC;
	filehdr.f_timdat = 0;
	filehdr.f_flags = 0;



	{

		unsigned int   symtable_size = filehdr.f_nsyms * SYMESZ;
		char *buffer1 = malloc(symtable_size + string_byte_count + 4);
		char *ptr = buffer1;
		filehdr.f_symptr = bfd_tell(abfd);
		w_symbols(abfd, buffer1, symbol_rootP);
		w_strings(buffer1 + symtable_size);
		bfd_write(buffer1, 1,symtable_size +  string_byte_count + 4, abfd);
		free(buffer1);

	}
	coff_header_append(abfd, &filehdr, &aouthdr);

	bfd_close_all_done(abfd);
}


static void DEFUN(change_to_section,(name, len, exp),
		  char *name AND
		  unsigned int len AND
		  unsigned int exp)
{
	unsigned int i;
	/* Find out if we've already got a section of this name etc */
	for (i = SEG_E0; i < SEG_E9 && segment_info[i].scnhdr.s_name[0] ; i++)
	    {
		    if (strncmp(segment_info[i].scnhdr.s_name, name, len) == 0)
			{
				subseg_new(i, exp);
				return;

			}
	    }
	/* No section, add one */
	strncpy(segment_info[i].scnhdr.s_name, name, 8);
	subseg_new(i, exp);
}

static void
    DEFUN_VOID(obj_coff_section)
{
	/* Strip out the section name */
	char *section_name ;
	char *section_name_end;
	char c;

	unsigned int len;
	unsigned int exp;

	section_name =  input_line_pointer;
	c =   get_symbol_end();
	section_name_end =  input_line_pointer;

	len = section_name_end - section_name ;
	input_line_pointer++;
	SKIP_WHITESPACE();
	if (c == ',')
	    {
		    exp = get_absolute_expression();
	    }
	else if ( *input_line_pointer == ',')
	    {

		    input_line_pointer++;
		    exp = get_absolute_expression();
	    }
	else
	    {
		    exp = 0;
	    }

	change_to_section(section_name, len,exp);
	*section_name_end = c;

}


static void obj_coff_text()
{
	change_to_section(".text",5, get_absolute_expression());
}


static void obj_coff_data()
{
	change_to_section(".data",5, get_absolute_expression());
}

void c_symbol_merge(debug, normal)
symbolS *debug;
symbolS *normal;
{
	S_SET_DATA_TYPE(normal, S_GET_DATA_TYPE(debug));
	S_SET_STORAGE_CLASS(normal, S_GET_STORAGE_CLASS(debug));

	if (S_GET_NUMBER_AUXILIARY(debug) > S_GET_NUMBER_AUXILIARY(normal)) {
		S_SET_NUMBER_AUXILIARY(normal, S_GET_NUMBER_AUXILIARY(debug));
	} /* take the most we have */

	if (S_GET_NUMBER_AUXILIARY(debug) > 0) {
		memcpy((char*)&normal->sy_symbol.ost_auxent[0], (char*)&debug->sy_symbol.ost_auxent[0], S_GET_NUMBER_AUXILIARY(debug) * AUXESZ);
	} /* Move all the auxiliary information */

	/* Move the debug flags. */
	SF_SET_DEBUG_FIELD(normal, SF_GET_DEBUG_FIELD(debug));
} /* c_symbol_merge() */

static int
    DEFUN(c_line_new,(symbol, paddr, line_number, frag),
	  symbolS *symbol AND
	  long paddr AND
	  unsigned short line_number AND
	  fragS* frag)
{
	struct lineno_list* new_line =
	    (struct lineno_list *)xmalloc(sizeof(struct lineno_list));

	segment_info_type *s =   segment_info + now_seg;
	new_line->line.l_lnno = line_number;

	if (line_number == 0)
	    {
		    new_line->line.l_addr.l_symndx = (long)symbol;
	    }
	else
	    {
		    new_line->line.l_addr.l_paddr = paddr;
	    }

	new_line->frag = (char*)frag;
	new_line->next = (struct lineno_list*)NULL;


	if (s->lineno_list_head == (struct lineno_list *)NULL)
	    {
		    s->lineno_list_head = new_line;
	    }
	else
	    {
		    s->lineno_list_tail->next = new_line;
	    }
	s->lineno_list_tail = new_line;
	return LINESZ * s->scnhdr.s_nlnno ++;
}

void c_dot_file_symbol(filename)
char *filename;
{
	symbolS* symbolP;

	symbolP = symbol_new(".file",
			     SEG_DEBUG,
			     0,
			     &zero_address_frag);

	S_SET_STORAGE_CLASS(symbolP, C_FILE);
	S_SET_NUMBER_AUXILIARY(symbolP, 1);
	SA_SET_FILE_FNAME(symbolP, filename);
#ifndef NO_LISTING
	{
		extern int listing;
		if (listing)
		    {
			    listing_source_file(filename);
		    }

	}

#endif
	SF_SET_DEBUG(symbolP);
	S_SET_VALUE(symbolP, (long) previous_file_symbol);

	previous_file_symbol = symbolP;

	/* Make sure that the symbol is first on the symbol chain */
	if (symbol_rootP != symbolP) {
		if (symbolP == symbol_lastP) {
			symbol_lastP = symbol_lastP->sy_previous;
		} /* if it was the last thing on the list */

		symbol_remove(symbolP, &symbol_rootP, &symbol_lastP);
		symbol_insert(symbolP, symbol_rootP, &symbol_rootP, &symbol_lastP);
		symbol_rootP = symbolP;
	} /* if not first on the list */

} /* c_dot_file_symbol() */

/*
 * Build a 'section static' symbol.
 */

symbolS *c_section_symbol(name,idx)
char *name;
int idx;
{
	symbolS *symbolP;

	symbolP = symbol_new(name,idx,
			     0,
			     &zero_address_frag);

	S_SET_STORAGE_CLASS(symbolP, C_STAT);
	S_SET_NUMBER_AUXILIARY(symbolP, 1);

	SF_SET_STATICS(symbolP);

	return symbolP;
} /* c_section_symbol() */

static void
    DEFUN(w_symbols,(abfd, where, symbol_rootP),
	  bfd *abfd AND
	  char *where AND
	  symbolS *symbol_rootP)
{
	symbolS *symbolP;
	unsigned int i;

	/* First fill in those values we have only just worked out */
	for (i = SEG_E0; i < SEG_E9; i++)
	    {
		    symbolP = segment_info[i].dot;
		    if (symbolP)
			{

				SA_SET_SCN_SCNLEN(symbolP, segment_info[i].scnhdr.s_size);
				SA_SET_SCN_NRELOC(symbolP, segment_info[i].scnhdr.s_nreloc);
				SA_SET_SCN_NLINNO(symbolP, segment_info[i].scnhdr.s_nlnno);

			}
	    }

	/*
	 * Emit all symbols left in the symbol chain.
	 */
	for (symbolP = symbol_rootP; symbolP; symbolP = symbol_next(symbolP)) {
		/* Used to save the offset of the name. It is used to point
		   to the string in memory but must be a file offset. */
		register char *	temp;

		tc_coff_symbol_emit_hook(symbolP);

		temp = S_GET_NAME(symbolP);
		if (SF_GET_STRING(symbolP)) {
			S_SET_OFFSET(symbolP, symbolP->sy_name_offset);
			S_SET_ZEROES(symbolP, 0);
		} else {
			memset(symbolP->sy_symbol.ost_entry.n_name, '\0', SYMNMLEN);
			strncpy(symbolP->sy_symbol.ost_entry.n_name, temp, SYMNMLEN);
		}
		where =  symbol_to_chars(abfd, where, symbolP);
		S_SET_NAME(symbolP,temp);
	}

}				/* w_symbols() */

static void DEFUN_VOID(obj_coff_lcomm)
{
	char *name;
	char c;
	int temp;
	char *p;
	symbolS *symbolP;
	name = input_line_pointer;



	c = get_symbol_end();
	p = input_line_pointer;
	*p = c;
	SKIP_WHITESPACE();
	if (*input_line_pointer != ',') {
		as_bad("Expected comma after name");
		ignore_rest_of_line();
		return;
	}
	if (*input_line_pointer == '\n') {
		as_bad("Missing size expression");
		return;
	}
	input_line_pointer++;
	if ((temp = get_absolute_expression ()) < 0) {
		as_warn("lcomm length (%d.) <0! Ignored.", temp);
		ignore_rest_of_line();
		return;
	}
	*p = 0;
	symbolP = symbol_find_or_make(name);
	S_SET_VALUE(symbolP, segment_info[SEG_E2].scnhdr.s_size);
	S_SET_SEGMENT(symbolP, SEG_E2);
	segment_info[SEG_E2].scnhdr.s_size += temp;
	S_SET_STORAGE_CLASS(symbolP, C_STAT);
	demand_empty_rest_of_line();
}


#if 1
static void DEFUN(fixup_segment,(fixP, this_segment_type),
		  register fixS *	fixP AND
		  segT		this_segment_type)
{
	register symbolS *add_symbolP;
	register symbolS *sub_symbolP;
	register long add_number;
	register int size;
	register char *place;
	register long where;
	register char pcrel;
	register fragS *fragP;
	register segT add_symbol_segment = SEG_ABSOLUTE;


	for ( ;  fixP;  fixP = fixP->fx_next)
	    {
		    fragP       = fixP->fx_frag;
		    know(fragP);
		    where	  = fixP->fx_where;
		    place       = fragP->fr_literal + where;
		    size	  = fixP->fx_size;
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
		    }			/* callj relocation */
#endif
		    sub_symbolP = fixP->fx_subsy;
		    add_number  = fixP->fx_offset;
		    pcrel	  = fixP->fx_pcrel;

		    if (add_symbolP) {
			    add_symbol_segment = S_GET_SEGMENT(add_symbolP);
		    }			/* if there is an addend */

		    if (sub_symbolP) {
			    if (!add_symbolP) {
				    /* Its just -sym */
				    if (S_GET_SEGMENT(sub_symbolP) != SEG_ABSOLUTE) {
					    as_bad("Negative of non-absolute symbol %s", S_GET_NAME(sub_symbolP));
				    }	/* not absolute */

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
#endif				/* TC_I960 */
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
		    }			/* if sub_symbolP */

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
#endif				/* TC_I960 */

				    add_number += S_GET_VALUE(add_symbolP);
				    add_number -= md_pcrel_from (fixP);
				    pcrel = 0; /* Lie. Don't want further pcrel processing. */
				    fixP->fx_addsy = NULL; /* No relocations please. */
			    } else
				{
					switch (add_symbol_segment)
					    {
					    case SEG_ABSOLUTE:
#ifdef TC_I960
						    reloc_callj(fixP); /* See comment about reloc_callj() above*/
#endif				/* TC_I960 */
						    add_number += S_GET_VALUE(add_symbolP);
						    fixP->fx_addsy = NULL;
						    add_symbolP = NULL;
						    break;
					    default:

						    add_number += S_GET_VALUE(add_symbolP) +
							segment_info[S_GET_SEGMENT(add_symbolP)].scnhdr.s_paddr ;
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
							    fixP->fx_addsy = NULL; /* No relocations please. */
							    continue;
						    } /* COBR */
#endif				/* TC_I960 */



						    break;


					    }	/* switch on symbol seg */
				}		/* if not in local seg */
		    }			/* if there was a + symbol */

		    if (pcrel) {
			    add_number -= md_pcrel_from(fixP);
			    if (add_symbolP == 0) {
				    fixP->fx_addsy = & abs_symbol;
			    }		/* if there's an add_symbol */
		    }			/* if pcrel */

		    if (!fixP->fx_bit_fixP) {
			    if ((size == 1
				 && (add_number & ~0xFF) && (add_number & ~0xFF != (-1 & ~0xFF))) ||
				(size == 2
				 && (add_number & ~0xFFFF) && (add_number & ~0xFFFF != (-1 & ~0xFFFF)))) {
				    as_bad("Value of %d too large for field of %d bytes at 0x%x",
					   add_number, size, fragP->fr_address + where);
			    }		/* generic error checking */
		    }			/* not a bit fix */
		    /* once this fix has been applied, we don't have to output anything
		       nothing more need be done -*/
		    md_apply_fix(fixP, add_number);

	    }				/* For each fixS in this segment. */


} /* fixup_segment() */
#endif

/*
 * Local Variables:
 * fill-column: 131
 * End:
 */

/* end of obj-coffbfd.c */
