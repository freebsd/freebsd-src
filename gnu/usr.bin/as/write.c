/* write.c - emit .o file - Copyright(C)1986 Free Software Foundation, Inc.
   Copyright (C) 1986,1987 Free Software Foundation, Inc.

This file is part of GAS, the GNU Assembler.

GAS is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 1, or (at your option)
any later version.

GAS is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with GAS; see the file COPYING.  If not, write to
the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.  */

/* 

   Umm, with real good luck, this thing should be set up to do byteordering
   correctly, but I may have managed to miss a place or two.  Treat a.out
   very carefully until you're SURE that it works. . .

   In order to cross-assemble the target machine must have an a.out header
   similar to the one in a.out.h on THIS machine.  Byteorder doesn't matter;
   we take special care of it, but the numbers must be the same SIZE (# of
   bytes) and in the same PLACE.  If this is not true, you will have some
   trouble.
 */

#include "as.h"
#include "md.h"
#include "subsegs.h"
#include "obstack.h"
#include "struc-symbol.h"
#include "write.h"
#include "symbols.h"

#ifdef SPARC
#include "sparc.h"
#endif
#ifdef I860
#include "i860.h"
#endif

void	append();

#ifdef hpux
#define EXEC_MACHINE_TYPE HP9000S200_ID
#endif

#ifdef DOT_LABEL_PREFIX
#define LOCAL_LABEL(name) (name[0] =='.' \
                         && ( name [1] == 'L' || name [1] == '.' ))
#else  /* not defined DOT_LABEL_PREFIX */
#define LOCAL_LABEL(name) (name [0] == 'L' )
#endif /* not defined DOT_LABEL_PREFIX */

/*
 * In: length of relocation (or of address) in chars: 1, 2 or 4.
 * Out: GNU LD relocation length code: 0, 1, or 2.
 */

static unsigned char

nbytes_r_length [] = {
  42, 0, 1, 42, 2
  };


static struct frag *	text_frag_root;
static struct frag *	data_frag_root;

static struct frag *	text_last_frag;	/* Last frag in segment. */
static struct frag *	data_last_frag;	/* Last frag in segment. */

static struct exec	the_exec;

static long int string_byte_count;

static char *		the_object_file;

#if !defined(SPARC) && !defined(I860)
static
#endif
char *		next_object_file_charP;	/* Tracks object file bytes. */

static long int		size_of_the_object_file; /* # bytes in object file. */

/* static long int		length; JF unused */	/* String length, including trailing '\0'. */

static void	relax_segment();
void		emit_segment();
static relax_addressT	relax_align();
static long int	fixup_segment();
#if !defined(SPARC) && !defined(I860)
static void		emit_relocations();
#endif
/*
 *			fix_new()
 *
 * Create a fixS in obstack 'notes'.
 */
void
#if defined(SPARC) || defined(I860)
fix_new (frag, where, size, add_symbol, sub_symbol, offset, pcrel, r_type)
#else
fix_new (frag, where, size, add_symbol, sub_symbol, offset, pcrel)
#endif
     fragS *	frag;		/* Which frag? */
     int	where;		/* Where in that frag? */
     short int	size;		/* 1, 2  or 4 usually. */
     symbolS *	add_symbol;	/* X_add_symbol. */
     symbolS *	sub_symbol;	/* X_subtract_symbol. */
     long int	offset;		/* X_add_number. */
     int	pcrel;		/* TRUE if PC-relative relocation. */
#if defined(SPARC) || defined(I860)
    int		r_type;
#endif
{
  register fixS *	fixP;

  fixP = (fixS *)obstack_alloc(&notes,sizeof(fixS));

  fixP -> fx_frag	= frag;
  fixP -> fx_where	= where;
  fixP -> fx_size	= size;
  fixP -> fx_addsy	= add_symbol;
  fixP -> fx_subsy	= sub_symbol;
  fixP -> fx_offset	= offset;
  fixP -> fx_pcrel	= pcrel;
  fixP -> fx_next	= * seg_fix_rootP;

  /* JF these 'cuz of the NS32K stuff */
  fixP -> fx_im_disp	= 0;
  fixP -> fx_pcrel_adjust = 0;
  fixP -> fx_bsr	= 0;
  fixP ->fx_bit_fixP	= 0;

#if defined(SPARC) || defined(I860)
  fixP->fx_r_type = r_type;
#endif

  * seg_fix_rootP = fixP;
}

void
write_object_file()
{
  register struct frchain *	frchainP; /* Track along all frchains. */
  register fragS *		fragP;	/* Track along all frags. */
  register struct frchain *	next_frchainP;
  register fragS * *		prev_fragPP;
  register char *		name;
  register symbolS *		symbolP;
  register symbolS **		symbolPP;
  /* register fixS *		fixP; JF unused */
  unsigned
  	text_siz,
	data_siz,
	syms_siz,
	tr_siz,
	dr_siz;
  void output_file_create();
  void output_file_append();
  void output_file_close();
#ifdef DONTDEF
  void gdb_emit();
  void gdb_end();
#endif
  extern long omagic;		/* JF magic # to write out.  Is different for
				   Suns and Vaxen and other boxes */

#ifdef	VMS
  /*
   *	Under VMS we try to be compatible with VAX-11 "C".  Thus, we
   *	call a routine to check for the definition of the procedure
   *	"_main", and if so -- fix it up so that it can be program
   *	entry point.
   */
  VMS_Check_For_Main();
#endif /* VMS */
  /*
   * After every sub-segment, we fake an ".align ...". This conforms to BSD4.2
   * brane-damage. We then fake ".fill 0" because that is the kind of frag
   * that requires least thought. ".align" frags like to have a following
   * frag since that makes calculating their intended length trivial.
   */
#define SUB_SEGMENT_ALIGN (2)
  for ( frchainP=frchain_root; frchainP; frchainP=frchainP->frch_next )
    {
#ifdef	VMS
      /*
       *	Under VAX/VMS, the linker (and PSECT specifications)
       *	take care of correctly aligning the segments.
       *	Doing the alignment here (on initialized data) can
       *	mess up the calculation of global data PSECT sizes.
       */
#undef	SUB_SEGMENT_ALIGN
#define	SUB_SEGMENT_ALIGN ((frchainP->frch_seg != SEG_DATA) ? 2 : 0)
#endif	/* VMS */
      subseg_new (frchainP -> frch_seg, frchainP -> frch_subseg);
      frag_align (SUB_SEGMENT_ALIGN, 0);
				/* frag_align will have left a new frag. */
				/* Use this last frag for an empty ".fill". */
      /*
       * For this segment ...
       * Create a last frag. Do not leave a "being filled in frag".
       */
      frag_wane (frag_now);
      frag_now -> fr_fix	= 0;
      know( frag_now -> fr_next == NULL );
      /* know( frags . obstack_c_base == frags . obstack_c_next_free ); */
      /* Above shows we haven't left a half-completed object on obstack. */
    }

  /*
   * From now on, we don't care about sub-segments.
   * Build one frag chain for each segment. Linked thru fr_next.
   * We know that there is at least 1 text frchain & at least 1 data frchain.
   */
  prev_fragPP = &text_frag_root;
  for ( frchainP=frchain_root; frchainP; frchainP=next_frchainP )
    {
      know( frchainP -> frch_root );
      * prev_fragPP = frchainP -> frch_root;
      prev_fragPP = & frchainP -> frch_last -> fr_next;
      if (   ((next_frchainP = frchainP->frch_next) == NULL)
	  || next_frchainP == data0_frchainP)
	{
	  prev_fragPP = & data_frag_root;
	  if ( next_frchainP )
	    {
	      text_last_frag = frchainP -> frch_last;
	    }
	  else
	    {
	      data_last_frag = frchainP -> frch_last;
	    }
	}
    }				/* for(each struct frchain) */

  /*
   * We have two segments. If user gave -R flag, then we must put the
   * data frags into the text segment. Do this before relaxing so
   * we know to take advantage of -R and make shorter addresses.
   */
  if ( flagseen [ 'R' ] )
    {
      fixS *tmp;

      text_last_frag -> fr_next = data_frag_root;
      text_last_frag = data_last_frag;
      data_last_frag = NULL;
      data_frag_root = NULL;
      if(text_fix_root) {
	for(tmp=text_fix_root;tmp->fx_next;tmp=tmp->fx_next)
	  ;
	tmp->fx_next=data_fix_root;
      } else
        text_fix_root=data_fix_root;
      data_fix_root=NULL;
    }

  relax_segment (text_frag_root, SEG_TEXT);
  relax_segment (data_frag_root, SEG_DATA);
  /*
   * Now the addresses of frags are correct within the segment.
   */

  know(   text_last_frag -> fr_type   == rs_fill && text_last_frag -> fr_offset == 0 );
  text_siz=text_last_frag->fr_address;
#ifdef SPARC
  text_siz= (text_siz+7)&(~7);
  text_last_frag->fr_address=text_siz;
#endif
  md_number_to_chars((char *)&the_exec.a_text,text_siz, sizeof(the_exec.a_text));
  /* the_exec . a_text = text_last_frag -> fr_address; */

  /*
   * Join the 2 segments into 1 huge segment.
   * To do this, re-compute every rn_address in the SEG_DATA frags.
   * Then join the data frags after the text frags.
   *
   * Determine a_data [length of data segment].
   */
  if (data_frag_root)
    {
      register relax_addressT	slide;

      know(   text_last_frag -> fr_type   == rs_fill && text_last_frag -> fr_offset == 0 );
      data_siz=data_last_frag->fr_address;
#ifdef SPARC
      data_siz += (8 - (data_siz % 8)) % 8;
      data_last_frag->fr_address = data_siz;
#endif
      md_number_to_chars((char *)&the_exec.a_data,data_siz,sizeof(the_exec.a_data));
      /* the_exec . a_data = data_last_frag -> fr_address; */
      slide = text_siz; /* Address in file of the data segment. */
      for (fragP = data_frag_root;
	   fragP;
	   fragP = fragP -> fr_next)
	{
	  fragP -> fr_address += slide;
	}
      know( text_last_frag );
      text_last_frag -> fr_next = data_frag_root;
    }
  else {
      md_number_to_chars((char *)&the_exec.a_data,0,sizeof(the_exec.a_data));
      data_siz = 0;
  }

  bss_address_frag . fr_address = text_siz + data_siz;
#ifdef SPARC
  local_bss_counter=(local_bss_counter+7)&(~7);
#endif
  md_number_to_chars((char *)&the_exec.a_bss,local_bss_counter,sizeof(the_exec.a_bss));

	      
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
   * from the symbol chain.
   *
   * Count the (length of the nlists of the) (remaining) symbols.
   * Assign a symbol number to each symbol.
   * Count the number of string-table chars we will emit.
   *
   */
  know( zero_address_frag . fr_address == 0 );
  string_byte_count = sizeof( string_byte_count );

  /* JF deal with forward references first. . . */
  for(symbolP=symbol_rootP;symbolP;symbolP=symbolP->sy_next) {
  	if(symbolP->sy_forward) {
		symbolP->sy_value+=symbolP->sy_forward->sy_value+symbolP->sy_forward->sy_frag->fr_address;
		symbolP->sy_forward=0;
	}
  }
  symbolPP = & symbol_rootP;	/* -> last symbol chain link. */
  {
    register long int		symbol_number;

    symbol_number = 0;
    while (symbolP  = * symbolPP)
      {
	name = symbolP -> sy_name;
	if(flagseen['R'] && (symbolP->sy_nlist.n_type&N_DATA)) {
	  symbolP->sy_nlist.n_type&= ~N_DATA;
	  symbolP->sy_nlist.n_type|= N_TEXT;
	}
	/* if(symbolP->sy_forward) {
	  symbolP->sy_value += symbolP->sy_forward->sy_value + symbolP->sy_forward->sy_frag->fr_address;
	} */
	
	symbolP -> sy_value += symbolP -> sy_frag -> fr_address;
		/* JF the 128 bit is a hack so stabs like
		   "LET_STMT:23. . ."  don't go away */
	/* CPH: 128 bit hack is moby loser.  N_SO for file "Lower.c"
	   fell through the cracks.  I think that N_STAB should be
	   used instead of 128. */
		/* JF the \001 bit is to make sure that local labels
		   ( 1: - 9: don't make it into the symtable either */
#ifndef	VMS	/* Under VMS we need to keep local symbols */
	if ( !name || (symbolP->sy_nlist.n_type&N_STAB)
	    || (name[2]!='\001' && (flagseen ['L'] || ! LOCAL_LABEL(name) )))
#endif	/* not VMS */
	  {
	    symbolP -> sy_number = symbol_number ++;
#ifndef	VMS
	    if (name)
	      {			/* Ordinary case. */
		symbolP -> sy_name_offset = string_byte_count;
		string_byte_count += strlen (symbolP  -> sy_name) + 1;
	      }
	    else			/* .Stabd case. */
#endif	/* not VMS */
		symbolP -> sy_name_offset = 0;
	    symbolPP = & (symbolP -> sy_next);
	  }
#ifndef	VMS
	else
	    * symbolPP = symbolP -> sy_next;
#endif	/* not VMS */
      }				/* for each symbol */

    syms_siz = sizeof( struct nlist) * symbol_number;
    md_number_to_chars((char *)&the_exec.a_syms,syms_siz,sizeof(the_exec.a_syms));
    /* the_exec . a_syms = sizeof( struct nlist) * symbol_number; */
  }

  /*
   * Addresses of frags now reflect addresses we use in the object file.
   * Symbol values are correct.
   * Scan the frags, converting any ".org"s and ".align"s to ".fill"s.
   * Also converting any machine-dependent frags using md_convert_frag();
   */
  subseg_change( SEG_TEXT, 0);

  for (fragP = text_frag_root;  fragP;  fragP = fragP -> fr_next)
    {
      switch (fragP -> fr_type)
	{
	case rs_align:
	case rs_org:
	  fragP -> fr_type = rs_fill;
	  know( fragP -> fr_var == 1 );
	  know( fragP -> fr_next );
	  fragP -> fr_offset
	    =     fragP -> fr_next -> fr_address
	      -   fragP -> fr_address
		- fragP -> fr_fix;
	  break;

	case rs_fill:
	  break;

	case rs_machine_dependent:
	  md_convert_frag (fragP);
	  /*
	   * After md_convert_frag, we make the frag into a ".space 0".
	   * Md_convert_frag() should set up any fixSs and constants
	   * required.
	   */
	  frag_wane (fragP);
	  break;

#ifndef WORKING_DOT_WORD
	case rs_broken_word:
	  {
	    struct broken_word *lie;
	    extern md_short_jump_size;
	    extern md_long_jump_size;

	    if(fragP->fr_subtype) {
	      fragP->fr_fix+=md_short_jump_size;
	      for(lie=(struct broken_word *)(fragP->fr_symbol);lie && lie->dispfrag==fragP;lie=lie->next_broken_word)
		if(lie->added==1)
		  fragP->fr_fix+=md_long_jump_size;
	    }
	    frag_wane(fragP);
	  }
	  break;
#endif

	default:
	  BAD_CASE( fragP -> fr_type );
	  break;
	}			/* switch (fr_type) */
    }				/* for each frag. */

#ifndef WORKING_DOT_WORD
    {
      struct broken_word *lie;
      struct broken_word **prevP;

      prevP= &broken_words;
      for(lie=broken_words; lie; lie=lie->next_broken_word)
	if(!lie->added) {
#if defined(SPARC) || defined(I860)
	  fix_new(	lie->frag,  lie->word_goes_here - lie->frag->fr_literal,
	  		2,  lie->add,
			lie->sub,  lie->addnum,
			0,  NO_RELOC);
#endif
#ifdef NS32K
	  fix_new_ns32k(lie->frag,
	  		lie->word_goes_here - lie->frag->fr_literal,
			2,
			lie->add,
			lie->sub,
			lie->addnum,
			0, 0, 2, 0, 0);
#endif
#if !defined(SPARC) && !defined(NS32K) && !defined(I860)
	  fix_new(	lie->frag,  lie->word_goes_here - lie->frag->fr_literal,
	  		2,  lie->add,
			lie->sub,  lie->addnum,
			0);
#endif
	  /* md_number_to_chars(lie->word_goes_here,
			       lie->add->sy_value
			       + lie->addnum
			       - (lie->sub->sy_value),
			     2); */
	  *prevP=lie->next_broken_word;
	} else
	  prevP= &(lie->next_broken_word);

      for(lie=broken_words;lie;) {
	struct broken_word *untruth;
	char	*table_ptr;
	long	table_addr;
	long	from_addr,
		to_addr;
	int	n,
		m;

	extern	md_short_jump_size;
	extern	md_long_jump_size;
	void	md_create_short_jump();
	void	md_create_long_jump();



	fragP=lie->dispfrag;

	/* Find out how many broken_words go here */
	n=0;
	for(untruth=lie;untruth && untruth->dispfrag==fragP;untruth=untruth->next_broken_word)
	  if(untruth->added==1)
	    n++;

	table_ptr=lie->dispfrag->fr_opcode;
	table_addr=lie->dispfrag->fr_address+(table_ptr - lie->dispfrag->fr_literal);
	/* Create the jump around the long jumps */
	/* This is a short jump from table_ptr+0 to table_ptr+n*long_jump_size */
	from_addr=table_addr;
	to_addr = table_addr + md_short_jump_size + n * md_long_jump_size;
	md_create_short_jump(table_ptr,from_addr,to_addr,lie->dispfrag,lie->add);
	table_ptr+=md_short_jump_size;
	table_addr+=md_short_jump_size;

	for(m=0;lie && lie->dispfrag==fragP;m++,lie=lie->next_broken_word) {
	  if(lie->added==2)
	    continue;
	  /* Patch the jump table */
	  /* This is the offset from ??? to table_ptr+0 */
	  to_addr =   table_addr
	            - (lie->sub->sy_value);
	  md_number_to_chars(lie->word_goes_here,to_addr,2);
	  for(untruth=lie->next_broken_word;untruth && untruth->dispfrag==fragP;untruth=untruth->next_broken_word) {
	    if(untruth->use_jump==lie)
	      md_number_to_chars(untruth->word_goes_here,to_addr,2);
	  }

	  /* Install the long jump */
	  /* this is a long jump from table_ptr+0 to the final target */
	  from_addr=table_addr;
	  to_addr=lie->add->sy_value+lie->addnum;
	  md_create_long_jump(table_ptr,from_addr,to_addr,lie->dispfrag,lie->add);
	  table_ptr+=md_long_jump_size;
	  table_addr+=md_long_jump_size;
	}
      }
    }
#endif
#ifndef	VMS
  /*
   * Scan every FixS performing fixups. We had to wait until now to do
   * this because md_convert_frag() may have made some fixSs.
   */
  /* the_exec . a_trsize
    = sizeof(struct relocation_info) * fixup_segment (text_fix_root, N_TEXT);
  the_exec . a_drsize
    = sizeof(struct relocation_info) * fixup_segment (data_fix_root, N_DATA); */

  tr_siz=sizeof(struct relocation_info) * fixup_segment (text_fix_root, N_TEXT);
  md_number_to_chars((char *)&the_exec.a_trsize, tr_siz ,sizeof(the_exec.a_trsize));
  dr_siz=sizeof(struct relocation_info) * fixup_segment (data_fix_root, N_DATA);
  md_number_to_chars((char *)&the_exec.a_drsize, dr_siz, sizeof(the_exec.a_drsize));
  md_number_to_chars((char *)&the_exec.a_info,omagic,sizeof(the_exec.a_info));
  md_number_to_chars((char *)&the_exec.a_entry,0,sizeof(the_exec.a_entry));

#ifdef EXEC_MACHINE_TYPE
  md_number_to_chars((char *)&the_exec.a_machtype, EXEC_MACHINE_TYPE, sizeof(the_exec.a_machtype));
#endif
#ifdef EXEC_VERSION
  md_number_to_chars((char *)&the_exec.a_version,EXEC_VERSION,sizeof(the_exec.a_version));
#endif
  
  /* the_exec . a_entry = 0; */

  size_of_the_object_file =
    sizeof( the_exec ) +
      text_siz +
        data_siz +
	  syms_siz +
	    tr_siz +
	      dr_siz +
		string_byte_count;
	
  next_object_file_charP
    = the_object_file
      = xmalloc ( size_of_the_object_file );

  output_file_create (out_file_name);

  append (& next_object_file_charP, (char *)(&the_exec), (unsigned long)sizeof(the_exec));

  /*
   * Emit code.
   */
  for (fragP = text_frag_root;  fragP;  fragP = fragP -> fr_next)
    {
      register long int		count;
      register char *		fill_literal;
      register long int		fill_size;

      know( fragP -> fr_type == rs_fill );
      append (& next_object_file_charP, fragP -> fr_literal, (unsigned long)fragP -> fr_fix);
      fill_literal= fragP -> fr_literal + fragP -> fr_fix;
      fill_size   = fragP -> fr_var;
      know( fragP -> fr_offset >= 0 );
      for (count = fragP -> fr_offset;  count;  count --)
	  append (& next_object_file_charP, fill_literal, (unsigned long)fill_size);
    }				/* for each code frag. */

  /*
   * Emit relocations.
   */
  emit_relocations (text_fix_root, (relax_addressT)0);
  emit_relocations (data_fix_root, text_last_frag -> fr_address);
  /*
   * Emit all symbols left in the symbol chain.
   * Any symbol still undefined is made N_EXT.
   */
  for (   symbolP = symbol_rootP;   symbolP;   symbolP = symbolP -> sy_next   )
    {
      register char *	temp;

      temp = symbolP -> sy_nlist . n_un . n_name;
      /* JF fix the numbers up. Call by value RULES! */
      md_number_to_chars((char *)&(symbolP -> sy_nlist  . n_un . n_strx ),symbolP -> sy_name_offset,sizeof(symbolP -> sy_nlist  . n_un . n_strx ));
      md_number_to_chars((char *)&(symbolP->sy_nlist.n_desc),symbolP->sy_nlist.n_desc,sizeof(symbolP -> sy_nlist  . n_desc));
      md_number_to_chars((char *)&(symbolP->sy_nlist.n_value),symbolP->sy_nlist.n_value,sizeof(symbolP->sy_nlist.n_value));
      /* symbolP -> sy_nlist  . n_un . n_strx = symbolP -> sy_name_offset; JF replaced by md above */
      if (symbolP -> sy_type == N_UNDF)
	  symbolP -> sy_type |= N_EXT; /* Any undefined symbols become N_EXT. */
      append (& next_object_file_charP, (char *)(& symbolP -> sy_nlist),
	      (unsigned long)sizeof(struct nlist));
      symbolP -> sy_nlist . n_un . n_name = temp;
    }				/* for each symbol */

  /*
   * next_object_file_charP -> slot for next object byte.
   * Emit strings.
   * Find strings by crawling along symbol table chain.
   */
/* Gotta do md_ byte-ordering stuff for string_byte_count first - KWK */
  md_number_to_chars((char *)&string_byte_count, string_byte_count, sizeof(string_byte_count));

  append (& next_object_file_charP, (char *)&string_byte_count, (unsigned long)sizeof(string_byte_count));
  for (   symbolP = symbol_rootP;   symbolP;   symbolP = symbolP -> sy_next   )
    {
      if (symbolP -> sy_name)
	{			/* Ordinary case: not .stabd. */
	  append (& next_object_file_charP, symbolP -> sy_name,
		  (unsigned long)(strlen (symbolP -> sy_name) + 1));
	}
    }				/* for each symbol */

  know( next_object_file_charP == the_object_file + size_of_the_object_file );

  output_file_append (the_object_file, size_of_the_object_file, out_file_name);

#ifdef DONTDEF
  if (flagseen['G'])		/* GDB symbol file to be appended? */
    {
      gdb_emit (out_file_name);
      gdb_end ();
    }
#endif

  output_file_close (out_file_name);
#else	/* VMS */
  /*
   *	Now do the VMS-dependent part of writing the object file
   */
  VMS_write_object_file(text_siz, data_siz, text_frag_root, data_frag_root);
#endif	/* VMS */
}				/* write_object_file() */

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
#ifndef	VMS
static
#endif	/* not VMS */
void
relax_segment (segment_frag_root, segment_type)
     struct frag *	segment_frag_root;
     segT		segment_type; /* N_DATA or N_TEXT */
{
  register struct frag *	fragP;
  register relax_addressT	address;
  /* register relax_addressT	old_address; JF unused */
  /* register relax_addressT	new_address; JF unused */

  know( segment_type == SEG_DATA || segment_type == SEG_TEXT );

  /* In case md_estimate_size_before_relax() wants to make fixSs. */
  subseg_change (segment_type, 0);

  /*
   * For each frag in segment: count and store  (a 1st guess of) fr_address.
   */
  address = 0;
  for ( fragP = segment_frag_root;   fragP;   fragP = fragP -> fr_next )
    {
      fragP -> fr_address = address;
      address += fragP -> fr_fix;
      switch (fragP -> fr_type)
	{
	case rs_fill:
	  address += fragP -> fr_offset * fragP -> fr_var;
	  break;

	case rs_align:
	  address += relax_align (address, fragP -> fr_offset);
	  break;

	case rs_org:
	  /*
	   * Assume .org is nugatory. It will grow with 1st relax.
	   */
	  break;

	case rs_machine_dependent:
	  address += md_estimate_size_before_relax
	    (fragP, seg_N_TYPE [(int) segment_type]);
	  break;

#ifndef WORKING_DOT_WORD
		/* Broken words don't concern us yet */
	case rs_broken_word:
		break;
#endif

	default:
	  BAD_CASE( fragP -> fr_type );
	  break;
	}			/* switch(fr_type) */
    }				/* for each frag in the segment */

  /*
   * Do relax().
   */
  {
    register long int	stretch; /* May be any size, 0 or negative. */
				/* Cumulative number of addresses we have */
				/* relaxed this pass. */
				/* We may have relaxed more than one address. */
    register long int stretched;  /* Have we stretched on this pass? */
				  /* This is 'cuz stretch may be zero, when,
				     in fact some piece of code grew, and
				     another shrank.  If a branch instruction
				     doesn't fit anymore, we could be scrod */

    do
      {
	stretch = stretched = 0;
	for (fragP = segment_frag_root;  fragP;  fragP = fragP -> fr_next)
	  {
	    register long int	growth;
	    register long int	was_address;
	    /* register long int	var; */
	    register long int	offset;
	    register symbolS *	symbolP;
	    register long int	target;
	    register long int	after;
	    register long int	aim;

	    was_address = fragP -> fr_address;
	    address = fragP -> fr_address += stretch;
	    symbolP = fragP -> fr_symbol;
	    offset = fragP -> fr_offset;
	    /* var = fragP -> fr_var; */
	    switch (fragP -> fr_type)
	      {
	      case rs_fill:	/* .fill never relaxes. */
		growth = 0;
		break;

#ifndef WORKING_DOT_WORD
		/* JF:  This is RMS's idea.  I do *NOT* want to be blamed
		   for it I do not want to write it.  I do not want to have
		   anything to do with it.  This is not the proper way to
		   implement this misfeature. */
	      case rs_broken_word:
	        {
		struct broken_word *lie;
		struct broken_word *untruth;
		extern int md_short_jump_size;
		extern int md_long_jump_size;

			/* Yes this is ugly (storing the broken_word pointer
			   in the symbol slot).  Still, this whole chunk of
			   code is ugly, and I don't feel like doing anything
			   about it.  Think of it as stubbornness in action */
		growth=0;
	        for(lie=(struct broken_word *)(fragP->fr_symbol);
		    lie && lie->dispfrag==fragP;
		    lie=lie->next_broken_word) {

			if(lie->added)
				continue;
			offset=  lie->add->sy_frag->fr_address+lie->add->sy_value + lie->addnum -
				(lie->sub->sy_frag->fr_address+lie->sub->sy_value);
			if(offset<=-32768 || offset>=32767) {
				if(flagseen['k'])
					as_warn(".word %s-%s+%ld didn't fit",lie->add->sy_name,lie->sub->sy_name,lie->addnum);
				lie->added=1;
				if(fragP->fr_subtype==0) {
					fragP->fr_subtype++;
					growth+=md_short_jump_size;
				}
				for(untruth=lie->next_broken_word;untruth && untruth->dispfrag==lie->dispfrag;untruth=untruth->next_broken_word)
					if(untruth->add->sy_frag==lie->add->sy_frag && untruth->add->sy_value==lie->add->sy_value) {
						untruth->added=2;
						untruth->use_jump=lie;
					}
				growth+=md_long_jump_size;
			}
		    }
		}
		break;
#endif
	      case rs_align:
		growth = relax_align ((relax_addressT)(address + fragP -> fr_fix), offset)
		  - relax_align ((relax_addressT)(was_address +  fragP -> fr_fix), offset);
		break;

	      case rs_org:
		target = offset;
		if (symbolP)
		  {
		    know(   ((symbolP -> sy_type & N_TYPE) == N_ABS) || ((symbolP -> sy_type & N_TYPE) == N_DATA) || ((symbolP -> sy_type & N_TYPE) == N_TEXT));
		    know( symbolP -> sy_frag );
		    know( (symbolP->sy_type&N_TYPE)!=N_ABS || symbolP->sy_frag==&zero_address_frag );
		    target +=
		      symbolP -> sy_value
			+ symbolP -> sy_frag -> fr_address;
		  }
		know( fragP -> fr_next );
		after = fragP -> fr_next -> fr_address;
		growth = ((target - after ) > 0) ? (target - after) : 0;
				/* Growth may be -ve, but variable part */
				/* of frag cannot have < 0 chars. */
				/* That is, we can't .org backwards. */

		growth -= stretch;	/* This is an absolute growth factor */
		break;

	      case rs_machine_dependent:
		{
		  register const relax_typeS *	this_type;
		  register const relax_typeS *	start_type;
		  register relax_substateT	next_state;
		  register relax_substateT	this_state;

		  start_type = this_type
		    = md_relax_table + (this_state = fragP -> fr_subtype);
		target = offset;
		if (symbolP)
		  {
 know(   ((symbolP -> sy_type & N_TYPE) == N_ABS) || ((symbolP -> sy_type &
 N_TYPE) == N_DATA) || ((symbolP -> sy_type & N_TYPE) == N_TEXT));
		    know( symbolP -> sy_frag );
		    know( (symbolP->sy_type&N_TYPE)!=N_ABS || symbolP->sy_frag==&zero_address_frag );
		    target +=
		      symbolP -> sy_value
			+ symbolP -> sy_frag -> fr_address;

			/* If frag has yet to be reached on this pass,
			   assume it will move by STRETCH just as we did.
			   If this is not so, it will be because some frag
			   between grows, and that will force another pass.  */

			   	/* JF was just address */
				/* JF also added is_dnrange hack */
				/* There's gotta be a better/faster/etc way
				   to do this. . . */
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

		    if (symbolP->sy_frag->fr_address >= was_address && is_dnrange(fragP,symbolP->sy_frag))
		      target += stretch;

		  }
		  aim = target - address - fragP -> fr_fix;
		  /* The displacement is affected by the instruction size
		   * for the 32k architecture. I think we ought to be able
		   * to add fragP->fr_pcrel_adjust in all cases (it should be
		   * zero if not used), but just in case it breaks something
		   * else we'll put this inside #ifdef NS32K ... #endif
		   */
#ifdef NS32K
		  aim += fragP->fr_pcrel_adjust;
#endif

		  if (aim < 0)
		    {
		      /* Look backwards. */
		      for (next_state = this_type -> rlx_more;  next_state;  )
			{
			  if (aim >= this_type -> rlx_backward)
			      next_state = 0;
			  else
			    {	/* Grow to next state. */
			      this_type = md_relax_table + (this_state = next_state);
			      next_state = this_type -> rlx_more;
			    }
			}
		    }
		  else
		    {
#ifdef DONTDEF
/* JF these next few lines of code are for the mc68020 which can't handle short
   offsets of zero in branch instructions.  What a kludge! */
 if(aim==0 && this_state==(1<<2+0)) {	/* FOO hard encoded from m.c */
	aim=this_type->rlx_forward+1;	/* Force relaxation into word mode */
 }
#endif
/* JF end of 68020 code */
		      /* Look forwards. */
		      for (next_state = this_type -> rlx_more;  next_state;  )
			{
			  if (aim <= this_type -> rlx_forward)
			      next_state = 0;
			  else
			    {	/* Grow to next state. */
			      this_type = md_relax_table + (this_state = next_state);
			      next_state = this_type -> rlx_more;
			    }
			}
		    }
		  if (growth = this_type -> rlx_length - start_type -> rlx_length)
		      fragP -> fr_subtype = this_state;
		}
		break;

	      default:
		BAD_CASE( fragP -> fr_type );
		break;
	      }
	    if(growth) {
	      stretch += growth;
	      stretched++;
	    }
	  }			/* For each frag in the segment. */
      } while (stretched);	/* Until nothing further to relax. */
  }

  /*
   * We now have valid fr_address'es for each frag.
   */

  /*
   * All fr_address's are correct, relative to their own segment.
   * We have made all the fixS we will ever make.
   */
}				/* relax_segment() */

/*
 * Relax_align. Advance location counter to next address that has 'alignment'
 * lowest order bits all 0s.
 */

static relax_addressT		/* How many addresses does the .align take? */
relax_align (address, alignment)
     register relax_addressT	address; /* Address now. */
     register long int		alignment; /* Alignment (binary). */
{
  relax_addressT	mask;
  relax_addressT	new_address;

  mask = ~ ( (~0) << alignment );
  new_address = (address + mask) & (~ mask);
  return (new_address - address);
}

/*
 *			fixup_segment()
 */
static long int
fixup_segment (fixP, this_segment_type)
     register fixS *	fixP;
     int		this_segment_type; /* N_TYPE bits for segment. */
{
  register long int		seg_reloc_count;
		/* JF these all used to be local to the for loop, but GDB doesn't seem to be able to deal with them there, so I moved them here for a bit. */
      register symbolS *	add_symbolP;
      register symbolS *	sub_symbolP;
      register long int		add_number;
      register int		size;
      register char *		place;
      register long int		where;
      register char		pcrel;
      register fragS *		fragP;
      register int		add_symbol_N_TYPE;


  seg_reloc_count = 0;
  for ( ;  fixP;  fixP = fixP -> fx_next)
    {
      fragP       = fixP  -> fx_frag;
      know( fragP );
      where	  = fixP  -> fx_where;
      place       = fragP -> fr_literal + where;
      size	  = fixP  -> fx_size;
      add_symbolP = fixP  -> fx_addsy;
      sub_symbolP = fixP  -> fx_subsy;
      add_number  = fixP  -> fx_offset;
      pcrel	  = fixP  -> fx_pcrel;
      if(add_symbolP)
	add_symbol_N_TYPE = add_symbolP -> sy_type & N_TYPE;
      if (sub_symbolP)
	{
	  if(!add_symbolP)	/* Its just -sym */
	    {
	      if(sub_symbolP->sy_type!=N_ABS)
	        as_warn("Negative of non-absolute symbol %s", sub_symbolP->sy_name);
	      add_number-=sub_symbolP->sy_value;
	    }
	  else if (   ((sub_symbolP -> sy_type ^ add_symbol_N_TYPE) & N_TYPE) == 0
	      && (   add_symbol_N_TYPE == N_DATA
		  || add_symbol_N_TYPE == N_TEXT
		  || add_symbol_N_TYPE == N_BSS
		  || add_symbol_N_TYPE == N_ABS))
	    {
	      /* Difference of 2 symbols from same segment. */
	      /* Can't make difference of 2 undefineds: 'value' means */
	      /* something different for N_UNDF. */
	      add_number += add_symbolP -> sy_value - sub_symbolP -> sy_value;
	      add_symbolP = NULL;
	      fixP -> fx_addsy = NULL;
	    }
	  else
	    {
	      /* Different segments in subtraction. */
	      know( sub_symbolP -> sy_type != (N_ABS | N_EXT))
		if (sub_symbolP -> sy_type == N_ABS)
		    add_number -= sub_symbolP -> sy_value;
		else
		  {
			   as_warn("Can't emit reloc {- %s-seg symbol \"%s\"} @ file address %d.",
				    seg_name[(int)N_TYPE_seg[sub_symbolP->sy_type&N_TYPE]],
				    sub_symbolP -> sy_name, fragP -> fr_address + where);
		  }
	    }
	}
      if (add_symbolP)
	{
	  if (add_symbol_N_TYPE == this_segment_type && pcrel)
	    {
	      /*
	       * This fixup was made when the symbol's segment was
	       * SEG_UNKNOWN, but it is now in the local segment.
	       * So we know how to do the address without relocation.
	       */
	      add_number += add_symbolP -> sy_value;
	      add_number -=
#ifndef NS32K
		      size +
#endif
		      where + fragP -> fr_address;
#if defined(NS32K) && defined(SEQUENT_COMPATABILITY)
	      if (fragP->fr_bsr)
	        add_number -= 0x12;	/* FOO Kludge alert! */
#endif
		/* Kenny thinks this needs *
		/* add_number +=size-2; */
	      pcrel = 0;	/* Lie. Don't want further pcrel processing. */
	      fixP -> fx_addsy = NULL; /* No relocations please. */
	      /*
	       * It would be nice to check that the address does not overflow.
	       * I didn't do this check because:
	       * +  It is machine dependent in the general case (eg 32032)
	       * +  Compiler output will never need this checking, so why
	       *    slow down the usual case?
	       */
	    }
	  else
	    {
	      switch (add_symbol_N_TYPE)
		{
		case N_ABS:
		  add_number += add_symbolP -> sy_value;
		  fixP -> fx_addsy = NULL;
		  add_symbolP = NULL;
		  break;
		  
		case N_BSS:
		case N_DATA:
		case N_TEXT:
		  seg_reloc_count ++;
		  add_number += add_symbolP -> sy_value;
		  break;
		  
		case N_UNDF:
		  seg_reloc_count ++;
		  break;
		  
		default:
		  BAD_CASE( add_symbol_N_TYPE );
		  break;
		}		/* switch on symbol seg */
	    }			/* if not in local seg */
	}			/* if there was a + symbol */
      if (pcrel)
	{
	  add_number -=
#ifndef NS32K
		  size + 
#endif
		  where + fragP -> fr_address;
	  if (add_symbolP == 0)
	    {
	      fixP -> fx_addsy = & abs_symbol;
	      seg_reloc_count ++;
	    }
	}
      /* OVE added fx_im_disp for ns32k and others */
      if (!fixP->fx_bit_fixP) {
	/* JF I hope this works . . . */
	if((size==1 && (add_number& ~0xFF)   && (add_number&~0xFF!=(-1&~0xFF))) ||
	   (size==2 && (add_number& ~0xFFFF) && (add_number&~0xFFFF!=(-1&~0xFFFF))))
	  as_warn("Fixup of %d too large for field width of %d",add_number, size);

	switch (fixP->fx_im_disp) {
	case 0:
#if defined(SPARC) || defined(I860)
	  fixP->fx_addnumber = add_number;
	  md_number_to_imm(place, add_number, size, fixP, this_segment_type);
#else
	  md_number_to_imm (place, add_number, size);
	  /* OVE: the immediates, like disps, have lsb at lowest address */
#endif
	  break;
	case 1:
	  md_number_to_disp (place,
			     fixP->fx_pcrel ? add_number+fixP->fx_pcrel_adjust:add_number,
			     size);
	  break;
	case 2: /* fix requested for .long .word etc */
	  md_number_to_chars (place, add_number, size);
	  break;
	default:
	  as_fatal("Internal error in write.c in fixup_segment");
	} /* OVE: maybe one ought to put _imm _disp _chars in one md-func */
      } else {
	md_number_to_field (place, add_number, fixP->fx_bit_fixP);
      }
    }				/* For each fixS in this segment. */
  return (seg_reloc_count);
}				/* fixup_segment() */


/* The sparc needs its own emit_relocations() */
#if !defined(SPARC) && !defined(I860)
/*
 *		emit_relocations()
 *
 * Crawl along a fixS chain. Emit the segment's relocations.
 */
static void
emit_relocations (fixP, segment_address_in_file)
     register fixS *	fixP;	/* Fixup chain for this segment. */
     relax_addressT	segment_address_in_file;
{
  struct relocation_info	ri;
  register symbolS *		symbolP;

	/* JF this is for paranoia */
  bzero((char *)&ri,sizeof(ri));
  for ( ;  fixP;  fixP = fixP -> fx_next)
    {
      if (symbolP = fixP -> fx_addsy)
	{

#ifdef NS32K
		/* These two 'cuz of NS32K */
	  ri . r_bsr		= fixP -> fx_bsr;
	  ri . r_disp		= fixP -> fx_im_disp;
#endif

	  ri . r_length		= nbytes_r_length [fixP -> fx_size];
	  ri . r_pcrel		= fixP -> fx_pcrel;
	  ri . r_address	= fixP -> fx_frag -> fr_address
	    +   fixP -> fx_where
	      - segment_address_in_file;
	  if ((symbolP -> sy_type & N_TYPE) == N_UNDF)
	    {
	      ri . r_extern	= 1;
	      ri . r_symbolnum	= symbolP -> sy_number;
	    }
	  else
	    {
	      ri . r_extern	= 0;
	      ri . r_symbolnum	= symbolP -> sy_type & N_TYPE;
	    }

	  /* 
	    The 68k machines assign bit-fields from higher bits to 
	    lower bits ("left-to-right") within the int.  VAXen assign 
	    bit-fields from lower bits to higher bits ("right-to-left").
	    Both handle multi-byte numbers in their usual fashion
	    (Big-endian and little-endian stuff).
	    Thus we need a machine dependent routine to make
	    sure the structure is written out correctly.  FUN!
	   */
	  md_ri_to_chars((char *) &ri, ri); 
	  append (&next_object_file_charP, (char *)& ri, (unsigned long)sizeof(ri));
	}
    }

}
#endif

int
is_dnrange(f1,f2)
struct frag *f1,*f2;
{
	while(f1) {
		if(f1->fr_next==f2)
			return 1;
		f1=f1->fr_next;
	}
	return 0;
}
/* End: as-write.c */
