/* BFD back-end for AMD 29000 COFF binaries.
   Copyright 1990, 1991, 1992, 1993, 1994 Free Software Foundation, Inc.
   Contributed by David Wood at New York University 7/8/91.

This file is part of BFD, the Binary File Descriptor library.

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
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.  */

#define A29K 1

#include "bfd.h"
#include "sysdep.h"
#include "libbfd.h"
#include "obstack.h"
#include "coff/a29k.h"
#include "coff/internal.h"
#include "libcoff.h"

static long get_symbol_value PARAMS ((asymbol *));
static bfd_reloc_status_type a29k_reloc
  PARAMS ((bfd *, arelent *, asymbol *, PTR, asection *, bfd *, char **));
static boolean coff_a29k_relocate_section
  PARAMS ((bfd *, struct bfd_link_info *, bfd *, asection *, bfd_byte *,
	   struct internal_reloc *, struct internal_syment *, asection **));
static boolean coff_a29k_adjust_symndx
  PARAMS ((bfd *, struct bfd_link_info *, bfd *, asection *,
	   struct internal_reloc *, boolean *));

#define COFF_DEFAULT_SECTION_ALIGNMENT_POWER (2)

#define INSERT_HWORD(WORD,HWORD)	\
    (((WORD) & 0xff00ff00) | (((HWORD) & 0xff00) << 8) | ((HWORD)& 0xff))
#define EXTRACT_HWORD(WORD) \
    ((((WORD) & 0x00ff0000) >> 8) | ((WORD)& 0xff))
#define SIGN_EXTEND_HWORD(HWORD) \
    ((HWORD) & 0x8000 ? (HWORD)|(~0xffffL) : (HWORD))

/* Provided the symbol, returns the value reffed */
static long
get_symbol_value (symbol)       
     asymbol *symbol;
{                                             
  long relocation = 0;

  if (bfd_is_com_section (symbol->section))
  {
    relocation = 0;                           
  }
  else 
  {                                      
    relocation = symbol->value +
     symbol->section->output_section->vma +
      symbol->section->output_offset;
  }                                           

  return(relocation);
}

/* this function is in charge of performing all the 29k relocations */

static bfd_reloc_status_type
a29k_reloc (abfd, reloc_entry, symbol_in, data, input_section, output_bfd,
	    error_message)
     bfd *abfd;
     arelent *reloc_entry;
     asymbol *symbol_in;
     PTR data;
     asection *input_section;
     bfd *output_bfd;
     char **error_message;
{
  /* the consth relocation comes in two parts, we have to remember
     the state between calls, in these variables */
  static boolean part1_consth_active = false;
  static unsigned long part1_consth_value;

  unsigned long insn;
  unsigned long sym_value;
  unsigned long unsigned_value;
  unsigned short r_type;
  long signed_value;

  unsigned long addr = reloc_entry->address ; /*+ input_section->vma*/
  bfd_byte  *hit_data =addr + (bfd_byte *)(data);
	
  r_type = reloc_entry->howto->type;

  if (output_bfd) {
    /* Partial linking - do nothing */
    reloc_entry->address += input_section->output_offset;
    return bfd_reloc_ok;

  }

  if (symbol_in != NULL
      && bfd_is_und_section (symbol_in->section))
  {
    /* Keep the state machine happy in case we're called again */
    if (r_type == R_IHIHALF) 
    {
      part1_consth_active = true;
      part1_consth_value  = 0;
    }
    return(bfd_reloc_undefined);
  }

  if ((part1_consth_active) && (r_type != R_IHCONST)) 
  {
    part1_consth_active = false;
    *error_message = (char *) "Missing IHCONST";
    return(bfd_reloc_dangerous);
  }


  sym_value = get_symbol_value(symbol_in);

  switch (r_type) 
  {
   case R_IREL: 	
    insn = bfd_get_32(abfd, hit_data); 
    /* Take the value in the field and sign extend it */
    signed_value = EXTRACT_HWORD(insn);
    signed_value = SIGN_EXTEND_HWORD(signed_value);
    signed_value <<= 2;

    /* See the note on the R_IREL reloc in coff_a29k_relocate_section.  */
    if (signed_value == - (long) reloc_entry->address)
      signed_value = 0;

    signed_value += sym_value + reloc_entry->addend;
    if ((signed_value & ~0x3ffff) == 0)
    {				/* Absolute jmp/call */
      insn |= (1<<24);		/* Make it absolute */
      /* FIXME: Should we change r_type to R_IABS */
    } 
    else 
    {
      /* Relative jmp/call, so subtract from the value the
	 address of the place we're coming from */
      signed_value -= (reloc_entry->address
		       + input_section->output_section->vma
		       + input_section->output_offset);
      if (signed_value>0x1ffff || signed_value<-0x20000) 
       return(bfd_reloc_overflow);
    }
    signed_value >>= 2;
    insn = INSERT_HWORD(insn, signed_value);
    bfd_put_32(abfd, insn ,hit_data); 
    break;
   case R_ILOHALF: 
    insn = bfd_get_32(abfd, hit_data); 
    unsigned_value = EXTRACT_HWORD(insn);
    unsigned_value +=  sym_value + reloc_entry->addend;
    insn = INSERT_HWORD(insn, unsigned_value);
    bfd_put_32(abfd, insn, hit_data); 
    break;
   case R_IHIHALF:
    insn = bfd_get_32(abfd, hit_data); 
    /* consth, part 1 
       Just get the symbol value that is referenced */
    part1_consth_active = true;
    part1_consth_value = sym_value + reloc_entry->addend;
    /* Don't modify insn until R_IHCONST */
    break;
   case R_IHCONST:	
    insn = bfd_get_32(abfd, hit_data); 
    /* consth, part 2 
       Now relocate the reference */
    if (part1_consth_active == false) {
      *error_message = (char *) "Missing IHIHALF";
      return(bfd_reloc_dangerous);
    }
    /* sym_ptr_ptr = r_symndx, in coff_slurp_reloc_table() */
    unsigned_value = 0;		/*EXTRACT_HWORD(insn) << 16;*/
    unsigned_value += reloc_entry->addend; /* r_symndx */
    unsigned_value += part1_consth_value;
    unsigned_value = unsigned_value >> 16;
    insn = INSERT_HWORD(insn, unsigned_value);
    part1_consth_active = false;
    bfd_put_32(abfd, insn, hit_data); 
    break;
   case R_BYTE:
    insn = bfd_get_8(abfd, hit_data); 
    unsigned_value = insn + sym_value + reloc_entry->addend;	
    if (unsigned_value & 0xffffff00)
      return(bfd_reloc_overflow);
    bfd_put_8(abfd, unsigned_value, hit_data); 
    break;
   case R_HWORD:
    insn = bfd_get_16(abfd, hit_data); 
    unsigned_value = insn + sym_value + reloc_entry->addend;	
    if (unsigned_value & 0xffff0000)
      return(bfd_reloc_overflow);
    bfd_put_16(abfd, insn, hit_data); 
    break;
   case R_WORD:
    insn = bfd_get_32(abfd, hit_data); 
    insn += sym_value + reloc_entry->addend;  
    bfd_put_32(abfd, insn, hit_data);
    break;
   default:
    *error_message = "Unrecognized reloc";
    return (bfd_reloc_dangerous);
  }


  return(bfd_reloc_ok);	
}

/*      type	   rightshift
		       size
			  bitsize
			       pc-relative
				     bitpos
					 absolute
					     complain_on_overflow
						  special_function
						    relocation name
							       partial_inplace 
								      src_mask
*/

/*FIXME: I'm not real sure about this table */
static reloc_howto_type howto_table[] = 
{
  {R_ABS,     0, 3, 32, false, 0, complain_overflow_bitfield,a29k_reloc,"ABS",     true, 0xffffffff,0xffffffff, false},
  {1},  {2},  {3},   {4},  {5},  {6},  {7},  {8},  {9}, {10},
  {11}, {12}, {13}, {14}, {15}, {16}, {17}, {18}, {19}, {20},
  {21}, {22}, {23},
  {R_IREL,    0, 3, 32, true,  0, complain_overflow_signed,a29k_reloc,"IREL",    true, 0xffffffff,0xffffffff, false},
  {R_IABS,    0, 3, 32, false, 0, complain_overflow_bitfield, a29k_reloc,"IABS",    true, 0xffffffff,0xffffffff, false},
  {R_ILOHALF, 0, 3, 16, true,  0, complain_overflow_signed, a29k_reloc,"ILOHALF", true, 0x0000ffff,0x0000ffff, false},
  {R_IHIHALF, 0, 3, 16, true,  16, complain_overflow_signed, a29k_reloc,"IHIHALF", true, 0xffff0000,0xffff0000, false},
  {R_IHCONST, 0, 3, 16, true,  0, complain_overflow_signed, a29k_reloc,"IHCONST", true, 0xffff0000,0xffff0000, false},
  {R_BYTE,    0, 0, 8, false, 0, complain_overflow_bitfield, a29k_reloc,"BYTE",    true, 0x000000ff,0x000000ff, false},
  {R_HWORD,   0, 1, 16, false, 0, complain_overflow_bitfield, a29k_reloc,"HWORD",   true, 0x0000ffff,0x0000ffff, false},
  {R_WORD,    0, 2, 32, false, 0, complain_overflow_bitfield, a29k_reloc,"WORD",    true, 0xffffffff,0xffffffff, false},
};

#define BADMAG(x) A29KBADMAG(x)

#define RELOC_PROCESSING(relent, reloc, symbols, abfd, section) \
 reloc_processing(relent, reloc, symbols, abfd, section)

static void
reloc_processing (relent,reloc, symbols, abfd, section)
     arelent *relent;
     struct internal_reloc *reloc;
     asymbol **symbols;
     bfd *abfd;
     asection *section;
{
    static bfd_vma ihihalf_vaddr = (bfd_vma) -1;

    relent->address = reloc->r_vaddr;		
    relent->howto = howto_table + reloc->r_type;
    if (reloc->r_type == R_IHCONST) 
    {		
      /* The address of an R_IHCONST should always be the address of
	 the immediately preceding R_IHIHALF.  relocs generated by gas
	 are correct, but relocs generated by High C are different (I
	 can't figure out what the address means for High C).  We can
	 handle both gas and High C by ignoring the address here, and
	 simply reusing the address saved for R_IHIHALF.  */
        if (ihihalf_vaddr == (bfd_vma) -1)
	  abort ();
	relent->address = ihihalf_vaddr;
	ihihalf_vaddr = (bfd_vma) -1;
	relent->addend = reloc->r_symndx;		
	relent->sym_ptr_ptr= bfd_abs_section_ptr->symbol_ptr_ptr;
    }
    else 
    {
      asymbol *ptr;
      relent->sym_ptr_ptr = symbols + obj_convert(abfd)[reloc->r_symndx];

      ptr = *(relent->sym_ptr_ptr);

      if (ptr 
	  && bfd_asymbol_bfd(ptr) == abfd		

	  && ((ptr->flags & BSF_OLD_COMMON)== 0))	
      {						
	  relent->addend = 0;
      }						
      else
      {					
	  relent->addend = 0;			
      }			
      relent->address-= section->vma;
      if (reloc->r_type == R_IHIHALF)
	ihihalf_vaddr = relent->address;
      else if (ihihalf_vaddr != (bfd_vma) -1)
	abort ();
  }
}

/* The reloc processing routine for the optimized COFF linker.  */

static boolean
coff_a29k_relocate_section (output_bfd, info, input_bfd, input_section,
			    contents, relocs, syms, sections)
     bfd *output_bfd;
     struct bfd_link_info *info;
     bfd *input_bfd;
     asection *input_section;
     bfd_byte *contents;
     struct internal_reloc *relocs;
     struct internal_syment *syms;
     asection **sections;
{
  struct internal_reloc *rel;
  struct internal_reloc *relend;
  boolean hihalf;
  bfd_vma hihalf_val;

  /* If we are performing a relocateable link, we don't need to do a
     thing.  The caller will take care of adjusting the reloc
     addresses and symbol indices.  */
  if (info->relocateable)
    return true;

  hihalf = false;
  hihalf_val = 0;

  rel = relocs;
  relend = rel + input_section->reloc_count;
  for (; rel < relend; rel++)
    {
      long symndx;
      bfd_byte *loc;
      struct coff_link_hash_entry *h;
      struct internal_syment *sym;
      asection *sec;
      bfd_vma val;
      boolean overflow;
      unsigned long insn;
      long signed_value;
      unsigned long unsigned_value;
      bfd_reloc_status_type rstat;

      symndx = rel->r_symndx;
      loc = contents + rel->r_vaddr - input_section->vma;

      if (symndx == -1)
	h = NULL;
      else
	h = obj_coff_sym_hashes (input_bfd)[symndx];

      sym = NULL;
      sec = NULL;
      val = 0;

      /* An R_IHCONST reloc does not have a symbol.  Instead, the
         symbol index is an addend.  R_IHCONST is always used in
         conjunction with R_IHHALF.  */
      if (rel->r_type != R_IHCONST)
	{
	  if (h == NULL)
	    {
	      if (symndx == -1)
		sec = bfd_abs_section_ptr;
	      else
		{
		  sym = syms + symndx;
		  sec = sections[symndx];
		  val = (sec->output_section->vma
			 + sec->output_offset
			 + sym->n_value
			 - sec->vma);
		}
	    }
	  else
	    {
	      if (h->root.type == bfd_link_hash_defined
		  || h->root.type == bfd_link_hash_defweak)
		{
		  sec = h->root.u.def.section;
		  val = (h->root.u.def.value
			 + sec->output_section->vma
			 + sec->output_offset);
		}
	      else
		{
		  if (! ((*info->callbacks->undefined_symbol)
			 (info, h->root.root.string, input_bfd, input_section,
			  rel->r_vaddr - input_section->vma)))
		    return false;
		}
	    }

	  if (hihalf)
	    {
	      if (! ((*info->callbacks->reloc_dangerous)
		     (info, "missing IHCONST reloc", input_bfd,
		      input_section, rel->r_vaddr - input_section->vma)))
		return false;
	      hihalf = false;
	    }
	}

      overflow = false;

      switch (rel->r_type)
	{
	default:
	  bfd_set_error (bfd_error_bad_value);
	  return false;

	case R_IREL:
	  insn = bfd_get_32 (input_bfd, loc);

	  /* Extract the addend.  */
	  signed_value = EXTRACT_HWORD (insn);
	  signed_value = SIGN_EXTEND_HWORD (signed_value);
	  signed_value <<= 2;

	  /* Unfortunately, there are two different versions of COFF
	     a29k.  In the original AMD version, the value stored in
	     the field for the R_IREL reloc is a simple addend.  In
	     the GNU version, the value is the negative of the address
	     of the reloc within section.  We try to cope here by
	     assuming the AMD version, unless the addend is exactly
	     the negative of the address; in the latter case we assume
	     the GNU version.  This means that something like
	         .text
		 nop
		 jmp i-4
	     will fail, because the addend of -4 will happen to equal
	     the negative of the address within the section.  The
	     compiler will never generate code like this.

	     At some point in the future we may want to take out this
	     check.  */

	  if (signed_value == - (long) (rel->r_vaddr - input_section->vma))
	    signed_value = 0;

	  /* Determine the destination of the jump.  */
	  signed_value += val;

	  if ((signed_value & ~0x3ffff) == 0)
	    {
	      /* We can use an absolute jump.  */
	      insn |= (1 << 24);
	    }
	  else
	    {
	      /* Make the destination PC relative.  */
	      signed_value -= (input_section->output_section->vma
			       + input_section->output_offset
			       + (rel->r_vaddr - input_section->vma));
	      if (signed_value > 0x1ffff || signed_value < - 0x20000)
		{
		  overflow = true;
		  signed_value = 0;
		}
	    }

	  /* Put the adjusted value back into the instruction.  */
	  signed_value >>= 2;
	  insn = INSERT_HWORD (insn, signed_value);

	  bfd_put_32 (input_bfd, (bfd_vma) insn, loc);

	  break;

	case R_ILOHALF:
	  insn = bfd_get_32 (input_bfd, loc);
	  unsigned_value = EXTRACT_HWORD (insn);
	  unsigned_value += val;
	  insn = INSERT_HWORD (insn, unsigned_value);
	  bfd_put_32 (input_bfd, insn, loc);
	  break;

	case R_IHIHALF:
	  /* Save the value for the R_IHCONST reloc.  */
	  hihalf = true;
	  hihalf_val = val;
	  break;

	case R_IHCONST:
	  if (! hihalf)
	    {
	      if (! ((*info->callbacks->reloc_dangerous)
		     (info, "missing IHIHALF reloc", input_bfd,
		      input_section, rel->r_vaddr - input_section->vma)))
		return false;
	      hihalf_val = 0;
	    }

	  insn = bfd_get_32 (input_bfd, loc);
	  unsigned_value = rel->r_symndx + hihalf_val;
	  unsigned_value >>= 16;
	  insn = INSERT_HWORD (insn, unsigned_value);
	  bfd_put_32 (input_bfd, (bfd_vma) insn, loc);

	  hihalf = false;

	  break;

	case R_BYTE:
	case R_HWORD:
	case R_WORD:
	  rstat = _bfd_relocate_contents (howto_table + rel->r_type,
					  input_bfd, val, loc);
	  if (rstat == bfd_reloc_overflow)
	    overflow = true;
	  else if (rstat != bfd_reloc_ok)
	    abort ();
	  break;
	}

      if (overflow)
	{
	  const char *name;
	  char buf[SYMNMLEN + 1];

	  if (symndx == -1)
	    name = "*ABS*";
	  else if (h != NULL)
	    name = h->root.root.string;
	  else if (sym == NULL)
	    name = "*unknown*";
	  else if (sym->_n._n_n._n_zeroes == 0
		   && sym->_n._n_n._n_offset != 0)
	    name = obj_coff_strings (input_bfd) + sym->_n._n_n._n_offset;
	  else
	    {
	      strncpy (buf, sym->_n._n_name, SYMNMLEN);
	      buf[SYMNMLEN] = '\0';
	      name = buf;
	    }

	  if (! ((*info->callbacks->reloc_overflow)
		 (info, name, howto_table[rel->r_type].name, (bfd_vma) 0,
		  input_bfd, input_section,
		  rel->r_vaddr - input_section->vma)))
	    return false;
	}
    }     

  return true;
}

#define coff_relocate_section coff_a29k_relocate_section

/* We don't want to change the symndx of a R_IHCONST reloc, since it
   is actually an addend, not a symbol index at all.  */

/*ARGSUSED*/
static boolean
coff_a29k_adjust_symndx (obfd, info, ibfd, sec, irel, adjustedp)
     bfd *obfd;
     struct bfd_link_info *info;
     bfd *ibfd;
     asection *sec;
     struct internal_reloc *irel;
     boolean *adjustedp;
{
  if (irel->r_type == R_IHCONST)
    *adjustedp = true;
  else
    *adjustedp = false;
  return true;
}

#define coff_adjust_symndx coff_a29k_adjust_symndx

#include "coffcode.h"

const bfd_target a29kcoff_big_vec =
{
  "coff-a29k-big",		/* name */
  bfd_target_coff_flavour,
  BFD_ENDIAN_BIG,		/* data byte order is big */
  BFD_ENDIAN_BIG,		/* header byte order is big */

  (HAS_RELOC | EXEC_P |		/* object flags */
   HAS_LINENO | HAS_DEBUG |
   HAS_SYMS | HAS_LOCALS | WP_TEXT),

  (SEC_HAS_CONTENTS | SEC_ALLOC /* section flags */
   | SEC_LOAD | SEC_RELOC  
   | SEC_READONLY ),
  '_',				/* leading underscore */
  '/',				/* ar_pad_char */
  15,				/* ar_max_namelen */
  /* data */
  bfd_getb64, bfd_getb_signed_64, bfd_putb64,
     bfd_getb32, bfd_getb_signed_32,   bfd_putb32,
     bfd_getb16, bfd_getb_signed_16, bfd_putb16,
  /* hdrs */
  bfd_getb64, bfd_getb_signed_64, bfd_putb64,
     bfd_getb32, bfd_getb_signed_32,   bfd_putb32,
     bfd_getb16, bfd_getb_signed_16, bfd_putb16,

 {
	    
   _bfd_dummy_target,
   coff_object_p,
   bfd_generic_archive_p,
   _bfd_dummy_target
  },
 {
   bfd_false,
   coff_mkobject,
   _bfd_generic_mkarchive,
   bfd_false
  },
 {
   bfd_false,
   coff_write_object_contents,
   _bfd_write_archive_contents,
   bfd_false
  },

     BFD_JUMP_TABLE_GENERIC (coff),
     BFD_JUMP_TABLE_COPY (coff),
     BFD_JUMP_TABLE_CORE (_bfd_nocore),
     BFD_JUMP_TABLE_ARCHIVE (_bfd_archive_coff),
     BFD_JUMP_TABLE_SYMBOLS (coff),
     BFD_JUMP_TABLE_RELOCS (coff),
     BFD_JUMP_TABLE_WRITE (coff),
     BFD_JUMP_TABLE_LINK (coff),
     BFD_JUMP_TABLE_DYNAMIC (_bfd_nodynamic),

  COFF_SWAP_TABLE
 };
