/* BFD support for handling relocation entries.
   Copyright (C) 1990, 1991, 1992, 1993 Free Software Foundation, Inc.
   Written by Cygnus Support.

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
Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.  */

/*
SECTION
	Relocations

	BFD maintains relocations in much the same way it maintains
	symbols: they are left alone until required, then read in
	en-mass and translated into an internal form.  A common
	routine <<bfd_perform_relocation>> acts upon the
	canonical form to do the fixup.

	Relocations are maintained on a per section basis,
	while symbols are maintained on a per BFD basis.

	All that a back end has to do to fit the BFD interface is to create
	a <<struct reloc_cache_entry>> for each relocation
	in a particular section, and fill in the right bits of the structures.

@menu
@* typedef arelent::
@* howto manager::
@end menu

*/
#include "bfd.h"
#include "sysdep.h"
#include "bfdlink.h"
#include "libbfd.h"
/*
DOCDD
INODE
	typedef arelent, howto manager, Relocations, Relocations

SUBSECTION
	typedef arelent

	This is the structure of a relocation entry:

CODE_FRAGMENT
.
.typedef enum bfd_reloc_status
.{
.       {* No errors detected *}
.  bfd_reloc_ok,
.
.       {* The relocation was performed, but there was an overflow. *}
.  bfd_reloc_overflow,
.
.       {* The address to relocate was not within the section supplied. *}
.  bfd_reloc_outofrange,
.
.       {* Used by special functions *}
.  bfd_reloc_continue,
.
.       {* Unsupported relocation size requested. *}
.  bfd_reloc_notsupported,
.
.       {* Unused *}
.  bfd_reloc_other,
.
.       {* The symbol to relocate against was undefined. *}
.  bfd_reloc_undefined,
.
.       {* The relocation was performed, but may not be ok - presently
.          generated only when linking i960 coff files with i960 b.out
.          symbols.  If this type is returned, the error_message argument
.          to bfd_perform_relocation will be set.  *}
.  bfd_reloc_dangerous
. }
. bfd_reloc_status_type;
.
.
.typedef struct reloc_cache_entry
.{
.       {* A pointer into the canonical table of pointers  *}
.  struct symbol_cache_entry **sym_ptr_ptr;
.
.       {* offset in section *}
.  bfd_size_type address;
.
.       {* addend for relocation value *}
.  bfd_vma addend;
.
.       {* Pointer to how to perform the required relocation *}
.  const struct reloc_howto_struct *howto;
.
.} arelent;

*/

/*
DESCRIPTION

        Here is a description of each of the fields within an <<arelent>>:

        o <<sym_ptr_ptr>>

        The symbol table pointer points to a pointer to the symbol
        associated with the relocation request.  It is
        the pointer into the table returned by the back end's
        <<get_symtab>> action. @xref{Symbols}. The symbol is referenced
        through a pointer to a pointer so that tools like the linker
        can fix up all the symbols of the same name by modifying only
        one pointer. The relocation routine looks in the symbol and
        uses the base of the section the symbol is attached to and the
        value of the symbol as the initial relocation offset. If the
        symbol pointer is zero, then the section provided is looked up.

        o <<address>>

        The <<address>> field gives the offset in bytes from the base of
        the section data which owns the relocation record to the first
        byte of relocatable information. The actual data relocated
        will be relative to this point; for example, a relocation
        type which modifies the bottom two bytes of a four byte word
        would not touch the first byte pointed to in a big endian
        world.
	
	o <<addend>>

	The <<addend>> is a value provided by the back end to be added (!)
	to the relocation offset. Its interpretation is dependent upon
	the howto. For example, on the 68k the code:


|        char foo[];
|        main()
|                {
|                return foo[0x12345678];
|                }

        Could be compiled into:

|        linkw fp,#-4
|        moveb @@#12345678,d0
|        extbl d0
|        unlk fp
|        rts


        This could create a reloc pointing to <<foo>>, but leave the
        offset in the data, something like:


|RELOCATION RECORDS FOR [.text]:
|offset   type      value
|00000006 32        _foo
|
|00000000 4e56 fffc          ; linkw fp,#-4
|00000004 1039 1234 5678     ; moveb @@#12345678,d0
|0000000a 49c0               ; extbl d0
|0000000c 4e5e               ; unlk fp
|0000000e 4e75               ; rts


        Using coff and an 88k, some instructions don't have enough
        space in them to represent the full address range, and
        pointers have to be loaded in two parts. So you'd get something like:


|        or.u     r13,r0,hi16(_foo+0x12345678)
|        ld.b     r2,r13,lo16(_foo+0x12345678)
|        jmp      r1


        This should create two relocs, both pointing to <<_foo>>, and with
        0x12340000 in their addend field. The data would consist of:


|RELOCATION RECORDS FOR [.text]:
|offset   type      value
|00000002 HVRT16    _foo+0x12340000
|00000006 LVRT16    _foo+0x12340000
|
|00000000 5da05678           ; or.u r13,r0,0x5678
|00000004 1c4d5678           ; ld.b r2,r13,0x5678
|00000008 f400c001           ; jmp r1


        The relocation routine digs out the value from the data, adds
        it to the addend to get the original offset, and then adds the
        value of <<_foo>>. Note that all 32 bits have to be kept around
        somewhere, to cope with carry from bit 15 to bit 16.

        One further example is the sparc and the a.out format. The
        sparc has a similar problem to the 88k, in that some
        instructions don't have room for an entire offset, but on the
        sparc the parts are created in odd sized lumps. The designers of
        the a.out format chose to not use the data within the section
        for storing part of the offset; all the offset is kept within
        the reloc. Anything in the data should be ignored.

|        save %sp,-112,%sp
|        sethi %hi(_foo+0x12345678),%g2
|        ldsb [%g2+%lo(_foo+0x12345678)],%i0
|        ret
|        restore

        Both relocs contain a pointer to <<foo>>, and the offsets
        contain junk.


|RELOCATION RECORDS FOR [.text]:
|offset   type      value
|00000004 HI22      _foo+0x12345678
|00000008 LO10      _foo+0x12345678
|
|00000000 9de3bf90     ; save %sp,-112,%sp
|00000004 05000000     ; sethi %hi(_foo+0),%g2
|00000008 f048a000     ; ldsb [%g2+%lo(_foo+0)],%i0
|0000000c 81c7e008     ; ret
|00000010 81e80000     ; restore


        o <<howto>>

        The <<howto>> field can be imagined as a
        relocation instruction. It is a pointer to a structure which
        contains information on what to do with all of the other
        information in the reloc record and data section. A back end
        would normally have a relocation instruction set and turn
        relocations into pointers to the correct structure on input -
        but it would be possible to create each howto field on demand.

*/

/*
SUBSUBSECTION
	<<enum complain_overflow>>

	Indicates what sort of overflow checking should be done when
	performing a relocation.

CODE_FRAGMENT
.
.enum complain_overflow
.{
.	{* Do not complain on overflow. *}
.  complain_overflow_dont,
.
.	{* Complain if the bitfield overflows, whether it is considered
.	   as signed or unsigned. *}
.  complain_overflow_bitfield,
.
.	{* Complain if the value overflows when considered as signed
.	   number. *}
.  complain_overflow_signed,
.
.	{* Complain if the value overflows when considered as an
.	   unsigned number. *}
.  complain_overflow_unsigned
.};

*/

/*
SUBSUBSECTION
        <<reloc_howto_type>>

        The <<reloc_howto_type>> is a structure which contains all the
        information that libbfd needs to know to tie up a back end's data.

CODE_FRAGMENT
.struct symbol_cache_entry;		{* Forward declaration *}
.
.typedef unsigned char bfd_byte;
.typedef struct reloc_howto_struct reloc_howto_type;
.
.struct reloc_howto_struct
.{
.       {*  The type field has mainly a documetary use - the back end can
.           do what it wants with it, though normally the back end's
.           external idea of what a reloc number is stored
.           in this field. For example, a PC relative word relocation
.           in a coff environment has the type 023 - because that's
.           what the outside world calls a R_PCRWORD reloc. *}
.  unsigned int type;
.
.       {*  The value the final relocation is shifted right by. This drops
.           unwanted data from the relocation.  *}
.  unsigned int rightshift;
.
.	{*  The size of the item to be relocated.  This is *not* a
.	    power-of-two measure.  To get the number of bytes operated
.	    on by a type of relocation, use bfd_get_reloc_size.  *}
.  int size;
.
.       {*  The number of bits in the item to be relocated.  This is used
.	    when doing overflow checking.  *}
.  unsigned int bitsize;
.
.       {*  Notes that the relocation is relative to the location in the
.           data section of the addend. The relocation function will
.           subtract from the relocation value the address of the location
.           being relocated. *}
.  boolean pc_relative;
.
.	{*  The bit position of the reloc value in the destination.
.	    The relocated value is left shifted by this amount. *}
.  unsigned int bitpos;
.
.	{* What type of overflow error should be checked for when
.	   relocating. *}
.  enum complain_overflow complain_on_overflow;
.
.       {* If this field is non null, then the supplied function is
.          called rather than the normal function. This allows really
.          strange relocation methods to be accomodated (e.g., i960 callj
.          instructions). *}
.  bfd_reloc_status_type (*special_function)
.				    PARAMS ((bfd *abfd,
.					     arelent *reloc_entry,
.                                            struct symbol_cache_entry *symbol,
.                                            PTR data,
.                                            asection *input_section,
.                                            bfd *output_bfd,
.                                            char **error_message));
.
.       {* The textual name of the relocation type. *}
.  char *name;
.
.       {* When performing a partial link, some formats must modify the
.          relocations rather than the data - this flag signals this.*}
.  boolean partial_inplace;
.
.       {* The src_mask selects which parts of the read in data
.          are to be used in the relocation sum.  E.g., if this was an 8 bit
.          bit of data which we read and relocated, this would be
.          0x000000ff. When we have relocs which have an addend, such as
.          sun4 extended relocs, the value in the offset part of a
.          relocating field is garbage so we never use it. In this case
.          the mask would be 0x00000000. *}
.  bfd_vma src_mask;
.
.       {* The dst_mask selects which parts of the instruction are replaced
.          into the instruction. In most cases src_mask == dst_mask,
.          except in the above special case, where dst_mask would be
.          0x000000ff, and src_mask would be 0x00000000.   *}
.  bfd_vma dst_mask;
.
.       {* When some formats create PC relative instructions, they leave
.          the value of the pc of the place being relocated in the offset
.          slot of the instruction, so that a PC relative relocation can
.          be made just by adding in an ordinary offset (e.g., sun3 a.out).
.          Some formats leave the displacement part of an instruction
.          empty (e.g., m88k bcs); this flag signals the fact.*}
.  boolean pcrel_offset;
.
.};

*/

/*
FUNCTION
	The HOWTO Macro

DESCRIPTION
	The HOWTO define is horrible and will go away.


.#define HOWTO(C, R,S,B, P, BI, O, SF, NAME, INPLACE, MASKSRC, MASKDST, PC) \
.  {(unsigned)C,R,S,B, P, BI, O,SF,NAME,INPLACE,MASKSRC,MASKDST,PC}

DESCRIPTION
	And will be replaced with the totally magic way. But for the
	moment, we are compatible, so do it this way.


.#define NEWHOWTO( FUNCTION, NAME,SIZE,REL,IN) HOWTO(0,0,SIZE,0,REL,0,complain_overflow_dont,FUNCTION, NAME,false,0,0,IN)
.
DESCRIPTION
	Helper routine to turn a symbol into a relocation value.

.#define HOWTO_PREPARE(relocation, symbol)      \
.  {                                            \
.  if (symbol != (asymbol *)NULL) {             \
.    if (bfd_is_com_section (symbol->section)) { \
.      relocation = 0;                          \
.    }                                          \
.    else {                                     \
.      relocation = symbol->value;              \
.    }                                          \
.  }                                            \
.}

*/

/*
FUNCTION
	bfd_get_reloc_size

SYNOPSIS
	int bfd_get_reloc_size (const reloc_howto_type *);

DESCRIPTION
	For a reloc_howto_type that operates on a fixed number of bytes,
	this returns the number of bytes operated on.
 */

int
bfd_get_reloc_size (howto)
     const reloc_howto_type *howto;
{
  switch (howto->size)
    {
    case 0: return 1;
    case 1: return 2;
    case 2: return 4;
    case 3: return 0;
    case 4: return 8;
    case -2: return 4;
    default: abort ();
    }
}

/*
TYPEDEF
	arelent_chain

DESCRIPTION

	How relocs are tied together in an <<asection>>:

.typedef struct relent_chain {
.  arelent relent;
.  struct   relent_chain *next;
.} arelent_chain;

*/



/*
FUNCTION
	bfd_perform_relocation

SYNOPSIS
	bfd_reloc_status_type
                bfd_perform_relocation
                        (bfd *abfd,
                         arelent *reloc_entry,
                         PTR data,
                         asection *input_section,
                         bfd *output_bfd,
			 char **error_message);

DESCRIPTION
	If @var{output_bfd} is supplied to this function, the
	generated image will be relocatable; the relocations are
	copied to the output file after they have been changed to
	reflect the new state of the world. There are two ways of
	reflecting the results of partial linkage in an output file:
	by modifying the output data in place, and by modifying the
	relocation record.  Some native formats (e.g., basic a.out and
	basic coff) have no way of specifying an addend in the
	relocation type, so the addend has to go in the output data.
	This is no big deal since in these formats the output data
	slot will always be big enough for the addend. Complex reloc
	types with addends were invented to solve just this problem.
	The @var{error_message} argument is set to an error message if
	this return @code{bfd_reloc_dangerous}.

*/


bfd_reloc_status_type
bfd_perform_relocation (abfd, reloc_entry, data, input_section, output_bfd,
			error_message)
     bfd *abfd;
     arelent *reloc_entry;
     PTR data;
     asection *input_section;
     bfd *output_bfd;
     char **error_message;
{
  bfd_vma relocation;
  bfd_reloc_status_type flag = bfd_reloc_ok;
  bfd_size_type addr = reloc_entry->address;
  bfd_vma output_base = 0;
  const reloc_howto_type *howto = reloc_entry->howto;
  asection *reloc_target_output_section;
  asymbol *symbol;

  symbol = *(reloc_entry->sym_ptr_ptr);
  if (bfd_is_abs_section (symbol->section)
      && output_bfd != (bfd *) NULL)
    {
      reloc_entry->address += input_section->output_offset;
      return bfd_reloc_ok;
    }

  /* If we are not producing relocateable output, return an error if
     the symbol is not defined.  An undefined weak symbol is
     considered to have a value of zero (SVR4 ABI, p. 4-27).  */
  if (bfd_is_und_section (symbol->section)
      && (symbol->flags & BSF_WEAK) == 0
      && output_bfd == (bfd *) NULL)
    flag = bfd_reloc_undefined;

  /* If there is a function supplied to handle this relocation type,
     call it.  It'll return `bfd_reloc_continue' if further processing
     can be done.  */
  if (howto->special_function)
    {
      bfd_reloc_status_type cont;
      cont = howto->special_function (abfd, reloc_entry, symbol, data,
				      input_section, output_bfd,
				      error_message);
      if (cont != bfd_reloc_continue)
	return cont;
    }

  /* Is the address of the relocation really within the section?  */
  if (reloc_entry->address > input_section->_cooked_size)
    return bfd_reloc_outofrange;

  /* Work out which section the relocation is targetted at and the
     initial relocation command value.  */

  /* Get symbol value.  (Common symbols are special.)  */
  if (bfd_is_com_section (symbol->section))
    relocation = 0;
  else
    relocation = symbol->value;


  reloc_target_output_section = symbol->section->output_section;

  /* Convert input-section-relative symbol value to absolute.  */
  if (output_bfd && howto->partial_inplace == false)
    output_base = 0;
  else
    output_base = reloc_target_output_section->vma;

  relocation += output_base + symbol->section->output_offset;

  /* Add in supplied addend.  */
  relocation += reloc_entry->addend;

  /* Here the variable relocation holds the final address of the
     symbol we are relocating against, plus any addend.  */

  if (howto->pc_relative == true)
    {
      /* This is a PC relative relocation.  We want to set RELOCATION
	 to the distance between the address of the symbol and the
	 location.  RELOCATION is already the address of the symbol.

	 We start by subtracting the address of the section containing
	 the location.

	 If pcrel_offset is set, we must further subtract the position
	 of the location within the section.  Some targets arrange for
	 the addend to be the negative of the position of the location
	 within the section; for example, i386-aout does this.  For
	 i386-aout, pcrel_offset is false.  Some other targets do not
	 include the position of the location; for example, m88kbcs,
	 or ELF.  For those targets, pcrel_offset is true.

	 If we are producing relocateable output, then we must ensure
	 that this reloc will be correctly computed when the final
	 relocation is done.  If pcrel_offset is false we want to wind
	 up with the negative of the location within the section,
	 which means we must adjust the existing addend by the change
	 in the location within the section.  If pcrel_offset is true
	 we do not want to adjust the existing addend at all.

	 FIXME: This seems logical to me, but for the case of
	 producing relocateable output it is not what the code
	 actually does.  I don't want to change it, because it seems
	 far too likely that something will break.  */

      relocation -=
	input_section->output_section->vma + input_section->output_offset;

      if (howto->pcrel_offset == true)
	relocation -= reloc_entry->address;
    }

  if (output_bfd != (bfd *) NULL)
    {
      if (howto->partial_inplace == false)
	{
	  /* This is a partial relocation, and we want to apply the relocation
	     to the reloc entry rather than the raw data. Modify the reloc
	     inplace to reflect what we now know.  */
	  reloc_entry->addend = relocation;
	  reloc_entry->address += input_section->output_offset;
	  return flag;
	}
      else
	{
	  /* This is a partial relocation, but inplace, so modify the
	     reloc record a bit.

	     If we've relocated with a symbol with a section, change
	     into a ref to the section belonging to the symbol.  */

	  reloc_entry->address += input_section->output_offset;

	  /* WTF?? */
	  if (abfd->xvec->flavour == bfd_target_coff_flavour
	      && strcmp (abfd->xvec->name, "aixcoff-rs6000") != 0
	      && strcmp (abfd->xvec->name, "coff-Intel-little") != 0
	      && strcmp (abfd->xvec->name, "coff-Intel-big") != 0)
	    {
#if 1
	      /* For m68k-coff, the addend was being subtracted twice during
		 relocation with -r.  Removing the line below this comment
		 fixes that problem; see PR 2953.

However, Ian wrote the following, regarding removing the line below,
which explains why it is still enabled:  --djm

If you put a patch like that into BFD you need to check all the COFF
linkers.  I am fairly certain that patch will break coff-i386 (e.g.,
SCO); see coff_i386_reloc in coff-i386.c where I worked around the
problem in a different way.  There may very well be a reason that the
code works as it does.

Hmmm.  The first obvious point is that bfd_perform_relocation should
not have any tests that depend upon the flavour.  It's seem like
entirely the wrong place for such a thing.  The second obvious point
is that the current code ignores the reloc addend when producing
relocateable output for COFF.  That's peculiar.  In fact, I really
have no idea what the point of the line you want to remove is.

A typical COFF reloc subtracts the old value of the symbol and adds in
the new value to the location in the object file (if it's a pc
relative reloc it adds the difference between the symbol value and the
location).  When relocating we need to preserve that property.

BFD handles this by setting the addend to the negative of the old
value of the symbol.  Unfortunately it handles common symbols in a
non-standard way (it doesn't subtract the old value) but that's a
different story (we can't change it without losing backward
compatibility with old object files) (coff-i386 does subtract the old
value, to be compatible with existing coff-i386 targets, like SCO).

So everything works fine when not producing relocateable output.  When
we are producing relocateable output, logically we should do exactly
what we do when not producing relocateable output.  Therefore, your
patch is correct.  In fact, it should probably always just set
reloc_entry->addend to 0 for all cases, since it is, in fact, going to
add the value into the object file.  This won't hurt the COFF code,
which doesn't use the addend; I'm not sure what it will do to other
formats (the thing to check for would be whether any formats both use
the addend and set partial_inplace).

When I wanted to make coff-i386 produce relocateable output, I ran
into the problem that you are running into: I wanted to remove that
line.  Rather than risk it, I made the coff-i386 relocs use a special
function; it's coff_i386_reloc in coff-i386.c.  The function
specifically adds the addend field into the object file, knowing that
bfd_perform_relocation is not going to.  If you remove that line, then
coff-i386.c will wind up adding the addend field in twice.  It's
trivial to fix; it just needs to be done.

The problem with removing the line is just that it may break some
working code.  With BFD it's hard to be sure of anything.  The right
way to deal with this is simply to build and test at least all the
supported COFF targets.  It should be straightforward if time and disk
space consuming.  For each target:
    1) build the linker
    2) generate some executable, and link it using -r (I would
       probably use paranoia.o and link against newlib/libc.a, which
       for all the supported targets would be available in
       /usr/cygnus/progressive/H-host/target/lib/libc.a).
    3) make the change to reloc.c
    4) rebuild the linker
    5) repeat step 2
    6) if the resulting object files are the same, you have at least
       made it no worse
    7) if they are different you have to figure out which version is
       right
*/
	      relocation -= reloc_entry->addend;
#endif
	      reloc_entry->addend = 0;
	    }
	  else
	    {
	      reloc_entry->addend = relocation;
	    }
	}
    }
  else
    {
      reloc_entry->addend = 0;
    }

  /* FIXME: This overflow checking is incomplete, because the value
     might have overflowed before we get here.  For a correct check we
     need to compute the value in a size larger than bitsize, but we
     can't reasonably do that for a reloc the same size as a host
     machine word.
     FIXME: We should also do overflow checking on the result after
     adding in the value contained in the object file.  */
  if (howto->complain_on_overflow != complain_overflow_dont)
    {
      bfd_vma check;

      /* Get the value that will be used for the relocation, but
	 starting at bit position zero.  */
      if (howto->rightshift > howto->bitpos)
	check = relocation >> (howto->rightshift - howto->bitpos);
      else
	check = relocation << (howto->bitpos - howto->rightshift);
      switch (howto->complain_on_overflow)
	{
	case complain_overflow_signed:
	  {
	    /* Assumes two's complement.  */
	    bfd_signed_vma reloc_signed_max = (1 << (howto->bitsize - 1)) - 1;
	    bfd_signed_vma reloc_signed_min = ~reloc_signed_max;

	    /* The above right shift is incorrect for a signed value.
	       Fix it up by forcing on the upper bits.  */
	    if (howto->rightshift > howto->bitpos
		&& (bfd_signed_vma) relocation < 0)
	      check |= ((bfd_vma) - 1
			& ~((bfd_vma) - 1
			    >> (howto->rightshift - howto->bitpos)));
	    if ((bfd_signed_vma) check > reloc_signed_max
		|| (bfd_signed_vma) check < reloc_signed_min)
	      flag = bfd_reloc_overflow;
	  }
	  break;
	case complain_overflow_unsigned:
	  {
	    /* Assumes two's complement.  This expression avoids
	       overflow if howto->bitsize is the number of bits in
	       bfd_vma.  */
	    bfd_vma reloc_unsigned_max =
	    (((1 << (howto->bitsize - 1)) - 1) << 1) | 1;

	    if ((bfd_vma) check > reloc_unsigned_max)
	      flag = bfd_reloc_overflow;
	  }
	  break;
	case complain_overflow_bitfield:
	  {
	    /* Assumes two's complement.  This expression avoids
	       overflow if howto->bitsize is the number of bits in
	       bfd_vma.  */
	    bfd_vma reloc_bits = (((1 << (howto->bitsize - 1)) - 1) << 1) | 1;

	    if (((bfd_vma) check & ~reloc_bits) != 0
		&& ((bfd_vma) check & ~reloc_bits) != (-1 & ~reloc_bits))
	      {
		/* The above right shift is incorrect for a signed
		   value.  See if turning on the upper bits fixes the
		   overflow.  */
		if (howto->rightshift > howto->bitpos
		    && (bfd_signed_vma) relocation < 0)
		  {
		    check |= ((bfd_vma) - 1
			      & ~((bfd_vma) - 1
				  >> (howto->rightshift - howto->bitpos)));
		    if (((bfd_vma) check & ~reloc_bits) != (-1 & ~reloc_bits))
		      flag = bfd_reloc_overflow;
		  }
		else
		  flag = bfd_reloc_overflow;
	      }
	  }
	  break;
	default:
	  abort ();
	}
    }

  /*
    Either we are relocating all the way, or we don't want to apply
    the relocation to the reloc entry (probably because there isn't
    any room in the output format to describe addends to relocs)
    */

  /* The cast to bfd_vma avoids a bug in the Alpha OSF/1 C compiler
     (OSF version 1.3, compiler version 3.11).  It miscompiles the
     following program:

     struct str
     {
       unsigned int i0;
     } s = { 0 };

     int
     main ()
     {
       unsigned long x;

       x = 0x100000000;
       x <<= (unsigned long) s.i0;
       if (x == 0)
	 printf ("failed\n");
       else
	 printf ("succeeded (%lx)\n", x);
     }
     */

  relocation >>= (bfd_vma) howto->rightshift;

  /* Shift everything up to where it's going to be used */

  relocation <<= (bfd_vma) howto->bitpos;

  /* Wait for the day when all have the mask in them */

  /* What we do:
     i instruction to be left alone
     o offset within instruction
     r relocation offset to apply
     S src mask
     D dst mask
     N ~dst mask
     A part 1
     B part 2
     R result

     Do this:
     i i i i i o o o o o        from bfd_get<size>
     and           S S S S S    to get the size offset we want
     +   r r r r r r r r r r  to get the final value to place
     and           D D D D D  to chop to right size
     -----------------------
     A A A A A
     And this:
     ...   i i i i i o o o o o  from bfd_get<size>
     and   N N N N N            get instruction
     -----------------------
     ...   B B B B B

     And then:
     B B B B B
     or              A A A A A
     -----------------------
     R R R R R R R R R R        put into bfd_put<size>
     */

#define DOIT(x) \
  x = ( (x & ~howto->dst_mask) | (((x & howto->src_mask) +  relocation) & howto->dst_mask))

  switch (howto->size)
    {
    case 0:
      {
	char x = bfd_get_8 (abfd, (char *) data + addr);
	DOIT (x);
	bfd_put_8 (abfd, x, (unsigned char *) data + addr);
      }
      break;

    case 1:
      if (relocation)
	{
	  short x = bfd_get_16 (abfd, (bfd_byte *) data + addr);
	  DOIT (x);
	  bfd_put_16 (abfd, x, (unsigned char *) data + addr);
	}
      break;
    case 2:
      if (relocation)
	{
	  long x = bfd_get_32 (abfd, (bfd_byte *) data + addr);
	  DOIT (x);
	  bfd_put_32 (abfd, x, (bfd_byte *) data + addr);
	}
      break;
    case -2:
      {
	long x = bfd_get_32 (abfd, (bfd_byte *) data + addr);
	relocation = -relocation;
	DOIT (x);
	bfd_put_32 (abfd, x, (bfd_byte *) data + addr);
      }
      break;

    case 3:
      /* Do nothing */
      break;

    case 4:
#ifdef BFD64
      if (relocation)
	{
	  bfd_vma x = bfd_get_64 (abfd, (bfd_byte *) data + addr);
	  DOIT (x);
	  bfd_put_64 (abfd, x, (bfd_byte *) data + addr);
	}
#else
      abort ();
#endif
      break;
    default:
      return bfd_reloc_other;
    }

  return flag;
}

/* This relocation routine is used by some of the backend linkers.
   They do not construct asymbol or arelent structures, so there is no
   reason for them to use bfd_perform_relocation.  Also,
   bfd_perform_relocation is so hacked up it is easier to write a new
   function than to try to deal with it.

   This routine does a final relocation.  It should not be used when
   generating relocateable output.

   FIXME: This routine ignores any special_function in the HOWTO,
   since the existing special_function values have been written for
   bfd_perform_relocation.

   HOWTO is the reloc howto information.
   INPUT_BFD is the BFD which the reloc applies to.
   INPUT_SECTION is the section which the reloc applies to.
   CONTENTS is the contents of the section.
   ADDRESS is the address of the reloc within INPUT_SECTION.
   VALUE is the value of the symbol the reloc refers to.
   ADDEND is the addend of the reloc.  */

bfd_reloc_status_type
_bfd_final_link_relocate (howto, input_bfd, input_section, contents, address,
			  value, addend)
     const reloc_howto_type *howto;
     bfd *input_bfd;
     asection *input_section;
     bfd_byte *contents;
     bfd_vma address;
     bfd_vma value;
     bfd_vma addend;
{
  bfd_vma relocation;

  /* Sanity check the address.  */
  if (address > input_section->_cooked_size)
    return bfd_reloc_outofrange;

  /* This function assumes that we are dealing with a basic relocation
     against a symbol.  We want to compute the value of the symbol to
     relocate to.  This is just VALUE, the value of the symbol, plus
     ADDEND, any addend associated with the reloc.  */
  relocation = value + addend;

  /* If the relocation is PC relative, we want to set RELOCATION to
     the distance between the symbol (currently in RELOCATION) and the
     location we are relocating.  Some targets (e.g., i386-aout)
     arrange for the contents of the section to be the negative of the
     offset of the location within the section; for such targets
     pcrel_offset is false.  Other targets (e.g., m88kbcs or ELF)
     simply leave the contents of the section as zero; for such
     targets pcrel_offset is true.  If pcrel_offset is false we do not
     need to subtract out the offset of the location within the
     section (which is just ADDRESS).  */
  if (howto->pc_relative)
    {
      relocation -= (input_section->output_section->vma
		     + input_section->output_offset);
      if (howto->pcrel_offset)
	relocation -= address;
    }

  return _bfd_relocate_contents (howto, input_bfd, relocation,
				 contents + address);
}

/* Relocate a given location using a given value and howto.  */

bfd_reloc_status_type
_bfd_relocate_contents (howto, input_bfd, relocation, location)
     const reloc_howto_type *howto;
     bfd *input_bfd;
     bfd_vma relocation;
     bfd_byte *location;
{
  int size;
  bfd_vma x;
  boolean overflow;

  /* If the size is negative, negate RELOCATION.  This isn't very
     general.  */
  if (howto->size < 0)
    relocation = -relocation;

  /* Get the value we are going to relocate.  */
  size = bfd_get_reloc_size (howto);
  switch (size)
    {
    default:
    case 0:
      abort ();
    case 1:
      x = bfd_get_8 (input_bfd, location);
      break;
    case 2:
      x = bfd_get_16 (input_bfd, location);
      break;
    case 4:
      x = bfd_get_32 (input_bfd, location);
      break;
    case 8:
#ifdef BFD64
      x = bfd_get_64 (input_bfd, location);
#else
      abort ();
#endif
      break;
    }

  /* Check for overflow.  FIXME: We may drop bits during the addition
     which we don't check for.  We must either check at every single
     operation, which would be tedious, or we must do the computations
     in a type larger than bfd_vma, which would be inefficient.  */
  overflow = false;
  if (howto->complain_on_overflow != complain_overflow_dont)
    {
      bfd_vma check;
      bfd_signed_vma signed_check;
      bfd_vma add;
      bfd_signed_vma signed_add;

      if (howto->rightshift == 0)
	{
	  check = relocation;
	  signed_check = (bfd_signed_vma) relocation;
	}
      else
	{
	  /* Drop unwanted bits from the value we are relocating to.  */
	  check = relocation >> howto->rightshift;

	  /* If this is a signed value, the rightshift just dropped
	     leading 1 bits (assuming twos complement).  */
	  if ((bfd_signed_vma) relocation >= 0)
	    signed_check = check;
	  else
	    signed_check = (check
			    | ((bfd_vma) - 1
			       & ~((bfd_vma) - 1 >> howto->rightshift)));
	}

      /* Get the value from the object file.  */
      add = x & howto->src_mask;

      /* Get the value from the object file with an appropriate sign.
	 The expression involving howto->src_mask isolates the upper
	 bit of src_mask.  If that bit is set in the value we are
	 adding, it is negative, and we subtract out that number times
	 two.  If src_mask includes the highest possible bit, then we
	 can not get the upper bit, but that does not matter since
	 signed_add needs no adjustment to become negative in that
	 case.  */
      signed_add = add;
      if ((add & (((~howto->src_mask) >> 1) & howto->src_mask)) != 0)
	signed_add -= (((~howto->src_mask) >> 1) & howto->src_mask) << 1;

      /* Add the value from the object file, shifted so that it is a
	 straight number.  */
      if (howto->bitpos == 0)
	{
	  check += add;
	  signed_check += signed_add;
	}
      else
	{
	  check += add >> howto->bitpos;

	  /* For the signed case we use ADD, rather than SIGNED_ADD,
	     to avoid warnings from SVR4 cc.  This is OK since we
	     explictly handle the sign bits.  */
	  if (signed_add >= 0)
	    signed_check += add >> howto->bitpos;
	  else
	    signed_check += ((add >> howto->bitpos)
			     | ((bfd_vma) - 1
				& ~((bfd_vma) - 1 >> howto->bitpos)));
	}

      switch (howto->complain_on_overflow)
	{
	case complain_overflow_signed:
	  {
	    /* Assumes two's complement.  */
	    bfd_signed_vma reloc_signed_max = (1 << (howto->bitsize - 1)) - 1;
	    bfd_signed_vma reloc_signed_min = ~reloc_signed_max;

	    if (signed_check > reloc_signed_max
		|| signed_check < reloc_signed_min)
	      overflow = true;
	  }
	  break;
	case complain_overflow_unsigned:
	  {
	    /* Assumes two's complement.  This expression avoids
	       overflow if howto->bitsize is the number of bits in
	       bfd_vma.  */
	    bfd_vma reloc_unsigned_max =
	    (((1 << (howto->bitsize - 1)) - 1) << 1) | 1;

	    if (check > reloc_unsigned_max)
	      overflow = true;
	  }
	  break;
	case complain_overflow_bitfield:
	  {
	    /* Assumes two's complement.  This expression avoids
	       overflow if howto->bitsize is the number of bits in
	       bfd_vma.  */
	    bfd_vma reloc_bits = (((1 << (howto->bitsize - 1)) - 1) << 1) | 1;

	    if ((check & ~reloc_bits) != 0
		&& (((bfd_vma) signed_check & ~reloc_bits)
		    != (-1 & ~reloc_bits)))
	      overflow = true;
	  }
	  break;
	default:
	  abort ();
	}
    }

  /* Put RELOCATION in the right bits.  */
  relocation >>= (bfd_vma) howto->rightshift;
  relocation <<= (bfd_vma) howto->bitpos;

  /* Add RELOCATION to the right bits of X.  */
  x = ((x & ~howto->dst_mask)
       | (((x & howto->src_mask) + relocation) & howto->dst_mask));

  /* Put the relocated value back in the object file.  */
  switch (size)
    {
    default:
    case 0:
      abort ();
    case 1:
      bfd_put_8 (input_bfd, x, location);
      break;
    case 2:
      bfd_put_16 (input_bfd, x, location);
      break;
    case 4:
      bfd_put_32 (input_bfd, x, location);
      break;
    case 8:
#ifdef BFD64
      bfd_put_64 (input_bfd, x, location);
#else
      abort ();
#endif
      break;
    }

  return overflow ? bfd_reloc_overflow : bfd_reloc_ok;
}

/*
DOCDD
INODE
	howto manager,  , typedef arelent, Relocations

SECTION
	The howto manager

	When an application wants to create a relocation, but doesn't
	know what the target machine might call it, it can find out by
	using this bit of code.

*/

/*
TYPEDEF
	bfd_reloc_code_type

DESCRIPTION
	The insides of a reloc code.  The idea is that, eventually, there
	will be one enumerator for every type of relocation we ever do.
	Pass one of these values to <<bfd_reloc_type_lookup>>, and it'll
	return a howto pointer.

	This does mean that the application must determine the correct
	enumerator value; you can't get a howto pointer from a random set
	of attributes.

CODE_FRAGMENT
.
.typedef enum bfd_reloc_code_real
.{
.  {* Basic absolute relocations *}
.  BFD_RELOC_64,
.  BFD_RELOC_32,
.  BFD_RELOC_26,
.  BFD_RELOC_16,
.  BFD_RELOC_14,
.  BFD_RELOC_8,
.
.  {* PC-relative relocations *}
.  BFD_RELOC_64_PCREL,
.  BFD_RELOC_32_PCREL,
.  BFD_RELOC_24_PCREL,    {* used by i960 *}
.  BFD_RELOC_16_PCREL,
.  BFD_RELOC_8_PCREL,
.
.  {* Linkage-table relative *}
.  BFD_RELOC_32_BASEREL,
.  BFD_RELOC_16_BASEREL,
.  BFD_RELOC_8_BASEREL,
.
.  {* The type of reloc used to build a contructor table - at the moment
.     probably a 32 bit wide abs address, but the cpu can choose. *}
.  BFD_RELOC_CTOR,
.
.  {* 8 bits wide, but used to form an address like 0xffnn *}
.  BFD_RELOC_8_FFnn,
.
.  {* 32-bit pc-relative, shifted right 2 bits (i.e., 30-bit
.     word displacement, e.g. for SPARC) *}
.  BFD_RELOC_32_PCREL_S2,
.  {* signed 16-bit pc-relative, shifted right 2 bits (e.g. for MIPS) *}
.  BFD_RELOC_16_PCREL_S2,
.  {* this is used on the Alpha *}
.  BFD_RELOC_23_PCREL_S2,
.
.  {* High 22 bits of 32-bit value, placed into lower 22 bits of
.     target word; simple reloc.  *}
.  BFD_RELOC_HI22,
.  {* Low 10 bits.  *}
.  BFD_RELOC_LO10,
.
.  {* For systems that allocate a Global Pointer register, these are
.     displacements off that register.  These relocation types are
.     handled specially, because the value the register will have is
.     decided relatively late.  *}
.  BFD_RELOC_GPREL16,
.  BFD_RELOC_GPREL32,
.
.  {* Reloc types used for i960/b.out.  *}
.  BFD_RELOC_I960_CALLJ,
.
.  {* now for the sparc/elf codes *}
.  BFD_RELOC_NONE,		{* actually used *}
.  BFD_RELOC_SPARC_WDISP22,
.  BFD_RELOC_SPARC22,
.  BFD_RELOC_SPARC13,
.  BFD_RELOC_SPARC_GOT10,
.  BFD_RELOC_SPARC_GOT13,
.  BFD_RELOC_SPARC_GOT22,
.  BFD_RELOC_SPARC_PC10,
.  BFD_RELOC_SPARC_PC22,
.  BFD_RELOC_SPARC_WPLT30,
.  BFD_RELOC_SPARC_COPY,
.  BFD_RELOC_SPARC_GLOB_DAT,
.  BFD_RELOC_SPARC_JMP_SLOT,
.  BFD_RELOC_SPARC_RELATIVE,
.  BFD_RELOC_SPARC_UA32,
.
.  {* these are a.out specific? *}
.  BFD_RELOC_SPARC_BASE13,
.  BFD_RELOC_SPARC_BASE22,
.
.  {* some relocations we're using for sparc v9
.     -- subject to change *}
.  BFD_RELOC_SPARC_10,
.  BFD_RELOC_SPARC_11,
.#define  BFD_RELOC_SPARC_64 BFD_RELOC_64
.  BFD_RELOC_SPARC_OLO10,
.  BFD_RELOC_SPARC_HH22,
.  BFD_RELOC_SPARC_HM10,
.  BFD_RELOC_SPARC_LM22,
.  BFD_RELOC_SPARC_PC_HH22,
.  BFD_RELOC_SPARC_PC_HM10,
.  BFD_RELOC_SPARC_PC_LM22,
.  BFD_RELOC_SPARC_WDISP16,
.  BFD_RELOC_SPARC_WDISP19,
.  BFD_RELOC_SPARC_GLOB_JMP,
.  BFD_RELOC_SPARC_LO7,
.
.  {* Alpha ECOFF relocations.  Some of these treat the symbol or "addend"
.     in some special way.  *}
.  {* For GPDISP_HI16 ("gpdisp") relocations, the symbol is ignored when
.     writing; when reading, it will be the absolute section symbol.  The
.     addend is the displacement in bytes of the "lda" instruction from
.     the "ldah" instruction (which is at the address of this reloc).  *}
.  BFD_RELOC_ALPHA_GPDISP_HI16,
.  {* For GPDISP_LO16 ("ignore") relocations, the symbol is handled as
.     with GPDISP_HI16 relocs.  The addend is ignored when writing the
.     relocations out, and is filled in with the file's GP value on
.     reading, for convenience.  *}
.  BFD_RELOC_ALPHA_GPDISP_LO16,
.
.  {* The Alpha LITERAL/LITUSE relocs are produced by a symbol reference;
.     the assembler turns it into a LDQ instruction to load the address of
.     the symbol, and then fills in a register in the real instruction.
.
.     The LITERAL reloc, at the LDQ instruction, refers to the .lita
.     section symbol.  The addend is ignored when writing, but is filled
.     in with the file's GP value on reading, for convenience, as with the
.     GPDISP_LO16 reloc.
.
.     The LITUSE reloc, on the instruction using the loaded address, gives
.     information to the linker that it might be able to use to optimize
.     away some literal section references.  The symbol is ignored (read
.     as the absolute section symbol), and the "addend" indicates the type
.     of instruction using the register:
.              1 - "memory" fmt insn
.              2 - byte-manipulation (byte offset reg)
.              3 - jsr (target of branch)
.
.     The GNU linker currently doesn't do any of this optimizing.  *}
.  BFD_RELOC_ALPHA_LITERAL,
.  BFD_RELOC_ALPHA_LITUSE,
.
.  {* The HINT relocation indicates a value that should be filled into the
.     "hint" field of a jmp/jsr/ret instruction, for possible branch-
.     prediction logic which may be provided on some processors.  *}
.  BFD_RELOC_ALPHA_HINT,
.
.  {* Bits 27..2 of the relocation address shifted right 2 bits;
.     simple reloc otherwise.  *}
.  BFD_RELOC_MIPS_JMP,
.
.  {* High 16 bits of 32-bit value; simple reloc.  *}
.  BFD_RELOC_HI16,
.  {* High 16 bits of 32-bit value but the low 16 bits will be sign
.     extended and added to form the final result.  If the low 16
.     bits form a negative number, we need to add one to the high value
.     to compensate for the borrow when the low bits are added.  *}
.  BFD_RELOC_HI16_S,
.  {* Low 16 bits.  *}
.  BFD_RELOC_LO16,
.  {* Like BFD_RELOC_HI16_S, but PC relative.  *}
.  BFD_RELOC_PCREL_HI16_S,
.  {* Like BFD_RELOC_LO16, but PC relative.  *}
.  BFD_RELOC_PCREL_LO16,
.
.  {* relocation relative to the global pointer.  *}
.#define BFD_RELOC_MIPS_GPREL BFD_RELOC_GPREL16
.
.  {* Relocation against a MIPS literal section.  *}
.  BFD_RELOC_MIPS_LITERAL,
.
.  {* MIPS ELF relocations.  *}
.  BFD_RELOC_MIPS_GOT16,
.  BFD_RELOC_MIPS_CALL16,
.#define BFD_RELOC_MIPS_GPREL32 BFD_RELOC_GPREL32
.
.  {* i386/elf relocations *}
.  BFD_RELOC_386_GOT32,
.  BFD_RELOC_386_PLT32,
.  BFD_RELOC_386_COPY,
.  BFD_RELOC_386_GLOB_DAT,
.  BFD_RELOC_386_JUMP_SLOT,
.  BFD_RELOC_386_RELATIVE,
.  BFD_RELOC_386_GOTOFF,
.  BFD_RELOC_386_GOTPC,
.
.  {* ns32k relocations *}
.  BFD_RELOC_NS32K_IMM_8,
.  BFD_RELOC_NS32K_IMM_16,
.  BFD_RELOC_NS32K_IMM_32,
.  BFD_RELOC_NS32K_IMM_8_PCREL,
.  BFD_RELOC_NS32K_IMM_16_PCREL,
.  BFD_RELOC_NS32K_IMM_32_PCREL,
.  BFD_RELOC_NS32K_DISP_8,
.  BFD_RELOC_NS32K_DISP_16,
.  BFD_RELOC_NS32K_DISP_32,
.  BFD_RELOC_NS32K_DISP_8_PCREL,
.  BFD_RELOC_NS32K_DISP_16_PCREL,
.  BFD_RELOC_NS32K_DISP_32_PCREL,
.
.  {* PowerPC/POWER (RS/6000) relocs.  *}
.  {* 26 bit relative branch.  Low two bits must be zero.  High 24
.     bits installed in bits 6 through 29 of instruction.  *}
.  BFD_RELOC_PPC_B26,
.  {* 26 bit absolute branch, like BFD_RELOC_PPC_B26 but absolute.  *}
.  BFD_RELOC_PPC_BA26,
.  {* 16 bit TOC relative reference.  *}
.  BFD_RELOC_PPC_TOC16,
.
.  {* this must be the highest numeric value *}
.  BFD_RELOC_UNUSED
. } bfd_reloc_code_real_type;
*/


/*
FUNCTION
	bfd_reloc_type_lookup

SYNOPSIS
	const struct reloc_howto_struct *
	bfd_reloc_type_lookup (bfd *abfd, bfd_reloc_code_real_type code);

DESCRIPTION
	Return a pointer to a howto structure which, when
	invoked, will perform the relocation @var{code} on data from the
	architecture noted.

*/


const struct reloc_howto_struct *
bfd_reloc_type_lookup (abfd, code)
     bfd *abfd;
     bfd_reloc_code_real_type code;
{
  return BFD_SEND (abfd, reloc_type_lookup, (abfd, code));
}

static reloc_howto_type bfd_howto_32 =
HOWTO (0, 00, 2, 32, false, 0, complain_overflow_bitfield, 0, "VRT32", false, 0xffffffff, 0xffffffff, true);


/*
INTERNAL_FUNCTION
	bfd_default_reloc_type_lookup

SYNOPSIS
	const struct reloc_howto_struct *bfd_default_reloc_type_lookup
	(bfd *abfd, bfd_reloc_code_real_type  code);

DESCRIPTION
	Provides a default relocation lookup routine for any architecture.


*/

const struct reloc_howto_struct *
bfd_default_reloc_type_lookup (abfd, code)
     bfd *abfd;
     bfd_reloc_code_real_type code;
{
  switch (code)
    {
    case BFD_RELOC_CTOR:
      /* The type of reloc used in a ctor, which will be as wide as the
	 address - so either a 64, 32, or 16 bitter.  */
      switch (bfd_get_arch_info (abfd)->bits_per_address)
	{
	case 64:
	  BFD_FAIL ();
	case 32:
	  return &bfd_howto_32;
	case 16:
	  BFD_FAIL ();
	default:
	  BFD_FAIL ();
	}
    default:
      BFD_FAIL ();
    }
  return (const struct reloc_howto_struct *) NULL;
}


/*
INTERNAL_FUNCTION
	bfd_generic_relax_section

SYNOPSIS
	boolean bfd_generic_relax_section
	 (bfd *abfd,
	  asection *section,
	  struct bfd_link_info *,
	  boolean *);

DESCRIPTION
	Provides default handling for relaxing for back ends which
	don't do relaxing -- i.e., does nothing.
*/

/*ARGSUSED*/
boolean
bfd_generic_relax_section (abfd, section, link_info, again)
     bfd *abfd;
     asection *section;
     struct bfd_link_info *link_info;
     boolean *again;
{
  *again = false;
  return true;
}

/*
INTERNAL_FUNCTION
	bfd_generic_get_relocated_section_contents

SYNOPSIS
	bfd_byte *
	   bfd_generic_get_relocated_section_contents (bfd *abfd,
	     struct bfd_link_info *link_info,
	     struct bfd_link_order *link_order,
	     bfd_byte *data,
	     boolean relocateable,
	     asymbol **symbols);

DESCRIPTION
	Provides default handling of relocation effort for back ends
	which can't be bothered to do it efficiently.

*/

bfd_byte *
bfd_generic_get_relocated_section_contents (abfd, link_info, link_order, data,
					    relocateable, symbols)
     bfd *abfd;
     struct bfd_link_info *link_info;
     struct bfd_link_order *link_order;
     bfd_byte *data;
     boolean relocateable;
     asymbol **symbols;
{
  /* Get enough memory to hold the stuff */
  bfd *input_bfd = link_order->u.indirect.section->owner;
  asection *input_section = link_order->u.indirect.section;

  long reloc_size = bfd_get_reloc_upper_bound (input_bfd, input_section);
  arelent **reloc_vector = NULL;
  long reloc_count;

  if (reloc_size < 0)
    goto error_return;

  reloc_vector = (arelent **) malloc (reloc_size);
  if (reloc_vector == NULL && reloc_size != 0)
    {
      bfd_set_error (bfd_error_no_memory);
      goto error_return;
    }

  /* read in the section */
  if (!bfd_get_section_contents (input_bfd,
				 input_section,
				 (PTR) data,
				 0,
				 input_section->_raw_size))
    goto error_return;

  /* We're not relaxing the section, so just copy the size info */
  input_section->_cooked_size = input_section->_raw_size;
  input_section->reloc_done = true;

  reloc_count = bfd_canonicalize_reloc (input_bfd,
					input_section,
					reloc_vector,
					symbols);
  if (reloc_count < 0)
    goto error_return;

  if (reloc_count > 0)
    {
      arelent **parent;
      for (parent = reloc_vector; *parent != (arelent *) NULL;
	   parent++)
	{
	  char *error_message = (char *) NULL;
	  bfd_reloc_status_type r =
	    bfd_perform_relocation (input_bfd,
				    *parent,
				    (PTR) data,
				    input_section,
				    relocateable ? abfd : (bfd *) NULL,
				    &error_message);

	  if (relocateable)
	    {
	      asection *os = input_section->output_section;

	      /* A partial link, so keep the relocs */
	      os->orelocation[os->reloc_count] = *parent;
	      os->reloc_count++;
	    }

	  if (r != bfd_reloc_ok)
	    {
	      switch (r)
		{
		case bfd_reloc_undefined:
		  if (!((*link_info->callbacks->undefined_symbol)
			(link_info, bfd_asymbol_name (*(*parent)->sym_ptr_ptr),
			 input_bfd, input_section, (*parent)->address)))
		    goto error_return;
		  break;
		case bfd_reloc_dangerous:
		  BFD_ASSERT (error_message != (char *) NULL);
		  if (!((*link_info->callbacks->reloc_dangerous)
			(link_info, error_message, input_bfd, input_section,
			 (*parent)->address)))
		    goto error_return;
		  break;
		case bfd_reloc_overflow:
		  if (!((*link_info->callbacks->reloc_overflow)
			(link_info, bfd_asymbol_name (*(*parent)->sym_ptr_ptr),
			 (*parent)->howto->name, (*parent)->addend,
			 input_bfd, input_section, (*parent)->address)))
		    goto error_return;
		  break;
		case bfd_reloc_outofrange:
		default:
		  abort ();
		  break;
		}

	    }
	}
    }
  if (reloc_vector != NULL)
    free (reloc_vector);
  return data;

error_return:
  if (reloc_vector != NULL)
    free (reloc_vector);
  return NULL;
}
