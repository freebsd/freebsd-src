/* obj-format for ieee-695 records.
   Copyright (C) 1991, 1992 Free Software Foundation, Inc.

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
   the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA. */


/*
  created by

  steve chamberlain steve@cygnus.com
  */

/*
  this will hopefully become the port through which bfd and gas talk,
  for the moment, only ieee is known to work well.
  */

#include "bfd.h"
#include "as.h"
#include "subsegs.h"
#include "output-file.h"
#include "frags.h"

bfd *abfd;

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

/* calculate the size of the frag chain and create a bfd section
   to contain all of it */
static void DEFUN(size_section,(abfd, idx),
		  bfd *abfd AND
		  unsigned int idx)
{
	asection *sec;
	unsigned int size = 0;
	fragS *frag = segment_info[idx].frag_root;
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
	if (size) {
		char *name = segment_info[idx].name;
		if (name == (char *)NULL) {
			name = ".data";
		}
		segment_info[idx].user_stuff = (char *)(sec =  bfd_make_section(abfd, name));
		/* Make it output through itself */
		sec->output_section = sec;
		sec->flags |= SEC_HAS_CONTENTS;
		bfd_set_section_size(abfd, sec, size);
	}
}

/* run through a frag chain and write out the data to go with it */
static void DEFUN(fill_section,(abfd, idx),
		  bfd *abfd AND
		  unsigned int idx)
{
	asection *sec = segment_info[idx].user_stuff;
	if (sec) {
		fragS *frag = segment_info[idx].frag_root;
		unsigned int offset = 0;
		while (frag) {
			unsigned int fill_size;
			unsigned int count;
			switch (frag->fr_type) {
			case rs_fill:
			case rs_align:
			case rs_org:
				if (frag->fr_fix)
				    {
					    bfd_set_section_contents(abfd,
								     sec,
								     frag->fr_literal,
								     frag->fr_address,
								     frag->fr_fix);
				    }
				offset += frag->fr_fix;
				fill_size = frag->fr_var;
				if (fill_size)
				    {
					    unsigned int off = frag->fr_fix;
					    for (count = frag->fr_offset; count;  count--)
						{
							bfd_set_section_contents(abfd, sec,
										 frag->fr_literal +
										 frag->fr_fix,
										 frag->fr_address + off,
										 fill_size);
							off += fill_size;
						}
				    }
				break;
			default:
				abort();
			}
			frag = frag->fr_next;
		}
	}
}

/* Count the relocations in a chain */

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
		    fixup_ptr = fixup_ptr->fx_next;
		    nrelocs ++ ;
	    }
	return nrelocs;
}

/* output all the relocations for a section */
void DEFUN(do_relocs_for,(idx),
	   unsigned int idx)
{
	unsigned int nrelocs;
	arelent **reloc_ptr_vector;
	arelent *reloc_vector;
	asymbol **ptrs;
	asection *section = (asection *)(segment_info[idx].user_stuff);
	unsigned int i;
	fixS *from;
	if (section) {
		nrelocs = count_entries_in_chain(idx);

		reloc_ptr_vector = (arelent**)malloc((nrelocs+1) * sizeof(arelent *));
		reloc_vector = (arelent*)malloc(nrelocs * sizeof(arelent));
		ptrs = (asymbol **)malloc(nrelocs * sizeof(asymbol *));
		from = segment_info[idx].fix_root;
		for (i = 0; i < nrelocs; i++)
		    {
			    arelent *to = reloc_vector + i;
			    asymbol *s ;
			    reloc_ptr_vector[i] = to;
			    to->howto = (reloc_howto_type *)(from->fx_r_type);

			    /* We can't represent complicated things in a reloc yet */
			    /*	if (from->fx_addsy == 0 ||
				from->fx_subsy != 0) abort();
				*/
			    s = &( from->fx_addsy->sy_symbol.sy);
			    to->address = ((char *)( from->fx_frag->fr_address +
						    from->fx_where))
				- ((char *)(&(from->fx_frag->fr_literal)));
			    to->addend = from->fx_offset ;
			    /* If we know the symbol which we want to relocate to, turn this
			       reloaction into a section relative.

			       If this relocation is pcrelative, and we know the
			       destination, we still want to keep the relocation - since
			       the linker might relax some of the bytes, but it stops
			       being pc relative and turns into an absolute relocation.

			       */
			    if (s) {
				    if ((s->flags & BSF_UNDEFINED) == 0) {
					    to->section = s->section;
					    to->addend +=  s->value  ;
					    to->sym_ptr_ptr  = 0;
					    if (to->howto->pcrel_offset) {
						    /* This is a pcrel relocation, the addend should be adjusted */
						    to->addend -=  to->address +1;
					    }
				    }
				    else {
					    to->section = 0;
					    *ptrs = &(from->fx_addsy->sy_symbol.sy);
					    to->sym_ptr_ptr = ptrs;

					    if (to->howto->pcrel_offset) {
						    /* This is a pcrel relocation, the addend should be adjusted */
						    to->addend -=  to->address -1;
					    }
				    }

			    }
			    else {
				    to->section = 0;
			    }

			    ptrs++;
			    from = from->fx_next;
		    }

		/* attatch to the section */
		section->orelocation =  reloc_ptr_vector;
		section->reloc_count = nrelocs;
		section->flags |= SEC_LOAD;
	}
}

/* do the symbols.. */
static void DEFUN(do_symbols, (abfd),
		  bfd *abfd)
{
	extern symbolS *symbol_rootP;
	symbolS *ptr;
	asymbol **symbol_ptr_vec;
	asymbol *symbol_vec;
	unsigned int count = 0;
	unsigned int index;


	for (ptr = symbol_rootP;
	     ptr != (symbolS *)NULL;
	     ptr = ptr->sy_next)
	    {
		    if (SEG_NORMAL(ptr->sy_symbol.seg))
			{
				ptr->sy_symbol.sy.section =
				    (asection *)(segment_info[ptr->sy_symbol.seg].user_stuff);
				ptr->sy_symbol.sy.value += ptr->sy_frag->fr_address;
				if (ptr->sy_symbol.sy.flags == 0) {
					ptr->sy_symbol.sy.flags = BSF_LOCAL ;
				}
			}
		    else {
			    switch (ptr->sy_symbol.seg) {
			    case  SEG_ABSOLUTE:
				    ptr->sy_symbol.sy.flags   |= BSF_ABSOLUTE;
				    ptr->sy_symbol.sy.section = 0;
				    break;
			    case SEG_UNKNOWN:
				    ptr->sy_symbol.sy.flags = BSF_UNDEFINED ;
				    ptr->sy_symbol.sy.section = 0;
				    break;
			    default:
				    abort();
			    }
		    }
		    count++;
	    }
	symbol_ptr_vec = (asymbol **)malloc((count+1) * sizeof(asymbol *));

	index = 0;
	for (ptr = symbol_rootP;
	     ptr != (symbolS *)NULL;
	     ptr = ptr->sy_next)
	    {
		    symbol_ptr_vec[index] =  &(ptr->sy_symbol.sy);
		    index++;
	    }
	symbol_ptr_vec[index] =0;
	abfd->outsymbols = symbol_ptr_vec;
	abfd->symcount = count;
}

/* The generic as->bfd converter. Other backends may have special case
   code */

void DEFUN_VOID(bfd_as_write_hook)
{
	int i;

	for (i = SEG_E0; i < SEG_UNKNOWN; i++) {
		size_section(abfd, i);
	}


	for (i = SEG_E0; i < SEG_UNKNOWN; i++)
	    fill_section(abfd,i);

	do_symbols(abfd);

	for (i = SEG_E0; i < SEG_UNKNOWN; i++)
	    do_relocs_for(i);

}



S_GET_VALUE(x)
symbolS *x;
{
	return x->sy_symbol.sy.value;
}

S_SET_SEGMENT(x,y)
symbolS *x ;
int y;
{
	x->sy_symbol.seg = y;
}

S_IS_DEFINED(x)
symbolS *x;
{
	if (SEG_NORMAL(x->sy_symbol.seg))
	    {
		    return 1;
	    }
	switch (x->sy_symbol.seg)
	    {
	    case  SEG_UNKNOWN:
		    return 0;
	    default:
		    abort();
	    }
}

S_IS_EXTERNAL(x) { abort(); }
S_GET_DESC(x) { abort()  ; }

S_GET_SEGMENT(x)
symbolS *x;
{ return x->sy_symbol.seg;  }

S_SET_EXTERNAL(x)
symbolS *x;
{
	x->sy_symbol.sy.flags |= BSF_GLOBAL | BSF_EXPORT;
}

S_SET_NAME(x,y)
symbolS*x;
char *y; {
	x->sy_symbol.sy.name = y; }

S_SET_VALUE(s,v)
symbolS *s;
long v;
{
	s->sy_symbol.sy.value = v;
}

S_GET_OTHER(x) { abort() ;}
S_IS_DEBUG(x) { abort(); }

char *segment_name() { abort(); }

void obj_read_begin_hook() {  }

static void obj_ieee_section(ignore)
int ignore;
{
	extern char *input_line_pointer;
	extern char is_end_of_line[];
	char *p= input_line_pointer;
	char *s = p;
	int i;
	/* Look up the name, if it doesn't exist, make it */
	while (*p &&* p != ' ' && *p != ',' && !is_end_of_line[*p]) {
		p++;
	}
	for (i = SEG_E0; i < SEG_UNKNOWN; i++) {
		if (segment_info[i].hadone){
			if (strncmp(segment_info[i].name, s, p-s) == 0) {
				goto ok;

			}
		}
		else break;
	}
	if (i == SEG_UNKNOWN) {
		as_bad("too many sections");
		return;
	}

	segment_info[i].hadone = 1;
	segment_info[i].name = malloc(p-s + 1);
	memcpy(segment_info[i].name, s, p-s);
	segment_info[i].name[p-s] = 0;
 ok:
	subseg_new(i,0);
	while (!is_end_of_line[*p])
	    p++;
	input_line_pointer = p;

}


void cons();
void s_ignore();


/*
 *			stringer()
 *
 * We read 0 or more ',' seperated, double-quoted strings.
 *
 * Caller should have checked need_pass_2 is FALSE because we don't check it.
 */

void stringer();
void s_globl();
const pseudo_typeS obj_pseudo_table[] =
{
	{"section", obj_ieee_section, 0},
	{"data.b", cons, 1},
	{"data.w", cons, 2},
	{"data.l", cons, 4},
	{"export", s_globl, 0},
	{"option", s_ignore, 0},
	{"end", s_ignore, 0},
	{"import", s_ignore, 0},
	{"sdata", stringer, 0},
	0,

};



void obj_symbol_new_hook(symbolP)
symbolS *symbolP;
{
	symbolP->sy_symbol.sy.the_bfd = abfd;
}





#if 1
extern void DEFUN_VOID(write_object_file)
{
	int i;
	struct frchain *frchain_ptr;
	struct frag *frag_ptr;

	abfd = bfd_openw(out_file_name, "ieee");

	if (abfd == 0) {
		as_perror ("FATAL: Can't create %s", out_file_name);
		exit(42);
	}
	bfd_set_format(abfd, bfd_object);
	bfd_set_arch_mach(abfd, bfd_arch_h8300, 0);
	subseg_new(1,0);
	subseg_new(2,0);
	subseg_new(3,0);
	for (frchain_ptr = frchain_root;
	     frchain_ptr != (struct frchain *)NULL;
	     frchain_ptr = frchain_ptr->frch_next) {
		/* Run through all the sub-segments and align them up. Also close any
		   open frags. We tack a .fill onto the end of the frag chain so
		   that any .align's size can be worked by looking at the next
		   frag */

		subseg_new(frchain_ptr->frch_seg, frchain_ptr->frch_subseg);
#define SUB_SEGMENT_ALIGN 2
		frag_align(SUB_SEGMENT_ALIGN,0);
		frag_wane(frag_now);
		frag_now->fr_fix = 0;
		know( frag_now->fr_next == NULL );
	}

	/* Now build one big frag chain for each segment, linked through
	   fr_next. */
	for (i = SEG_E0; i < SEG_UNKNOWN; i++)
	    {

		    fragS ** prev_frag_ptr_ptr ;
		    struct frchain *next_frchain_ptr;

		    /*	struct frag **head_ptr = segment_info[i].frag_root;*/

		    segment_info[i].frag_root =  segment_info[i].frchainP->frch_root;
#if 0
		    /* Im not sure what this is for */
		    for (frchain_ptr = segment_info[i].frchainP->frch_root;
			 frchain_ptr != (struct frchain *)NULL;
			 frchain_ptr = frchain_ptr->frch_next)
			{
				*head_ptr = frchain_ptr;
				head_ptr = &frchain_ptr->next;
			}


#endif
	    }

	for (i = SEG_E0; i < SEG_UNKNOWN; i++) {
		relax_segment(segment_info[i].frag_root, i);
	}

	/* Now the addresses of the frags are correct within the segment */

	bfd_as_write_hook();
	bfd_close(abfd);
}

#endif

H_SET_TEXT_SIZE(a,b) { abort(); }
H_GET_TEXT_SIZE()  { abort(); }
H_SET_BSS_SIZE() { abort(); }
H_SET_STRING_SIZE() { abort(); }
H_SET_RELOCATION_SIZE() { abort(); }
H_SET_MAGIC_NUMBER() { abort(); }
H_GET_FILE_SIZE() { abort(); }
H_GET_TEXT_RELOCATION_SIZE() { abort(); }

/* end of obj-ieee.c */
