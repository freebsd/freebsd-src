/* BFD support for handling relocation entries.
   Copyright 1990, 1991, 1992, 1993, 1994, 1995, 1996, 1997, 1998, 1999,
   2000, 2001
   Free Software Foundation, Inc.
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
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.  */

/*
SECTION
	Relocations

	BFD maintains relocations in much the same way it maintains
	symbols: they are left alone until required, then read in
	en-masse and translated into an internal form.  A common
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

/* DO compile in the reloc_code name table from libbfd.h.  */
#define _BFD_MAKE_TABLE_bfd_reloc_code_real

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
.  reloc_howto_type *howto;
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
.struct reloc_howto_struct
.{
.       {*  The type field has mainly a documentary use - the back end can
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
.       {* Some formats record a relocation addend in the section contents
.          rather than with the relocation.  For ELF formats this is the
.          distinction between USE_REL and USE_RELA (though the code checks
.          for USE_REL == 1/0).  The value of this field is TRUE if the
.          addend is recorded with the section contents; when performing a
.          partial link (ld -r) the section contents (the data) will be
.          modified.  The value of this field is FALSE if addends are
.          recorded with the relocation (in arelent.addend); when performing
.          a partial link the relocation will be modified.
.          All relocations for all ELF USE_RELA targets should set this field
.          to FALSE (values of TRUE should be looked on with suspicion).
.          However, the converse is not true: not all relocations of all ELF
.          USE_REL targets set this field to TRUE.  Why this is so is peculiar
.          to each particular target.  For relocs that aren't used in partial
.          links (e.g. GOT stuff) it doesn't matter what this is set to.  *}
.  boolean partial_inplace;
.
.       {* The src_mask selects which parts of the read in data
.          are to be used in the relocation sum.  E.g., if this was an 8 bit
.          byte of data which we read and relocated, this would be
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
	This is used to fill in an empty howto entry in an array.

.#define EMPTY_HOWTO(C) \
.  HOWTO((C),0,0,0,false,0,complain_overflow_dont,NULL,NULL,false,0,0,false)
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
	unsigned int bfd_get_reloc_size (reloc_howto_type *);

DESCRIPTION
	For a reloc_howto_type that operates on a fixed number of bytes,
	this returns the number of bytes operated on.
 */

unsigned int
bfd_get_reloc_size (howto)
     reloc_howto_type *howto;
{
  switch (howto->size)
    {
    case 0: return 1;
    case 1: return 2;
    case 2: return 4;
    case 3: return 0;
    case 4: return 8;
    case 8: return 16;
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

/* N_ONES produces N one bits, without overflowing machine arithmetic.  */
#define N_ONES(n) (((((bfd_vma) 1 << ((n) - 1)) - 1) << 1) | 1)

/*
FUNCTION
	bfd_check_overflow

SYNOPSIS
	bfd_reloc_status_type
		bfd_check_overflow
			(enum complain_overflow how,
			 unsigned int bitsize,
			 unsigned int rightshift,
			 unsigned int addrsize,
			 bfd_vma relocation);

DESCRIPTION
	Perform overflow checking on @var{relocation} which has
	@var{bitsize} significant bits and will be shifted right by
	@var{rightshift} bits, on a machine with addresses containing
	@var{addrsize} significant bits.  The result is either of
	@code{bfd_reloc_ok} or @code{bfd_reloc_overflow}.

*/

bfd_reloc_status_type
bfd_check_overflow (how, bitsize, rightshift, addrsize, relocation)
     enum complain_overflow how;
     unsigned int bitsize;
     unsigned int rightshift;
     unsigned int addrsize;
     bfd_vma relocation;
{
  bfd_vma fieldmask, addrmask, signmask, ss, a;
  bfd_reloc_status_type flag = bfd_reloc_ok;

  a = relocation;

  /* Note: BITSIZE should always be <= ADDRSIZE, but in case it's not,
     we'll be permissive: extra bits in the field mask will
     automatically extend the address mask for purposes of the
     overflow check.  */
  fieldmask = N_ONES (bitsize);
  addrmask = N_ONES (addrsize) | fieldmask;

  switch (how)
    {
    case complain_overflow_dont:
      break;

    case complain_overflow_signed:
      /* If any sign bits are set, all sign bits must be set.  That
         is, A must be a valid negative address after shifting.  */
      a = (a & addrmask) >> rightshift;
      signmask = ~ (fieldmask >> 1);
      ss = a & signmask;
      if (ss != 0 && ss != ((addrmask >> rightshift) & signmask))
	flag = bfd_reloc_overflow;
      break;

    case complain_overflow_unsigned:
      /* We have an overflow if the address does not fit in the field.  */
      a = (a & addrmask) >> rightshift;
      if ((a & ~ fieldmask) != 0)
	flag = bfd_reloc_overflow;
      break;

    case complain_overflow_bitfield:
      /* Bitfields are sometimes signed, sometimes unsigned.  We
	 explicitly allow an address wrap too, which means a bitfield
	 of n bits is allowed to store -2**n to 2**n-1.  Thus overflow
	 if the value has some, but not all, bits set outside the
	 field.  */
      a >>= rightshift;
      ss = a & ~ fieldmask;
      if (ss != 0 && ss != (((bfd_vma) -1 >> rightshift) & ~ fieldmask))
	flag = bfd_reloc_overflow;
      break;

    default:
      abort ();
    }

  return flag;
}

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
  bfd_size_type octets = reloc_entry->address * bfd_octets_per_byte (abfd);
  bfd_vma output_base = 0;
  reloc_howto_type *howto = reloc_entry->howto;
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
  if (reloc_entry->address > input_section->_cooked_size /
      bfd_octets_per_byte (abfd))
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
  if (howto->complain_on_overflow != complain_overflow_dont
      && flag == bfd_reloc_ok)
    flag = bfd_check_overflow (howto->complain_on_overflow,
			       howto->bitsize,
			       howto->rightshift,
			       bfd_arch_bits_per_address (abfd),
			       relocation);

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
     ((  i i i i i o o o o o  from bfd_get<size>
     and           S S S S S) to get the size offset we want
     +   r r r r r r r r r r) to get the final value to place
     and           D D D D D  to chop to right size
     -----------------------
     =             A A A A A
     And this:
     (   i i i i i o o o o o  from bfd_get<size>
     and N N N N N          ) get instruction
     -----------------------
     =   B B B B B

     And then:
     (   B B B B B
     or            A A A A A)
     -----------------------
     =   R R R R R R R R R R  put into bfd_put<size>
     */

#define DOIT(x) \
  x = ( (x & ~howto->dst_mask) | (((x & howto->src_mask) +  relocation) & howto->dst_mask))

  switch (howto->size)
    {
    case 0:
      {
	char x = bfd_get_8 (abfd, (char *) data + octets);
	DOIT (x);
	bfd_put_8 (abfd, x, (unsigned char *) data + octets);
      }
      break;

    case 1:
      {
	short x = bfd_get_16 (abfd, (bfd_byte *) data + octets);
	DOIT (x);
	bfd_put_16 (abfd, x, (unsigned char *) data + octets);
      }
      break;
    case 2:
      {
	long x = bfd_get_32 (abfd, (bfd_byte *) data + octets);
	DOIT (x);
	bfd_put_32 (abfd, x, (bfd_byte *) data + octets);
      }
      break;
    case -2:
      {
	long x = bfd_get_32 (abfd, (bfd_byte *) data + octets);
	relocation = -relocation;
	DOIT (x);
	bfd_put_32 (abfd, x, (bfd_byte *) data + octets);
      }
      break;

    case -1:
      {
	long x = bfd_get_16 (abfd, (bfd_byte *) data + octets);
	relocation = -relocation;
	DOIT (x);
	bfd_put_16 (abfd, x, (bfd_byte *) data + octets);
      }
      break;

    case 3:
      /* Do nothing */
      break;

    case 4:
#ifdef BFD64
      {
	bfd_vma x = bfd_get_64 (abfd, (bfd_byte *) data + octets);
	DOIT (x);
	bfd_put_64 (abfd, x, (bfd_byte *) data + octets);
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

/*
FUNCTION
	bfd_install_relocation

SYNOPSIS
	bfd_reloc_status_type
                bfd_install_relocation
                        (bfd *abfd,
                         arelent *reloc_entry,
                         PTR data, bfd_vma data_start,
                         asection *input_section,
			 char **error_message);

DESCRIPTION
	This looks remarkably like <<bfd_perform_relocation>>, except it
	does not expect that the section contents have been filled in.
	I.e., it's suitable for use when creating, rather than applying
	a relocation.

	For now, this function should be considered reserved for the
	assembler.

*/

bfd_reloc_status_type
bfd_install_relocation (abfd, reloc_entry, data_start, data_start_offset,
			input_section, error_message)
     bfd *abfd;
     arelent *reloc_entry;
     PTR data_start;
     bfd_vma data_start_offset;
     asection *input_section;
     char **error_message;
{
  bfd_vma relocation;
  bfd_reloc_status_type flag = bfd_reloc_ok;
  bfd_size_type octets = reloc_entry->address * bfd_octets_per_byte (abfd);
  bfd_vma output_base = 0;
  reloc_howto_type *howto = reloc_entry->howto;
  asection *reloc_target_output_section;
  asymbol *symbol;
  bfd_byte *data;

  symbol = *(reloc_entry->sym_ptr_ptr);
  if (bfd_is_abs_section (symbol->section))
    {
      reloc_entry->address += input_section->output_offset;
      return bfd_reloc_ok;
    }

  /* If there is a function supplied to handle this relocation type,
     call it.  It'll return `bfd_reloc_continue' if further processing
     can be done.  */
  if (howto->special_function)
    {
      bfd_reloc_status_type cont;

      /* XXX - The special_function calls haven't been fixed up to deal
	 with creating new relocations and section contents.  */
      cont = howto->special_function (abfd, reloc_entry, symbol,
				      /* XXX - Non-portable! */
				      ((bfd_byte *) data_start
				       - data_start_offset),
				      input_section, abfd, error_message);
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
  if (howto->partial_inplace == false)
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

      if (howto->pcrel_offset == true && howto->partial_inplace == true)
	relocation -= reloc_entry->address;
    }

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

Hmmm.  The first obvious point is that bfd_install_relocation should
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
bfd_install_relocation is not going to.  If you remove that line, then
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

  /* FIXME: This overflow checking is incomplete, because the value
     might have overflowed before we get here.  For a correct check we
     need to compute the value in a size larger than bitsize, but we
     can't reasonably do that for a reloc the same size as a host
     machine word.
     FIXME: We should also do overflow checking on the result after
     adding in the value contained in the object file.  */
  if (howto->complain_on_overflow != complain_overflow_dont)
    flag = bfd_check_overflow (howto->complain_on_overflow,
			       howto->bitsize,
			       howto->rightshift,
			       bfd_arch_bits_per_address (abfd),
			       relocation);

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
     ((  i i i i i o o o o o  from bfd_get<size>
     and           S S S S S) to get the size offset we want
     +   r r r r r r r r r r) to get the final value to place
     and           D D D D D  to chop to right size
     -----------------------
     =             A A A A A
     And this:
     (   i i i i i o o o o o  from bfd_get<size>
     and N N N N N          ) get instruction
     -----------------------
     =   B B B B B

     And then:
     (   B B B B B
     or            A A A A A)
     -----------------------
     =   R R R R R R R R R R  put into bfd_put<size>
     */

#define DOIT(x) \
  x = ( (x & ~howto->dst_mask) | (((x & howto->src_mask) +  relocation) & howto->dst_mask))

  data = (bfd_byte *) data_start + (octets - data_start_offset);

  switch (howto->size)
    {
    case 0:
      {
	char x = bfd_get_8 (abfd, (char *) data);
	DOIT (x);
	bfd_put_8 (abfd, x, (unsigned char *) data);
      }
      break;

    case 1:
      {
	short x = bfd_get_16 (abfd, (bfd_byte *) data);
	DOIT (x);
	bfd_put_16 (abfd, x, (unsigned char *) data);
      }
      break;
    case 2:
      {
	long x = bfd_get_32 (abfd, (bfd_byte *) data);
	DOIT (x);
	bfd_put_32 (abfd, x, (bfd_byte *) data);
      }
      break;
    case -2:
      {
	long x = bfd_get_32 (abfd, (bfd_byte *) data);
	relocation = -relocation;
	DOIT (x);
	bfd_put_32 (abfd, x, (bfd_byte *) data);
      }
      break;

    case 3:
      /* Do nothing */
      break;

    case 4:
      {
	bfd_vma x = bfd_get_64 (abfd, (bfd_byte *) data);
	DOIT (x);
	bfd_put_64 (abfd, x, (bfd_byte *) data);
      }
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

   This routine does a final relocation.  Whether it is useful for a
   relocateable link depends upon how the object format defines
   relocations.

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
     reloc_howto_type *howto;
     bfd *input_bfd;
     asection *input_section;
     bfd_byte *contents;
     bfd_vma address;
     bfd_vma value;
     bfd_vma addend;
{
  bfd_vma relocation;

  /* Sanity check the address.  */
  if (address > input_section->_raw_size)
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
     reloc_howto_type *howto;
     bfd *input_bfd;
     bfd_vma relocation;
     bfd_byte *location;
{
  int size;
  bfd_vma x = 0;
  bfd_reloc_status_type flag;
  unsigned int rightshift = howto->rightshift;
  unsigned int bitpos = howto->bitpos;

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
  flag = bfd_reloc_ok;
  if (howto->complain_on_overflow != complain_overflow_dont)
    {
      bfd_vma addrmask, fieldmask, signmask, ss;
      bfd_vma a, b, sum;

      /* Get the values to be added together.  For signed and unsigned
         relocations, we assume that all values should be truncated to
         the size of an address.  For bitfields, all the bits matter.
         See also bfd_check_overflow.  */
      fieldmask = N_ONES (howto->bitsize);
      addrmask = N_ONES (bfd_arch_bits_per_address (input_bfd)) | fieldmask;
      a = relocation;
      b = x & howto->src_mask;

      switch (howto->complain_on_overflow)
	{
	case complain_overflow_signed:
	  a = (a & addrmask) >> rightshift;

	  /* If any sign bits are set, all sign bits must be set.
	     That is, A must be a valid negative address after
	     shifting.  */
	  signmask = ~ (fieldmask >> 1);
	  ss = a & signmask;
	  if (ss != 0 && ss != ((addrmask >> rightshift) & signmask))
	    flag = bfd_reloc_overflow;

	  /* We only need this next bit of code if the sign bit of B
             is below the sign bit of A.  This would only happen if
             SRC_MASK had fewer bits than BITSIZE.  Note that if
             SRC_MASK has more bits than BITSIZE, we can get into
             trouble; we would need to verify that B is in range, as
             we do for A above.  */
	  signmask = ((~ howto->src_mask) >> 1) & howto->src_mask;

	  /* Set all the bits above the sign bit.  */
	  b = (b ^ signmask) - signmask;

	  b = (b & addrmask) >> bitpos;

	  /* Now we can do the addition.  */
	  sum = a + b;

	  /* See if the result has the correct sign.  Bits above the
             sign bit are junk now; ignore them.  If the sum is
             positive, make sure we did not have all negative inputs;
             if the sum is negative, make sure we did not have all
             positive inputs.  The test below looks only at the sign
             bits, and it really just
	         SIGN (A) == SIGN (B) && SIGN (A) != SIGN (SUM)
	     */
	  signmask = (fieldmask >> 1) + 1;
	  if (((~ (a ^ b)) & (a ^ sum)) & signmask)
	    flag = bfd_reloc_overflow;

	  break;

	case complain_overflow_unsigned:
	  /* Checking for an unsigned overflow is relatively easy:
             trim the addresses and add, and trim the result as well.
             Overflow is normally indicated when the result does not
             fit in the field.  However, we also need to consider the
             case when, e.g., fieldmask is 0x7fffffff or smaller, an
             input is 0x80000000, and bfd_vma is only 32 bits; then we
             will get sum == 0, but there is an overflow, since the
             inputs did not fit in the field.  Instead of doing a
             separate test, we can check for this by or-ing in the
             operands when testing for the sum overflowing its final
             field.  */
	  a = (a & addrmask) >> rightshift;
	  b = (b & addrmask) >> bitpos;
	  sum = (a + b) & addrmask;
	  if ((a | b | sum) & ~ fieldmask)
	    flag = bfd_reloc_overflow;

	  break;

	case complain_overflow_bitfield:
	  /* Much like the signed check, but for a field one bit
	     wider, and no trimming inputs with addrmask.  We allow a
	     bitfield to represent numbers in the range -2**n to
	     2**n-1, where n is the number of bits in the field.
	     Note that when bfd_vma is 32 bits, a 32-bit reloc can't
	     overflow, which is exactly what we want.  */
	  a >>= rightshift;

	  signmask = ~ fieldmask;
	  ss = a & signmask;
	  if (ss != 0 && ss != (((bfd_vma) -1 >> rightshift) & signmask))
	    flag = bfd_reloc_overflow;

	  signmask = ((~ howto->src_mask) >> 1) & howto->src_mask;
	  b = (b ^ signmask) - signmask;

	  b >>= bitpos;

	  sum = a + b;

	  /* We mask with addrmask here to explicitly allow an address
	     wrap-around.  The Linux kernel relies on it, and it is
	     the only way to write assembler code which can run when
	     loaded at a location 0x80000000 away from the location at
	     which it is linked.  */
	  signmask = fieldmask + 1;
	  if (((~ (a ^ b)) & (a ^ sum)) & signmask & addrmask)
	    flag = bfd_reloc_overflow;

	  break;

	default:
	  abort ();
	}
    }

  /* Put RELOCATION in the right bits.  */
  relocation >>= (bfd_vma) rightshift;
  relocation <<= (bfd_vma) bitpos;

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

  return flag;
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

SENUM
   bfd_reloc_code_real

ENUM
  BFD_RELOC_64
ENUMX
  BFD_RELOC_32
ENUMX
  BFD_RELOC_26
ENUMX
  BFD_RELOC_24
ENUMX
  BFD_RELOC_16
ENUMX
  BFD_RELOC_14
ENUMX
  BFD_RELOC_8
ENUMDOC
  Basic absolute relocations of N bits.

ENUM
  BFD_RELOC_64_PCREL
ENUMX
  BFD_RELOC_32_PCREL
ENUMX
  BFD_RELOC_24_PCREL
ENUMX
  BFD_RELOC_16_PCREL
ENUMX
  BFD_RELOC_12_PCREL
ENUMX
  BFD_RELOC_8_PCREL
ENUMDOC
  PC-relative relocations.  Sometimes these are relative to the address
of the relocation itself; sometimes they are relative to the start of
the section containing the relocation.  It depends on the specific target.

The 24-bit relocation is used in some Intel 960 configurations.

ENUM
  BFD_RELOC_32_GOT_PCREL
ENUMX
  BFD_RELOC_16_GOT_PCREL
ENUMX
  BFD_RELOC_8_GOT_PCREL
ENUMX
  BFD_RELOC_32_GOTOFF
ENUMX
  BFD_RELOC_16_GOTOFF
ENUMX
  BFD_RELOC_LO16_GOTOFF
ENUMX
  BFD_RELOC_HI16_GOTOFF
ENUMX
  BFD_RELOC_HI16_S_GOTOFF
ENUMX
  BFD_RELOC_8_GOTOFF
ENUMX
  BFD_RELOC_32_PLT_PCREL
ENUMX
  BFD_RELOC_24_PLT_PCREL
ENUMX
  BFD_RELOC_16_PLT_PCREL
ENUMX
  BFD_RELOC_8_PLT_PCREL
ENUMX
  BFD_RELOC_32_PLTOFF
ENUMX
  BFD_RELOC_16_PLTOFF
ENUMX
  BFD_RELOC_LO16_PLTOFF
ENUMX
  BFD_RELOC_HI16_PLTOFF
ENUMX
  BFD_RELOC_HI16_S_PLTOFF
ENUMX
  BFD_RELOC_8_PLTOFF
ENUMDOC
  For ELF.

ENUM
  BFD_RELOC_68K_GLOB_DAT
ENUMX
  BFD_RELOC_68K_JMP_SLOT
ENUMX
  BFD_RELOC_68K_RELATIVE
ENUMDOC
  Relocations used by 68K ELF.

ENUM
  BFD_RELOC_32_BASEREL
ENUMX
  BFD_RELOC_16_BASEREL
ENUMX
  BFD_RELOC_LO16_BASEREL
ENUMX
  BFD_RELOC_HI16_BASEREL
ENUMX
  BFD_RELOC_HI16_S_BASEREL
ENUMX
  BFD_RELOC_8_BASEREL
ENUMX
  BFD_RELOC_RVA
ENUMDOC
  Linkage-table relative.

ENUM
  BFD_RELOC_8_FFnn
ENUMDOC
  Absolute 8-bit relocation, but used to form an address like 0xFFnn.

ENUM
  BFD_RELOC_32_PCREL_S2
ENUMX
  BFD_RELOC_16_PCREL_S2
ENUMX
  BFD_RELOC_23_PCREL_S2
ENUMDOC
  These PC-relative relocations are stored as word displacements --
i.e., byte displacements shifted right two bits.  The 30-bit word
displacement (<<32_PCREL_S2>> -- 32 bits, shifted 2) is used on the
SPARC.  (SPARC tools generally refer to this as <<WDISP30>>.)  The
signed 16-bit displacement is used on the MIPS, and the 23-bit
displacement is used on the Alpha.

ENUM
  BFD_RELOC_HI22
ENUMX
  BFD_RELOC_LO10
ENUMDOC
  High 22 bits and low 10 bits of 32-bit value, placed into lower bits of
the target word.  These are used on the SPARC.

ENUM
  BFD_RELOC_GPREL16
ENUMX
  BFD_RELOC_GPREL32
ENUMDOC
  For systems that allocate a Global Pointer register, these are
displacements off that register.  These relocation types are
handled specially, because the value the register will have is
decided relatively late.

ENUM
  BFD_RELOC_I960_CALLJ
ENUMDOC
  Reloc types used for i960/b.out.

ENUM
  BFD_RELOC_NONE
ENUMX
  BFD_RELOC_SPARC_WDISP22
ENUMX
  BFD_RELOC_SPARC22
ENUMX
  BFD_RELOC_SPARC13
ENUMX
  BFD_RELOC_SPARC_GOT10
ENUMX
  BFD_RELOC_SPARC_GOT13
ENUMX
  BFD_RELOC_SPARC_GOT22
ENUMX
  BFD_RELOC_SPARC_PC10
ENUMX
  BFD_RELOC_SPARC_PC22
ENUMX
  BFD_RELOC_SPARC_WPLT30
ENUMX
  BFD_RELOC_SPARC_COPY
ENUMX
  BFD_RELOC_SPARC_GLOB_DAT
ENUMX
  BFD_RELOC_SPARC_JMP_SLOT
ENUMX
  BFD_RELOC_SPARC_RELATIVE
ENUMX
  BFD_RELOC_SPARC_UA16
ENUMX
  BFD_RELOC_SPARC_UA32
ENUMX
  BFD_RELOC_SPARC_UA64
ENUMDOC
  SPARC ELF relocations.  There is probably some overlap with other
  relocation types already defined.

ENUM
  BFD_RELOC_SPARC_BASE13
ENUMX
  BFD_RELOC_SPARC_BASE22
ENUMDOC
  I think these are specific to SPARC a.out (e.g., Sun 4).

ENUMEQ
  BFD_RELOC_SPARC_64
  BFD_RELOC_64
ENUMX
  BFD_RELOC_SPARC_10
ENUMX
  BFD_RELOC_SPARC_11
ENUMX
  BFD_RELOC_SPARC_OLO10
ENUMX
  BFD_RELOC_SPARC_HH22
ENUMX
  BFD_RELOC_SPARC_HM10
ENUMX
  BFD_RELOC_SPARC_LM22
ENUMX
  BFD_RELOC_SPARC_PC_HH22
ENUMX
  BFD_RELOC_SPARC_PC_HM10
ENUMX
  BFD_RELOC_SPARC_PC_LM22
ENUMX
  BFD_RELOC_SPARC_WDISP16
ENUMX
  BFD_RELOC_SPARC_WDISP19
ENUMX
  BFD_RELOC_SPARC_7
ENUMX
  BFD_RELOC_SPARC_6
ENUMX
  BFD_RELOC_SPARC_5
ENUMEQX
  BFD_RELOC_SPARC_DISP64
  BFD_RELOC_64_PCREL
ENUMX
  BFD_RELOC_SPARC_PLT64
ENUMX
  BFD_RELOC_SPARC_HIX22
ENUMX
  BFD_RELOC_SPARC_LOX10
ENUMX
  BFD_RELOC_SPARC_H44
ENUMX
  BFD_RELOC_SPARC_M44
ENUMX
  BFD_RELOC_SPARC_L44
ENUMX
  BFD_RELOC_SPARC_REGISTER
ENUMDOC
  SPARC64 relocations

ENUM
  BFD_RELOC_SPARC_REV32
ENUMDOC
  SPARC little endian relocation

ENUM
  BFD_RELOC_ALPHA_GPDISP_HI16
ENUMDOC
  Alpha ECOFF and ELF relocations.  Some of these treat the symbol or
     "addend" in some special way.
  For GPDISP_HI16 ("gpdisp") relocations, the symbol is ignored when
     writing; when reading, it will be the absolute section symbol.  The
     addend is the displacement in bytes of the "lda" instruction from
     the "ldah" instruction (which is at the address of this reloc).
ENUM
  BFD_RELOC_ALPHA_GPDISP_LO16
ENUMDOC
  For GPDISP_LO16 ("ignore") relocations, the symbol is handled as
     with GPDISP_HI16 relocs.  The addend is ignored when writing the
     relocations out, and is filled in with the file's GP value on
     reading, for convenience.

ENUM
  BFD_RELOC_ALPHA_GPDISP
ENUMDOC
  The ELF GPDISP relocation is exactly the same as the GPDISP_HI16
     relocation except that there is no accompanying GPDISP_LO16
     relocation.

ENUM
  BFD_RELOC_ALPHA_LITERAL
ENUMX
  BFD_RELOC_ALPHA_ELF_LITERAL
ENUMX
  BFD_RELOC_ALPHA_LITUSE
ENUMDOC
  The Alpha LITERAL/LITUSE relocs are produced by a symbol reference;
     the assembler turns it into a LDQ instruction to load the address of
     the symbol, and then fills in a register in the real instruction.

     The LITERAL reloc, at the LDQ instruction, refers to the .lita
     section symbol.  The addend is ignored when writing, but is filled
     in with the file's GP value on reading, for convenience, as with the
     GPDISP_LO16 reloc.

     The ELF_LITERAL reloc is somewhere between 16_GOTOFF and GPDISP_LO16.
     It should refer to the symbol to be referenced, as with 16_GOTOFF,
     but it generates output not based on the position within the .got
     section, but relative to the GP value chosen for the file during the
     final link stage.

     The LITUSE reloc, on the instruction using the loaded address, gives
     information to the linker that it might be able to use to optimize
     away some literal section references.  The symbol is ignored (read
     as the absolute section symbol), and the "addend" indicates the type
     of instruction using the register:
              1 - "memory" fmt insn
              2 - byte-manipulation (byte offset reg)
              3 - jsr (target of branch)

     The GNU linker currently doesn't do any of this optimizing.

ENUM
  BFD_RELOC_ALPHA_USER_LITERAL
ENUMX
  BFD_RELOC_ALPHA_USER_LITUSE_BASE
ENUMX
  BFD_RELOC_ALPHA_USER_LITUSE_BYTOFF
ENUMX
  BFD_RELOC_ALPHA_USER_LITUSE_JSR
ENUMX
  BFD_RELOC_ALPHA_USER_GPDISP
ENUMX
  BFD_RELOC_ALPHA_USER_GPRELHIGH
ENUMX
  BFD_RELOC_ALPHA_USER_GPRELLOW
ENUMDOC
  The BFD_RELOC_ALPHA_USER_* relocations are used by the assembler to
     process the explicit !<reloc>!sequence relocations, and are mapped
     into the normal relocations at the end of processing.

ENUM
  BFD_RELOC_ALPHA_HINT
ENUMDOC
  The HINT relocation indicates a value that should be filled into the
     "hint" field of a jmp/jsr/ret instruction, for possible branch-
     prediction logic which may be provided on some processors.

ENUM
  BFD_RELOC_ALPHA_LINKAGE
ENUMDOC
  The LINKAGE relocation outputs a linkage pair in the object file,
     which is filled by the linker.

ENUM
  BFD_RELOC_ALPHA_CODEADDR
ENUMDOC
  The CODEADDR relocation outputs a STO_CA in the object file,
     which is filled by the linker.

ENUM
  BFD_RELOC_MIPS_JMP
ENUMDOC
  Bits 27..2 of the relocation address shifted right 2 bits;
     simple reloc otherwise.

ENUM
  BFD_RELOC_MIPS16_JMP
ENUMDOC
  The MIPS16 jump instruction.

ENUM
  BFD_RELOC_MIPS16_GPREL
ENUMDOC
  MIPS16 GP relative reloc.

ENUM
  BFD_RELOC_HI16
ENUMDOC
  High 16 bits of 32-bit value; simple reloc.
ENUM
  BFD_RELOC_HI16_S
ENUMDOC
  High 16 bits of 32-bit value but the low 16 bits will be sign
     extended and added to form the final result.  If the low 16
     bits form a negative number, we need to add one to the high value
     to compensate for the borrow when the low bits are added.
ENUM
  BFD_RELOC_LO16
ENUMDOC
  Low 16 bits.
ENUM
  BFD_RELOC_PCREL_HI16_S
ENUMDOC
  Like BFD_RELOC_HI16_S, but PC relative.
ENUM
  BFD_RELOC_PCREL_LO16
ENUMDOC
  Like BFD_RELOC_LO16, but PC relative.

ENUMEQ
  BFD_RELOC_MIPS_GPREL
  BFD_RELOC_GPREL16
ENUMDOC
  Relocation relative to the global pointer.

ENUM
  BFD_RELOC_MIPS_LITERAL
ENUMDOC
  Relocation against a MIPS literal section.

ENUM
  BFD_RELOC_MIPS_GOT16
ENUMX
  BFD_RELOC_MIPS_CALL16
ENUMEQX
  BFD_RELOC_MIPS_GPREL32
  BFD_RELOC_GPREL32
ENUMX
  BFD_RELOC_MIPS_GOT_HI16
ENUMX
  BFD_RELOC_MIPS_GOT_LO16
ENUMX
  BFD_RELOC_MIPS_CALL_HI16
ENUMX
  BFD_RELOC_MIPS_CALL_LO16
ENUMX
  BFD_RELOC_MIPS_SUB
ENUMX
  BFD_RELOC_MIPS_GOT_PAGE
ENUMX
  BFD_RELOC_MIPS_GOT_OFST
ENUMX
  BFD_RELOC_MIPS_GOT_DISP
ENUMX
  BFD_RELOC_MIPS_SHIFT5
ENUMX
  BFD_RELOC_MIPS_SHIFT6
ENUMX
  BFD_RELOC_MIPS_INSERT_A
ENUMX
  BFD_RELOC_MIPS_INSERT_B
ENUMX
  BFD_RELOC_MIPS_DELETE
ENUMX
  BFD_RELOC_MIPS_HIGHEST
ENUMX
  BFD_RELOC_MIPS_HIGHER
ENUMX
  BFD_RELOC_MIPS_SCN_DISP
ENUMX
  BFD_RELOC_MIPS_REL16
ENUMX
  BFD_RELOC_MIPS_RELGOT
ENUMX
  BFD_RELOC_MIPS_JALR
COMMENT
ENUMDOC
  MIPS ELF relocations.

COMMENT

ENUM
  BFD_RELOC_386_GOT32
ENUMX
  BFD_RELOC_386_PLT32
ENUMX
  BFD_RELOC_386_COPY
ENUMX
  BFD_RELOC_386_GLOB_DAT
ENUMX
  BFD_RELOC_386_JUMP_SLOT
ENUMX
  BFD_RELOC_386_RELATIVE
ENUMX
  BFD_RELOC_386_GOTOFF
ENUMX
  BFD_RELOC_386_GOTPC
ENUMDOC
  i386/elf relocations

ENUM
  BFD_RELOC_X86_64_GOT32
ENUMX
  BFD_RELOC_X86_64_PLT32
ENUMX
  BFD_RELOC_X86_64_COPY
ENUMX
  BFD_RELOC_X86_64_GLOB_DAT
ENUMX
  BFD_RELOC_X86_64_JUMP_SLOT
ENUMX
  BFD_RELOC_X86_64_RELATIVE
ENUMX
  BFD_RELOC_X86_64_GOTPCREL
ENUMX
  BFD_RELOC_X86_64_32S
ENUMDOC
  x86-64/elf relocations

ENUM
  BFD_RELOC_NS32K_IMM_8
ENUMX
  BFD_RELOC_NS32K_IMM_16
ENUMX
  BFD_RELOC_NS32K_IMM_32
ENUMX
  BFD_RELOC_NS32K_IMM_8_PCREL
ENUMX
  BFD_RELOC_NS32K_IMM_16_PCREL
ENUMX
  BFD_RELOC_NS32K_IMM_32_PCREL
ENUMX
  BFD_RELOC_NS32K_DISP_8
ENUMX
  BFD_RELOC_NS32K_DISP_16
ENUMX
  BFD_RELOC_NS32K_DISP_32
ENUMX
  BFD_RELOC_NS32K_DISP_8_PCREL
ENUMX
  BFD_RELOC_NS32K_DISP_16_PCREL
ENUMX
  BFD_RELOC_NS32K_DISP_32_PCREL
ENUMDOC
  ns32k relocations

ENUM
  BFD_RELOC_PJ_CODE_HI16
ENUMX
  BFD_RELOC_PJ_CODE_LO16
ENUMX
  BFD_RELOC_PJ_CODE_DIR16
ENUMX
  BFD_RELOC_PJ_CODE_DIR32
ENUMX
  BFD_RELOC_PJ_CODE_REL16
ENUMX
  BFD_RELOC_PJ_CODE_REL32
ENUMDOC
  Picojava relocs.  Not all of these appear in object files.

ENUM
  BFD_RELOC_PPC_B26
ENUMX
  BFD_RELOC_PPC_BA26
ENUMX
  BFD_RELOC_PPC_TOC16
ENUMX
  BFD_RELOC_PPC_B16
ENUMX
  BFD_RELOC_PPC_B16_BRTAKEN
ENUMX
  BFD_RELOC_PPC_B16_BRNTAKEN
ENUMX
  BFD_RELOC_PPC_BA16
ENUMX
  BFD_RELOC_PPC_BA16_BRTAKEN
ENUMX
  BFD_RELOC_PPC_BA16_BRNTAKEN
ENUMX
  BFD_RELOC_PPC_COPY
ENUMX
  BFD_RELOC_PPC_GLOB_DAT
ENUMX
  BFD_RELOC_PPC_JMP_SLOT
ENUMX
  BFD_RELOC_PPC_RELATIVE
ENUMX
  BFD_RELOC_PPC_LOCAL24PC
ENUMX
  BFD_RELOC_PPC_EMB_NADDR32
ENUMX
  BFD_RELOC_PPC_EMB_NADDR16
ENUMX
  BFD_RELOC_PPC_EMB_NADDR16_LO
ENUMX
  BFD_RELOC_PPC_EMB_NADDR16_HI
ENUMX
  BFD_RELOC_PPC_EMB_NADDR16_HA
ENUMX
  BFD_RELOC_PPC_EMB_SDAI16
ENUMX
  BFD_RELOC_PPC_EMB_SDA2I16
ENUMX
  BFD_RELOC_PPC_EMB_SDA2REL
ENUMX
  BFD_RELOC_PPC_EMB_SDA21
ENUMX
  BFD_RELOC_PPC_EMB_MRKREF
ENUMX
  BFD_RELOC_PPC_EMB_RELSEC16
ENUMX
  BFD_RELOC_PPC_EMB_RELST_LO
ENUMX
  BFD_RELOC_PPC_EMB_RELST_HI
ENUMX
  BFD_RELOC_PPC_EMB_RELST_HA
ENUMX
  BFD_RELOC_PPC_EMB_BIT_FLD
ENUMX
  BFD_RELOC_PPC_EMB_RELSDA
ENUMDOC
  Power(rs6000) and PowerPC relocations.

ENUM
  BFD_RELOC_I370_D12
ENUMDOC
  IBM 370/390 relocations

ENUM
  BFD_RELOC_CTOR
ENUMDOC
  The type of reloc used to build a contructor table - at the moment
  probably a 32 bit wide absolute relocation, but the target can choose.
  It generally does map to one of the other relocation types.

ENUM
  BFD_RELOC_ARM_PCREL_BRANCH
ENUMDOC
  ARM 26 bit pc-relative branch.  The lowest two bits must be zero and are
  not stored in the instruction.
ENUM
  BFD_RELOC_ARM_PCREL_BLX
ENUMDOC
  ARM 26 bit pc-relative branch.  The lowest bit must be zero and is
  not stored in the instruction.  The 2nd lowest bit comes from a 1 bit
  field in the instruction.
ENUM
  BFD_RELOC_THUMB_PCREL_BLX
ENUMDOC
  Thumb 22 bit pc-relative branch.  The lowest bit must be zero and is
  not stored in the instruction.  The 2nd lowest bit comes from a 1 bit
  field in the instruction.
ENUM
  BFD_RELOC_ARM_IMMEDIATE
ENUMX
  BFD_RELOC_ARM_ADRL_IMMEDIATE
ENUMX
  BFD_RELOC_ARM_OFFSET_IMM
ENUMX
  BFD_RELOC_ARM_SHIFT_IMM
ENUMX
  BFD_RELOC_ARM_SWI
ENUMX
  BFD_RELOC_ARM_MULTI
ENUMX
  BFD_RELOC_ARM_CP_OFF_IMM
ENUMX
  BFD_RELOC_ARM_ADR_IMM
ENUMX
  BFD_RELOC_ARM_LDR_IMM
ENUMX
  BFD_RELOC_ARM_LITERAL
ENUMX
  BFD_RELOC_ARM_IN_POOL
ENUMX
  BFD_RELOC_ARM_OFFSET_IMM8
ENUMX
  BFD_RELOC_ARM_HWLITERAL
ENUMX
  BFD_RELOC_ARM_THUMB_ADD
ENUMX
  BFD_RELOC_ARM_THUMB_IMM
ENUMX
  BFD_RELOC_ARM_THUMB_SHIFT
ENUMX
  BFD_RELOC_ARM_THUMB_OFFSET
ENUMX
  BFD_RELOC_ARM_GOT12
ENUMX
  BFD_RELOC_ARM_GOT32
ENUMX
  BFD_RELOC_ARM_JUMP_SLOT
ENUMX
  BFD_RELOC_ARM_COPY
ENUMX
  BFD_RELOC_ARM_GLOB_DAT
ENUMX
  BFD_RELOC_ARM_PLT32
ENUMX
  BFD_RELOC_ARM_RELATIVE
ENUMX
  BFD_RELOC_ARM_GOTOFF
ENUMX
  BFD_RELOC_ARM_GOTPC
ENUMDOC
  These relocs are only used within the ARM assembler.  They are not
  (at present) written to any object files.

ENUM
  BFD_RELOC_SH_PCDISP8BY2
ENUMX
  BFD_RELOC_SH_PCDISP12BY2
ENUMX
  BFD_RELOC_SH_IMM4
ENUMX
  BFD_RELOC_SH_IMM4BY2
ENUMX
  BFD_RELOC_SH_IMM4BY4
ENUMX
  BFD_RELOC_SH_IMM8
ENUMX
  BFD_RELOC_SH_IMM8BY2
ENUMX
  BFD_RELOC_SH_IMM8BY4
ENUMX
  BFD_RELOC_SH_PCRELIMM8BY2
ENUMX
  BFD_RELOC_SH_PCRELIMM8BY4
ENUMX
  BFD_RELOC_SH_SWITCH16
ENUMX
  BFD_RELOC_SH_SWITCH32
ENUMX
  BFD_RELOC_SH_USES
ENUMX
  BFD_RELOC_SH_COUNT
ENUMX
  BFD_RELOC_SH_ALIGN
ENUMX
  BFD_RELOC_SH_CODE
ENUMX
  BFD_RELOC_SH_DATA
ENUMX
  BFD_RELOC_SH_LABEL
ENUMX
  BFD_RELOC_SH_LOOP_START
ENUMX
  BFD_RELOC_SH_LOOP_END
ENUMX
  BFD_RELOC_SH_COPY
ENUMX
  BFD_RELOC_SH_GLOB_DAT
ENUMX
  BFD_RELOC_SH_JMP_SLOT
ENUMX
  BFD_RELOC_SH_RELATIVE
ENUMX
  BFD_RELOC_SH_GOTPC
ENUMDOC
  Hitachi SH relocs.  Not all of these appear in object files.

ENUM
  BFD_RELOC_THUMB_PCREL_BRANCH9
ENUMX
  BFD_RELOC_THUMB_PCREL_BRANCH12
ENUMX
  BFD_RELOC_THUMB_PCREL_BRANCH23
ENUMDOC
  Thumb 23-, 12- and 9-bit pc-relative branches.  The lowest bit must
  be zero and is not stored in the instruction.

ENUM
  BFD_RELOC_ARC_B22_PCREL
ENUMDOC
  ARC Cores relocs.
  ARC 22 bit pc-relative branch.  The lowest two bits must be zero and are
  not stored in the instruction.  The high 20 bits are installed in bits 26
  through 7 of the instruction.
ENUM
  BFD_RELOC_ARC_B26
ENUMDOC
  ARC 26 bit absolute branch.  The lowest two bits must be zero and are not
  stored in the instruction.  The high 24 bits are installed in bits 23
  through 0.

ENUM
  BFD_RELOC_D10V_10_PCREL_R
ENUMDOC
  Mitsubishi D10V relocs.
  This is a 10-bit reloc with the right 2 bits
  assumed to be 0.
ENUM
  BFD_RELOC_D10V_10_PCREL_L
ENUMDOC
  Mitsubishi D10V relocs.
  This is a 10-bit reloc with the right 2 bits
  assumed to be 0.  This is the same as the previous reloc
  except it is in the left container, i.e.,
  shifted left 15 bits.
ENUM
  BFD_RELOC_D10V_18
ENUMDOC
  This is an 18-bit reloc with the right 2 bits
  assumed to be 0.
ENUM
  BFD_RELOC_D10V_18_PCREL
ENUMDOC
  This is an 18-bit reloc with the right 2 bits
  assumed to be 0.

ENUM
  BFD_RELOC_D30V_6
ENUMDOC
  Mitsubishi D30V relocs.
  This is a 6-bit absolute reloc.
ENUM
  BFD_RELOC_D30V_9_PCREL
ENUMDOC
  This is a 6-bit pc-relative reloc with
  the right 3 bits assumed to be 0.
ENUM
  BFD_RELOC_D30V_9_PCREL_R
ENUMDOC
  This is a 6-bit pc-relative reloc with
  the right 3 bits assumed to be 0. Same
  as the previous reloc but on the right side
  of the container.
ENUM
  BFD_RELOC_D30V_15
ENUMDOC
  This is a 12-bit absolute reloc with the
  right 3 bitsassumed to be 0.
ENUM
  BFD_RELOC_D30V_15_PCREL
ENUMDOC
  This is a 12-bit pc-relative reloc with
  the right 3 bits assumed to be 0.
ENUM
  BFD_RELOC_D30V_15_PCREL_R
ENUMDOC
  This is a 12-bit pc-relative reloc with
  the right 3 bits assumed to be 0. Same
  as the previous reloc but on the right side
  of the container.
ENUM
  BFD_RELOC_D30V_21
ENUMDOC
  This is an 18-bit absolute reloc with
  the right 3 bits assumed to be 0.
ENUM
  BFD_RELOC_D30V_21_PCREL
ENUMDOC
  This is an 18-bit pc-relative reloc with
  the right 3 bits assumed to be 0.
ENUM
  BFD_RELOC_D30V_21_PCREL_R
ENUMDOC
  This is an 18-bit pc-relative reloc with
  the right 3 bits assumed to be 0. Same
  as the previous reloc but on the right side
  of the container.
ENUM
  BFD_RELOC_D30V_32
ENUMDOC
  This is a 32-bit absolute reloc.
ENUM
  BFD_RELOC_D30V_32_PCREL
ENUMDOC
  This is a 32-bit pc-relative reloc.

ENUM
  BFD_RELOC_M32R_24
ENUMDOC
  Mitsubishi M32R relocs.
  This is a 24 bit absolute address.
ENUM
  BFD_RELOC_M32R_10_PCREL
ENUMDOC
  This is a 10-bit pc-relative reloc with the right 2 bits assumed to be 0.
ENUM
  BFD_RELOC_M32R_18_PCREL
ENUMDOC
  This is an 18-bit reloc with the right 2 bits assumed to be 0.
ENUM
  BFD_RELOC_M32R_26_PCREL
ENUMDOC
  This is a 26-bit reloc with the right 2 bits assumed to be 0.
ENUM
  BFD_RELOC_M32R_HI16_ULO
ENUMDOC
  This is a 16-bit reloc containing the high 16 bits of an address
  used when the lower 16 bits are treated as unsigned.
ENUM
  BFD_RELOC_M32R_HI16_SLO
ENUMDOC
  This is a 16-bit reloc containing the high 16 bits of an address
  used when the lower 16 bits are treated as signed.
ENUM
  BFD_RELOC_M32R_LO16
ENUMDOC
  This is a 16-bit reloc containing the lower 16 bits of an address.
ENUM
  BFD_RELOC_M32R_SDA16
ENUMDOC
  This is a 16-bit reloc containing the small data area offset for use in
  add3, load, and store instructions.

ENUM
  BFD_RELOC_V850_9_PCREL
ENUMDOC
  This is a 9-bit reloc
ENUM
  BFD_RELOC_V850_22_PCREL
ENUMDOC
  This is a 22-bit reloc

ENUM
  BFD_RELOC_V850_SDA_16_16_OFFSET
ENUMDOC
  This is a 16 bit offset from the short data area pointer.
ENUM
  BFD_RELOC_V850_SDA_15_16_OFFSET
ENUMDOC
  This is a 16 bit offset (of which only 15 bits are used) from the
  short data area pointer.
ENUM
  BFD_RELOC_V850_ZDA_16_16_OFFSET
ENUMDOC
  This is a 16 bit offset from the zero data area pointer.
ENUM
  BFD_RELOC_V850_ZDA_15_16_OFFSET
ENUMDOC
  This is a 16 bit offset (of which only 15 bits are used) from the
  zero data area pointer.
ENUM
  BFD_RELOC_V850_TDA_6_8_OFFSET
ENUMDOC
  This is an 8 bit offset (of which only 6 bits are used) from the
  tiny data area pointer.
ENUM
  BFD_RELOC_V850_TDA_7_8_OFFSET
ENUMDOC
  This is an 8bit offset (of which only 7 bits are used) from the tiny
  data area pointer.
ENUM
  BFD_RELOC_V850_TDA_7_7_OFFSET
ENUMDOC
  This is a 7 bit offset from the tiny data area pointer.
ENUM
  BFD_RELOC_V850_TDA_16_16_OFFSET
ENUMDOC
  This is a 16 bit offset from the tiny data area pointer.
COMMENT
ENUM
  BFD_RELOC_V850_TDA_4_5_OFFSET
ENUMDOC
  This is a 5 bit offset (of which only 4 bits are used) from the tiny
  data area pointer.
ENUM
  BFD_RELOC_V850_TDA_4_4_OFFSET
ENUMDOC
  This is a 4 bit offset from the tiny data area pointer.
ENUM
  BFD_RELOC_V850_SDA_16_16_SPLIT_OFFSET
ENUMDOC
  This is a 16 bit offset from the short data area pointer, with the
  bits placed non-contigously in the instruction.
ENUM
  BFD_RELOC_V850_ZDA_16_16_SPLIT_OFFSET
ENUMDOC
  This is a 16 bit offset from the zero data area pointer, with the
  bits placed non-contigously in the instruction.
ENUM
  BFD_RELOC_V850_CALLT_6_7_OFFSET
ENUMDOC
  This is a 6 bit offset from the call table base pointer.
ENUM
  BFD_RELOC_V850_CALLT_16_16_OFFSET
ENUMDOC
  This is a 16 bit offset from the call table base pointer.
COMMENT

ENUM
  BFD_RELOC_MN10300_32_PCREL
ENUMDOC
  This is a 32bit pcrel reloc for the mn10300, offset by two bytes in the
  instruction.
ENUM
  BFD_RELOC_MN10300_16_PCREL
ENUMDOC
  This is a 16bit pcrel reloc for the mn10300, offset by two bytes in the
  instruction.

ENUM
  BFD_RELOC_TIC30_LDP
ENUMDOC
  This is a 8bit DP reloc for the tms320c30, where the most
  significant 8 bits of a 24 bit word are placed into the least
  significant 8 bits of the opcode.

ENUM
  BFD_RELOC_TIC54X_PARTLS7
ENUMDOC
  This is a 7bit reloc for the tms320c54x, where the least
  significant 7 bits of a 16 bit word are placed into the least
  significant 7 bits of the opcode.

ENUM
  BFD_RELOC_TIC54X_PARTMS9
ENUMDOC
  This is a 9bit DP reloc for the tms320c54x, where the most
  significant 9 bits of a 16 bit word are placed into the least
  significant 9 bits of the opcode.

ENUM
  BFD_RELOC_TIC54X_23
ENUMDOC
  This is an extended address 23-bit reloc for the tms320c54x.

ENUM
  BFD_RELOC_TIC54X_16_OF_23
ENUMDOC
  This is a 16-bit reloc for the tms320c54x, where the least
  significant 16 bits of a 23-bit extended address are placed into
  the opcode.

ENUM
  BFD_RELOC_TIC54X_MS7_OF_23
ENUMDOC
  This is a reloc for the tms320c54x, where the most
  significant 7 bits of a 23-bit extended address are placed into
  the opcode.

ENUM
  BFD_RELOC_FR30_48
ENUMDOC
  This is a 48 bit reloc for the FR30 that stores 32 bits.
ENUM
  BFD_RELOC_FR30_20
ENUMDOC
  This is a 32 bit reloc for the FR30 that stores 20 bits split up into
  two sections.
ENUM
  BFD_RELOC_FR30_6_IN_4
ENUMDOC
  This is a 16 bit reloc for the FR30 that stores a 6 bit word offset in
  4 bits.
ENUM
  BFD_RELOC_FR30_8_IN_8
ENUMDOC
  This is a 16 bit reloc for the FR30 that stores an 8 bit byte offset
  into 8 bits.
ENUM
  BFD_RELOC_FR30_9_IN_8
ENUMDOC
  This is a 16 bit reloc for the FR30 that stores a 9 bit short offset
  into 8 bits.
ENUM
  BFD_RELOC_FR30_10_IN_8
ENUMDOC
  This is a 16 bit reloc for the FR30 that stores a 10 bit word offset
  into 8 bits.
ENUM
  BFD_RELOC_FR30_9_PCREL
ENUMDOC
  This is a 16 bit reloc for the FR30 that stores a 9 bit pc relative
  short offset into 8 bits.
ENUM
  BFD_RELOC_FR30_12_PCREL
ENUMDOC
  This is a 16 bit reloc for the FR30 that stores a 12 bit pc relative
  short offset into 11 bits.

ENUM
  BFD_RELOC_MCORE_PCREL_IMM8BY4
ENUMX
  BFD_RELOC_MCORE_PCREL_IMM11BY2
ENUMX
  BFD_RELOC_MCORE_PCREL_IMM4BY2
ENUMX
  BFD_RELOC_MCORE_PCREL_32
ENUMX
  BFD_RELOC_MCORE_PCREL_JSR_IMM11BY2
ENUMX
  BFD_RELOC_MCORE_RVA
ENUMDOC
  Motorola Mcore relocations.

ENUM
  BFD_RELOC_AVR_7_PCREL
ENUMDOC
  This is a 16 bit reloc for the AVR that stores 8 bit pc relative
  short offset into 7 bits.
ENUM
  BFD_RELOC_AVR_13_PCREL
ENUMDOC
  This is a 16 bit reloc for the AVR that stores 13 bit pc relative
  short offset into 12 bits.
ENUM
  BFD_RELOC_AVR_16_PM
ENUMDOC
  This is a 16 bit reloc for the AVR that stores 17 bit value (usually
  program memory address) into 16 bits.
ENUM
  BFD_RELOC_AVR_LO8_LDI
ENUMDOC
  This is a 16 bit reloc for the AVR that stores 8 bit value (usually
  data memory address) into 8 bit immediate value of LDI insn.
ENUM
  BFD_RELOC_AVR_HI8_LDI
ENUMDOC
  This is a 16 bit reloc for the AVR that stores 8 bit value (high 8 bit
  of data memory address) into 8 bit immediate value of LDI insn.
ENUM
  BFD_RELOC_AVR_HH8_LDI
ENUMDOC
  This is a 16 bit reloc for the AVR that stores 8 bit value (most high 8 bit
  of program memory address) into 8 bit immediate value of LDI insn.
ENUM
  BFD_RELOC_AVR_LO8_LDI_NEG
ENUMDOC
  This is a 16 bit reloc for the AVR that stores negated 8 bit value
  (usually data memory address) into 8 bit immediate value of SUBI insn.
ENUM
  BFD_RELOC_AVR_HI8_LDI_NEG
ENUMDOC
  This is a 16 bit reloc for the AVR that stores negated 8 bit value
  (high 8 bit of data memory address) into 8 bit immediate value of
  SUBI insn.
ENUM
  BFD_RELOC_AVR_HH8_LDI_NEG
ENUMDOC
  This is a 16 bit reloc for the AVR that stores negated 8 bit value
  (most high 8 bit of program memory address) into 8 bit immediate value
  of LDI or SUBI insn.
ENUM
  BFD_RELOC_AVR_LO8_LDI_PM
ENUMDOC
  This is a 16 bit reloc for the AVR that stores 8 bit value (usually
  command address) into 8 bit immediate value of LDI insn.
ENUM
  BFD_RELOC_AVR_HI8_LDI_PM
ENUMDOC
  This is a 16 bit reloc for the AVR that stores 8 bit value (high 8 bit
  of command address) into 8 bit immediate value of LDI insn.
ENUM
  BFD_RELOC_AVR_HH8_LDI_PM
ENUMDOC
  This is a 16 bit reloc for the AVR that stores 8 bit value (most high 8 bit
  of command address) into 8 bit immediate value of LDI insn.
ENUM
  BFD_RELOC_AVR_LO8_LDI_PM_NEG
ENUMDOC
  This is a 16 bit reloc for the AVR that stores negated 8 bit value
  (usually command address) into 8 bit immediate value of SUBI insn.
ENUM
  BFD_RELOC_AVR_HI8_LDI_PM_NEG
ENUMDOC
  This is a 16 bit reloc for the AVR that stores negated 8 bit value
  (high 8 bit of 16 bit command address) into 8 bit immediate value
  of SUBI insn.
ENUM
  BFD_RELOC_AVR_HH8_LDI_PM_NEG
ENUMDOC
  This is a 16 bit reloc for the AVR that stores negated 8 bit value
  (high 6 bit of 22 bit command address) into 8 bit immediate
  value of SUBI insn.
ENUM
  BFD_RELOC_AVR_CALL
ENUMDOC
  This is a 32 bit reloc for the AVR that stores 23 bit value
  into 22 bits.

ENUM
  BFD_RELOC_VTABLE_INHERIT
ENUMX
  BFD_RELOC_VTABLE_ENTRY
ENUMDOC
  These two relocations are used by the linker to determine which of
  the entries in a C++ virtual function table are actually used.  When
  the --gc-sections option is given, the linker will zero out the entries
  that are not used, so that the code for those functions need not be
  included in the output.

  VTABLE_INHERIT is a zero-space relocation used to describe to the
  linker the inheritence tree of a C++ virtual function table.  The
  relocation's symbol should be the parent class' vtable, and the
  relocation should be located at the child vtable.

  VTABLE_ENTRY is a zero-space relocation that describes the use of a
  virtual function table entry.  The reloc's symbol should refer to the
  table of the class mentioned in the code.  Off of that base, an offset
  describes the entry that is being used.  For Rela hosts, this offset
  is stored in the reloc's addend.  For Rel hosts, we are forced to put
  this offset in the reloc's section offset.

ENUM
  BFD_RELOC_IA64_IMM14
ENUMX
  BFD_RELOC_IA64_IMM22
ENUMX
  BFD_RELOC_IA64_IMM64
ENUMX
  BFD_RELOC_IA64_DIR32MSB
ENUMX
  BFD_RELOC_IA64_DIR32LSB
ENUMX
  BFD_RELOC_IA64_DIR64MSB
ENUMX
  BFD_RELOC_IA64_DIR64LSB
ENUMX
  BFD_RELOC_IA64_GPREL22
ENUMX
  BFD_RELOC_IA64_GPREL64I
ENUMX
  BFD_RELOC_IA64_GPREL32MSB
ENUMX
  BFD_RELOC_IA64_GPREL32LSB
ENUMX
  BFD_RELOC_IA64_GPREL64MSB
ENUMX
  BFD_RELOC_IA64_GPREL64LSB
ENUMX
  BFD_RELOC_IA64_LTOFF22
ENUMX
  BFD_RELOC_IA64_LTOFF64I
ENUMX
  BFD_RELOC_IA64_PLTOFF22
ENUMX
  BFD_RELOC_IA64_PLTOFF64I
ENUMX
  BFD_RELOC_IA64_PLTOFF64MSB
ENUMX
  BFD_RELOC_IA64_PLTOFF64LSB
ENUMX
  BFD_RELOC_IA64_FPTR64I
ENUMX
  BFD_RELOC_IA64_FPTR32MSB
ENUMX
  BFD_RELOC_IA64_FPTR32LSB
ENUMX
  BFD_RELOC_IA64_FPTR64MSB
ENUMX
  BFD_RELOC_IA64_FPTR64LSB
ENUMX
  BFD_RELOC_IA64_PCREL21B
ENUMX
  BFD_RELOC_IA64_PCREL21BI
ENUMX
  BFD_RELOC_IA64_PCREL21M
ENUMX
  BFD_RELOC_IA64_PCREL21F
ENUMX
  BFD_RELOC_IA64_PCREL22
ENUMX
  BFD_RELOC_IA64_PCREL60B
ENUMX
  BFD_RELOC_IA64_PCREL64I
ENUMX
  BFD_RELOC_IA64_PCREL32MSB
ENUMX
  BFD_RELOC_IA64_PCREL32LSB
ENUMX
  BFD_RELOC_IA64_PCREL64MSB
ENUMX
  BFD_RELOC_IA64_PCREL64LSB
ENUMX
  BFD_RELOC_IA64_LTOFF_FPTR22
ENUMX
  BFD_RELOC_IA64_LTOFF_FPTR64I
ENUMX
  BFD_RELOC_IA64_LTOFF_FPTR64MSB
ENUMX
  BFD_RELOC_IA64_LTOFF_FPTR64LSB
ENUMX
  BFD_RELOC_IA64_SEGREL32MSB
ENUMX
  BFD_RELOC_IA64_SEGREL32LSB
ENUMX
  BFD_RELOC_IA64_SEGREL64MSB
ENUMX
  BFD_RELOC_IA64_SEGREL64LSB
ENUMX
  BFD_RELOC_IA64_SECREL32MSB
ENUMX
  BFD_RELOC_IA64_SECREL32LSB
ENUMX
  BFD_RELOC_IA64_SECREL64MSB
ENUMX
  BFD_RELOC_IA64_SECREL64LSB
ENUMX
  BFD_RELOC_IA64_REL32MSB
ENUMX
  BFD_RELOC_IA64_REL32LSB
ENUMX
  BFD_RELOC_IA64_REL64MSB
ENUMX
  BFD_RELOC_IA64_REL64LSB
ENUMX
  BFD_RELOC_IA64_LTV32MSB
ENUMX
  BFD_RELOC_IA64_LTV32LSB
ENUMX
  BFD_RELOC_IA64_LTV64MSB
ENUMX
  BFD_RELOC_IA64_LTV64LSB
ENUMX
  BFD_RELOC_IA64_IPLTMSB
ENUMX
  BFD_RELOC_IA64_IPLTLSB
ENUMX
  BFD_RELOC_IA64_COPY
ENUMX
  BFD_RELOC_IA64_TPREL22
ENUMX
  BFD_RELOC_IA64_TPREL64MSB
ENUMX
  BFD_RELOC_IA64_TPREL64LSB
ENUMX
  BFD_RELOC_IA64_LTOFF_TP22
ENUMX
  BFD_RELOC_IA64_LTOFF22X
ENUMX
  BFD_RELOC_IA64_LDXMOV
ENUMDOC
  Intel IA64 Relocations.

ENUM
  BFD_RELOC_M68HC11_HI8
ENUMDOC
  Motorola 68HC11 reloc.
  This is the 8 bits high part of an absolute address.
ENUM
  BFD_RELOC_M68HC11_LO8
ENUMDOC
  Motorola 68HC11 reloc.
  This is the 8 bits low part of an absolute address.
ENUM
  BFD_RELOC_M68HC11_3B
ENUMDOC
  Motorola 68HC11 reloc.
  This is the 3 bits of a value.

ENUM
  BFD_RELOC_CRIS_BDISP8
ENUMX
  BFD_RELOC_CRIS_UNSIGNED_5
ENUMX
  BFD_RELOC_CRIS_SIGNED_6
ENUMX
  BFD_RELOC_CRIS_UNSIGNED_6
ENUMX
  BFD_RELOC_CRIS_UNSIGNED_4
ENUMDOC
  These relocs are only used within the CRIS assembler.  They are not
  (at present) written to any object files.

ENUM
  BFD_RELOC_860_COPY
ENUMX
  BFD_RELOC_860_GLOB_DAT
ENUMX
  BFD_RELOC_860_JUMP_SLOT
ENUMX
  BFD_RELOC_860_RELATIVE
ENUMX
  BFD_RELOC_860_PC26
ENUMX
  BFD_RELOC_860_PLT26
ENUMX
  BFD_RELOC_860_PC16
ENUMX
  BFD_RELOC_860_LOW0
ENUMX
  BFD_RELOC_860_SPLIT0
ENUMX
  BFD_RELOC_860_LOW1
ENUMX
  BFD_RELOC_860_SPLIT1
ENUMX
  BFD_RELOC_860_LOW2
ENUMX
  BFD_RELOC_860_SPLIT2
ENUMX
  BFD_RELOC_860_LOW3
ENUMX
  BFD_RELOC_860_LOGOT0
ENUMX
  BFD_RELOC_860_SPGOT0
ENUMX
  BFD_RELOC_860_LOGOT1
ENUMX
  BFD_RELOC_860_SPGOT1
ENUMX
  BFD_RELOC_860_LOGOTOFF0
ENUMX
  BFD_RELOC_860_SPGOTOFF0
ENUMX
  BFD_RELOC_860_LOGOTOFF1
ENUMX
  BFD_RELOC_860_SPGOTOFF1
ENUMX
  BFD_RELOC_860_LOGOTOFF2
ENUMX
  BFD_RELOC_860_LOGOTOFF3
ENUMX
  BFD_RELOC_860_LOPC
ENUMX
  BFD_RELOC_860_HIGHADJ
ENUMX
  BFD_RELOC_860_HAGOT
ENUMX
  BFD_RELOC_860_HAGOTOFF
ENUMX
  BFD_RELOC_860_HAPC
ENUMX
  BFD_RELOC_860_HIGH
ENUMX
  BFD_RELOC_860_HIGOT
ENUMX
  BFD_RELOC_860_HIGOTOFF
ENUMDOC
  Intel i860 Relocations.

ENDSENUM
  BFD_RELOC_UNUSED
CODE_FRAGMENT
.
.typedef enum bfd_reloc_code_real bfd_reloc_code_real_type;
*/

/*
FUNCTION
	bfd_reloc_type_lookup

SYNOPSIS
	reloc_howto_type *
	bfd_reloc_type_lookup (bfd *abfd, bfd_reloc_code_real_type code);

DESCRIPTION
	Return a pointer to a howto structure which, when
	invoked, will perform the relocation @var{code} on data from the
	architecture noted.

*/

reloc_howto_type *
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
	reloc_howto_type *bfd_default_reloc_type_lookup
	(bfd *abfd, bfd_reloc_code_real_type  code);

DESCRIPTION
	Provides a default relocation lookup routine for any architecture.

*/

reloc_howto_type *
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
  return (reloc_howto_type *) NULL;
}

/*
FUNCTION
	bfd_get_reloc_code_name

SYNOPSIS
	const char *bfd_get_reloc_code_name (bfd_reloc_code_real_type code);

DESCRIPTION
	Provides a printable name for the supplied relocation code.
	Useful mainly for printing error messages.
*/

const char *
bfd_get_reloc_code_name (code)
     bfd_reloc_code_real_type code;
{
  if (code > BFD_RELOC_UNUSED)
    return 0;
  return bfd_reloc_code_real_names[(int)code];
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
     bfd *abfd ATTRIBUTE_UNUSED;
     asection *section ATTRIBUTE_UNUSED;
     struct bfd_link_info *link_info ATTRIBUTE_UNUSED;
     boolean *again;
{
  *again = false;
  return true;
}

/*
INTERNAL_FUNCTION
	bfd_generic_gc_sections

SYNOPSIS
	boolean bfd_generic_gc_sections
	 (bfd *, struct bfd_link_info *);

DESCRIPTION
	Provides default handling for relaxing for back ends which
	don't do section gc -- i.e., does nothing.
*/

/*ARGSUSED*/
boolean
bfd_generic_gc_sections (abfd, link_info)
     bfd *abfd ATTRIBUTE_UNUSED;
     struct bfd_link_info *link_info ATTRIBUTE_UNUSED;
{
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

  reloc_vector = (arelent **) bfd_malloc ((size_t) reloc_size);
  if (reloc_vector == NULL && reloc_size != 0)
    goto error_return;

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
			 input_bfd, input_section, (*parent)->address,
			 true)))
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
