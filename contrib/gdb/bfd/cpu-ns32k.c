/* BFD support for the ns32k architecture.
   Copyright (C) 1990, 1991, 1994, 1995 Free Software Foundation, Inc.
   Almost totally rewritten by Ian Dall from initial work
   by Andrew Cagney.

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

#include "bfd.h"
#include "sysdep.h"
#include "libbfd.h"

long ns32k_get_displacement PARAMS ((bfd_byte *buffer, long offset, long size));
int ns32k_put_displacement PARAMS ((long value, bfd_byte *buffer, long offset, long size));
long ns32k_get_immediate PARAMS ((bfd_byte *buffer, long offset, long size));
int ns32k_put_immediate  PARAMS ((long value, bfd_byte *buffer, long offset, long size));
bfd_reloc_status_type
  ns32k_reloc_disp PARAMS ((bfd *abfd, arelent *reloc_entry,
		   struct symbol_cache_entry *symbol,
		   PTR data,
		   asection *input_section,
		   bfd *output_bfd,
		   char **error_message));
bfd_reloc_status_type
  ns32k_reloc_imm  PARAMS ((bfd *abfd,
		   arelent *reloc_entry,
		   struct symbol_cache_entry *symbol,
		   PTR data,
		   asection *input_section,
		   bfd *output_bfd,
		   char **error_message));
bfd_reloc_status_type ns32k_final_link_relocate  PARAMS ((reloc_howto_type *howto,
			     bfd *input_bfd,
			     asection *input_section,
			     bfd_byte *contents,
			     bfd_vma address,
			     bfd_vma value,
			     bfd_vma addend ));
bfd_reloc_status_type ns32k_relocate_contents  PARAMS ((reloc_howto_type *howto,
							bfd *input_bfd,
							bfd_vma relocation,
							bfd_byte *location));

int bfd_default_scan_num_mach();

#define N(machine, printable, d, next)  \
{  32, 32, 8, bfd_arch_ns32k, machine, "ns32k",printable,3,d,bfd_default_compatible,bfd_default_scan, next, }

static const bfd_arch_info_type arch_info_struct[] =
{ 
  N(32532,"ns32k:32532",true, 0), /* the word ns32k will match this too */
};

const bfd_arch_info_type bfd_ns32k_arch =
  N(32032,"ns32k:32032",false, &arch_info_struct[0]);

static long
ns32k_sign_extend(value, bits)
     int value;
     int bits;
{
  value = value & ((1 << bits) - 1);
  return (value & (1 << (bits-1))
	  ? value | (~((1 << bits) - 1))
	  : value);
}

long
ns32k_get_displacement(buffer, offset, size)
     bfd_byte *buffer;
     long offset;
     long size;
{
  long value;
  buffer += offset;
  switch (size)
    {
    case 1:
      value = ns32k_sign_extend (*buffer, 7);
      break;
    case 2:
      value = ns32k_sign_extend(*buffer++, 6);
      value = (value << 8) | (0xff & *buffer);
      break;
    case 4:
      value = ns32k_sign_extend(*buffer++, 6);
      value = (value << 8) | (0xff & *buffer++);
      value = (value << 8) | (0xff & *buffer++);
      value = (value << 8) | (0xff & *buffer);
      break;
    }
  return value;
}

int
ns32k_put_displacement(value, buffer, offset, size)
     long value;
     bfd_byte *buffer;
     long offset;
     long size;
{
  buffer += offset;
  switch (size)
    {
    case 1:
      if (value < -64 || value > 63)
	return -1;
      value&=0x7f;
      *buffer++=value;
      break;
    case 2:
      if (value < -8192 || value > 8191)
	return -1;
      value&=0x3fff;
      value|=0x8000;
      *buffer++=(value>>8);
      *buffer++=value;
      break;
    case 4:
      if (value < -0x1f000000 || value >= 0x20000000)
	return -1;
      value|=0xc0000000;
      *buffer++=(value>>24);
      *buffer++=(value>>16);
      *buffer++=(value>>8);
      *buffer++=value;
      break;
    default:
      return -1;
  }
  return 0;
}

long
ns32k_get_immediate(buffer, offset, size)
     bfd_byte *buffer;
     long offset;
     long size;
{
  long value = 0;
  buffer += offset;
  switch (size)
    {
    case 4:
      value = (value << 8) | (*buffer++ & 0xff);
    case 3:
      value = (value << 8) | (*buffer++ & 0xff);
    case 2:
      value = (value << 8) | (*buffer++ & 0xff);
    case 1:
      value = (value << 8) | (*buffer++ & 0xff);
    }
  return value;
}

int
ns32k_put_immediate (value, buffer, offset, size)
     long value;
     bfd_byte *buffer;
     long offset;
     long size;
{
  buffer += offset + size - 1;
  switch (size)
    {
    case 4:
      *buffer-- = (value & 0xff); value >>= 8;
    case 3:
      *buffer-- = (value & 0xff); value >>= 8;
    case 2:
      *buffer-- = (value & 0xff); value >>= 8;
    case 1:
      *buffer-- = (value & 0xff); value >>= 8;
    }
  return 0;
}

/* This is just like the standard perform_relocation except we
 * use get_data and put_data which know about the ns32k
 * storage methods.
 * This is probably a lot more complicated than it needs to be!
 */
static bfd_reloc_status_type
do_ns32k_reloc (abfd, reloc_entry, symbol, data, input_section, output_bfd,
		error_message, get_data, put_data)
     bfd *abfd;
     arelent *reloc_entry;
     struct symbol_cache_entry *symbol;
     PTR data;
     asection *input_section;
     bfd *output_bfd;
     char **error_message;
     long (*get_data)();
     int (*put_data)();
{
  int overflow = 0;
  bfd_vma relocation;
  bfd_reloc_status_type flag = bfd_reloc_ok;
  bfd_size_type addr = reloc_entry->address;
  bfd_vma output_base = 0;
  reloc_howto_type *howto = reloc_entry->howto;
  asection *reloc_target_output_section;

  if ((symbol->section == &bfd_abs_section)
      && output_bfd != (bfd *) NULL)
    {
      reloc_entry->address += input_section->output_offset;
      return bfd_reloc_ok;
    }

  /* If we are not producing relocateable output, return an error if
     the symbol is not defined.  An undefined weak symbol is
     considered to have a value of zero (SVR4 ABI, p. 4-27).  */
  if (symbol->section == &bfd_und_section
      && (symbol->flags & BSF_WEAK) == 0
      && output_bfd == (bfd *) NULL)
    flag = bfd_reloc_undefined;


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
	      && strcmp (abfd->xvec->name, "aixcoff-rs6000") != 0)
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
	char x = get_data (data, addr, 1);
	DOIT (x);
	overflow = put_data(x, data, addr, 1);
      }
      break;

    case 1:
      if (relocation)
	{
	  short x = get_data (data, addr, 2);
	  DOIT (x);
	  overflow = put_data(x, (unsigned char *) data, addr, 2);
	}
      break;
    case 2:
      if (relocation)
	{
	  long x = get_data (data, addr, 4);
	  DOIT (x);
	  overflow = put_data(x, data, addr, 4);
	}
      break;
    case -2:
      {
	long  x = get_data(data, addr, 4);
	relocation = -relocation;
	DOIT(x);
	overflow = put_data(x, data , addr, 4);
      }
      break;

    case 3:
      /* Do nothing */
      break;

    case 4:
#ifdef BFD64
      if (relocation)
	{
	  bfd_vma x = get_data (data, addr, 8);
	  DOIT (x);
	  overflow = put_data(x, data, addr, 8);
	}
#else
      abort ();
#endif
      break;
    default:
      return bfd_reloc_other;
    }
  if ((howto->complain_on_overflow != complain_overflow_dont) && overflow)
    return bfd_reloc_overflow;

  return flag;
}

/* Relocate a given location using a given value and howto.  */

bfd_reloc_status_type
do_ns32k_reloc_contents ( howto, input_bfd, relocation, location, get_data,
			 put_data)
     reloc_howto_type *howto;
     bfd *input_bfd;
     bfd_vma relocation;
     bfd_byte *location;
     long (*get_data)();
     int (*put_data)();
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
    case 2:
    case 4:
#ifdef BFD64
    case 8:
#endif
      x = get_data (location, 0, size);
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
    case 2:
    case 4:
#ifdef BFD64
    case 8:
#endif
      put_data(x, location, 0, size);
      break;
    }

  return overflow ? bfd_reloc_overflow : bfd_reloc_ok;
}

bfd_reloc_status_type
ns32k_reloc_disp(abfd, reloc_entry, symbol, data, input_section, output_bfd,
		 error_message)
     bfd *abfd;
     arelent *reloc_entry;
     struct symbol_cache_entry *symbol;
     PTR data;
     asection *input_section;
     bfd *output_bfd;
     char **error_message;
{
  return do_ns32k_reloc(abfd, reloc_entry, symbol, data, input_section, output_bfd, error_message, ns32k_get_displacement, ns32k_put_displacement);
}

bfd_reloc_status_type
ns32k_reloc_imm (abfd, reloc_entry, symbol, data, input_section, output_bfd, error_message)
     bfd *abfd;
     arelent *reloc_entry;
     struct symbol_cache_entry *symbol;
     PTR data;
     asection *input_section;
     bfd *output_bfd;
     char **error_message;
{
  return do_ns32k_reloc(abfd, reloc_entry, symbol, data, input_section, output_bfd, error_message, ns32k_get_immediate, ns32k_put_immediate);
}

bfd_reloc_status_type
ns32k_final_link_relocate (howto, input_bfd, input_section, contents, address, value, addend )
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

  return ns32k_relocate_contents (howto, input_bfd, relocation,
				  contents + address);
}
