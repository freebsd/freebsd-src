/* BFD back-end for ieee-695 objects.
   Copyright (C) 1990, 91, 92, 93, 94, 95, 1996 Free Software Foundation, Inc.
   Written by Steve Chamberlain of Cygnus Support.

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

#define KEEPMINUSPCININST 0

/* IEEE 695 format is a stream of records, which we parse using a simple one-
   token (which is one byte in this lexicon) lookahead recursive decent
   parser.  */

#include "bfd.h"
#include "sysdep.h"
#include "libbfd.h"
#include "ieee.h"
#include "libieee.h"

static boolean ieee_write_byte PARAMS ((bfd *, bfd_byte));
static boolean ieee_write_2bytes PARAMS ((bfd *, int));
static boolean ieee_write_int PARAMS ((bfd *, bfd_vma));
static boolean ieee_write_id PARAMS ((bfd *, const char *));
static boolean ieee_write_expression
  PARAMS ((bfd *, bfd_vma, asymbol *, boolean, unsigned int));
static void ieee_write_int5 PARAMS ((bfd_byte *, bfd_vma));
static boolean ieee_write_int5_out PARAMS ((bfd *, bfd_vma));
static boolean ieee_write_section_part PARAMS ((bfd *));
static boolean do_with_relocs PARAMS ((bfd *, asection *));
static boolean do_as_repeat PARAMS ((bfd *, asection *));
static boolean do_without_relocs PARAMS ((bfd *, asection *));
static boolean ieee_write_external_part PARAMS ((bfd *));
static boolean ieee_write_data_part PARAMS ((bfd *));
static boolean ieee_write_debug_part PARAMS ((bfd *));
static boolean ieee_write_me_part PARAMS ((bfd *));
static boolean ieee_write_processor PARAMS ((bfd *));

static boolean ieee_slurp_debug PARAMS ((bfd *));
static boolean ieee_slurp_section_data PARAMS ((bfd *));

/* Functions for writing to ieee files in the strange way that the
   standard requires. */

static boolean
ieee_write_byte (abfd, byte)
     bfd *abfd;
     bfd_byte byte;
{
  if (bfd_write ((PTR) &byte, 1, 1, abfd) != 1)
    return false;
  return true;
}

static boolean
ieee_write_2bytes (abfd, bytes)
     bfd *abfd;
     int bytes;
{
  bfd_byte buffer[2];

  buffer[0] = bytes >> 8;
  buffer[1] = bytes & 0xff;
  if (bfd_write ((PTR) buffer, 1, 2, abfd) != 2)
    return false;
  return true;
}

static boolean
ieee_write_int (abfd, value)
     bfd *abfd;
     bfd_vma value;
{
  if (value <= 127)
    {
      if (! ieee_write_byte (abfd, (bfd_byte) value))
	return false;
    }
  else
    {
      unsigned int length;

      /* How many significant bytes ? */
      /* FIXME FOR LONGER INTS */
      if (value & 0xff000000)
	length = 4;
      else if (value & 0x00ff0000)
	length = 3;
      else if (value & 0x0000ff00)
	length = 2;
      else
	length = 1;

      if (! ieee_write_byte (abfd,
			     (bfd_byte) ((int) ieee_number_repeat_start_enum
					 + length)))
	return false;
      switch (length)
	{
	case 4:
	  if (! ieee_write_byte (abfd, (bfd_byte) (value >> 24)))
	    return false;
	  /* Fall through.  */
	case 3:
	  if (! ieee_write_byte (abfd, (bfd_byte) (value >> 16)))
	    return false;
	  /* Fall through.  */
	case 2:
	  if (! ieee_write_byte (abfd, (bfd_byte) (value >> 8)))
	    return false;
	  /* Fall through.  */
	case 1:
	  if (! ieee_write_byte (abfd, (bfd_byte) (value)))
	    return false;
	}
    }

  return true;
}

static boolean
ieee_write_id (abfd, id)
     bfd *abfd;
     const char *id;
{
  size_t length = strlen (id);

  if (length <= 127)
    {
      if (! ieee_write_byte (abfd, (bfd_byte) length))
	return false;
    }
  else if (length < 255)
    {
      if (! ieee_write_byte (abfd, ieee_extension_length_1_enum)
	  || ! ieee_write_byte (abfd, (bfd_byte) length))
	return false;
    }
  else if (length < 65535)
    {
      if (! ieee_write_byte (abfd, ieee_extension_length_2_enum)
	  || ! ieee_write_2bytes (abfd, (int) length))
	return false;
    }
  else
    {
      (*_bfd_error_handler)
	("%s: string too long (%d chars, max 65535)",
	 bfd_get_filename (abfd), length);
      bfd_set_error (bfd_error_invalid_operation);
      return false;
    }

  if (bfd_write ((PTR) id, 1, length, abfd) != length)
    return false;
  return true;
}

/***************************************************************************
Functions for reading from ieee files in the strange way that the
standard requires:
*/

#define this_byte(ieee) *((ieee)->input_p)
#define next_byte(ieee) ((ieee)->input_p++)
#define this_byte_and_next(ieee) (*((ieee)->input_p++))

static unsigned short
read_2bytes (ieee)
     common_header_type *ieee;
{
  unsigned char c1 = this_byte_and_next (ieee);
  unsigned char c2 = this_byte_and_next (ieee);
  return (c1 << 8) | c2;
}

static void
bfd_get_string (ieee, string, length)
     common_header_type *ieee;
     char *string;
     size_t length;
{
  size_t i;
  for (i = 0; i < length; i++)
    {
      string[i] = this_byte_and_next (ieee);
    }
}

static char *
read_id (ieee)
     common_header_type *ieee;
{
  size_t length;
  char *string;
  length = this_byte_and_next (ieee);
  if (length <= 0x7f)
    {
      /* Simple string of length 0 to 127 */
    }
  else if (length == 0xde)
    {
      /* Length is next byte, allowing 0..255 */
      length = this_byte_and_next (ieee);
    }
  else if (length == 0xdf)
    {
      /* Length is next two bytes, allowing 0..65535 */
      length = this_byte_and_next (ieee);
      length = (length * 256) + this_byte_and_next (ieee);
    }
  /* Buy memory and read string */
  string = bfd_alloc (ieee->abfd, length + 1);
  if (!string)
    return NULL;
  bfd_get_string (ieee, string, length);
  string[length] = 0;
  return string;
}

static boolean
ieee_write_expression (abfd, value, symbol, pcrel, index)
     bfd *abfd;
     bfd_vma value;
     asymbol *symbol;
     boolean pcrel;
     unsigned int index;
{
  unsigned int term_count = 0;

  if (value != 0)
    {
      if (! ieee_write_int (abfd, value))
	return false;
      term_count++;
    }

  if (bfd_is_com_section (symbol->section)
      || bfd_is_und_section (symbol->section))
    {
      /* Def of a common symbol */
      if (! ieee_write_byte (abfd, ieee_variable_X_enum)
	  || ! ieee_write_int (abfd, symbol->value))
	return false;
      term_count++;
    }
  else if (! bfd_is_abs_section (symbol->section))
    {
      /* Ref to defined symbol - */

      if (symbol->flags & BSF_GLOBAL)
	{
	  if (! ieee_write_byte (abfd, ieee_variable_I_enum)
	      || ! ieee_write_int (abfd, symbol->value))
	    return false;
	  term_count++;
	}
      else if (symbol->flags & (BSF_LOCAL | BSF_SECTION_SYM))
	{
	  /* This is a reference to a defined local symbol.  We can
	     easily do a local as a section+offset.  */
	  if (! ieee_write_byte (abfd, ieee_variable_R_enum)
	      || ! ieee_write_byte (abfd,
				    (bfd_byte) (symbol->section->index
						+ IEEE_SECTION_NUMBER_BASE)))
	    return false;
	  term_count++;
	  if (symbol->value != 0)
	    {
	      if (! ieee_write_int (abfd, symbol->value))
		return false;
	      term_count++;
	    }
	}
      else
	{
	  (*_bfd_error_handler)
	    ("%s: unrecognized symbol `%s' flags 0x%x",
	     bfd_get_filename (abfd), bfd_asymbol_name (symbol),
	     symbol->flags);
	  bfd_set_error (bfd_error_invalid_operation);
	  return false;
	}
    }

  if (pcrel)
    {
      /* subtract the pc from here by asking for PC of this section*/
      if (! ieee_write_byte (abfd, ieee_variable_P_enum)
	  || ! ieee_write_byte (abfd,
				(bfd_byte) (index + IEEE_SECTION_NUMBER_BASE))
	  || ! ieee_write_byte (abfd, ieee_function_minus_enum))
	return false;
    }

  /* Handle the degenerate case of a 0 address.  */
  if (term_count == 0)
    {
      if (! ieee_write_int (abfd, 0))
	return false;
    }

  while (term_count > 1)
    {
      if (! ieee_write_byte (abfd, ieee_function_plus_enum))
	return false;
      term_count--;
    }

  return true;
}

/*****************************************************************************/

/*
writes any integer into the buffer supplied and always takes 5 bytes
*/
static void
ieee_write_int5 (buffer, value)
     bfd_byte *buffer;
     bfd_vma value;
{
  buffer[0] = (bfd_byte) ieee_number_repeat_4_enum;
  buffer[1] = (value >> 24) & 0xff;
  buffer[2] = (value >> 16) & 0xff;
  buffer[3] = (value >> 8) & 0xff;
  buffer[4] = (value >> 0) & 0xff;
}

static boolean
ieee_write_int5_out (abfd, value)
     bfd *abfd;
     bfd_vma value;
{
  bfd_byte b[5];

  ieee_write_int5 (b, value);
  if (bfd_write ((PTR) b, 1, 5, abfd) != 5)
    return false;
  return true;
}

static boolean
parse_int (ieee, value_ptr)
     common_header_type *ieee;
     bfd_vma *value_ptr;
{
  int value = this_byte (ieee);
  int result;
  if (value >= 0 && value <= 127)
    {
      *value_ptr = value;
      next_byte (ieee);
      return true;
    }
  else if (value >= 0x80 && value <= 0x88)
    {
      unsigned int count = value & 0xf;
      result = 0;
      next_byte (ieee);
      while (count)
	{
	  result = (result << 8) | this_byte_and_next (ieee);
	  count--;
	}
      *value_ptr = result;
      return true;
    }
  return false;
}

static int
parse_i (ieee, ok)
     common_header_type *ieee;
     boolean *ok;
{
  bfd_vma x;
  *ok = parse_int (ieee, &x);
  return x;
}

static bfd_vma
must_parse_int (ieee)
     common_header_type *ieee;
{
  bfd_vma result;
  BFD_ASSERT (parse_int (ieee, &result) == true);
  return result;
}

typedef struct
{
  bfd_vma value;
  asection *section;
  ieee_symbol_index_type symbol;
} ieee_value_type;


#ifdef KEEPMINUSPCININST

#define SRC_MASK(arg) arg
#define PCREL_OFFSET false

#else

#define SRC_MASK(arg) 0
#define PCREL_OFFSET true

#endif

static reloc_howto_type abs32_howto =
  HOWTO (1,
	 0,
	 2,
	 32,
	 false,
	 0,
	 complain_overflow_bitfield,
	 0,
	 "abs32",
	 true,
	 0xffffffff,
	 0xffffffff,
	 false);

static reloc_howto_type abs16_howto =
  HOWTO (1,
	 0,
	 1,
	 16,
	 false,
	 0,
	 complain_overflow_bitfield,
	 0,
	 "abs16",
	 true,
	 0x0000ffff,
	 0x0000ffff,
	 false);

static reloc_howto_type abs8_howto =
  HOWTO (1,
	 0,
	 0,
	 8,
	 false,
	 0,
	 complain_overflow_bitfield,
	 0,
	 "abs8",
	 true,
	 0x000000ff,
	 0x000000ff,
	 false);

static reloc_howto_type rel32_howto =
  HOWTO (1,
	 0,
	 2,
	 32,
	 true,
	 0,
	 complain_overflow_signed,
	 0,
	 "rel32",
	 true,
	 SRC_MASK (0xffffffff),
	 0xffffffff,
	 PCREL_OFFSET);

static reloc_howto_type rel16_howto =
  HOWTO (1,
	 0,
	 1,
	 16,
	 true,
	 0,
	 complain_overflow_signed,
	 0,
	 "rel16",
	 true,
	 SRC_MASK (0x0000ffff),
	 0x0000ffff,
	 PCREL_OFFSET);

static reloc_howto_type rel8_howto =
  HOWTO (1,
	 0,
	 0,
	 8,
	 true,
	 0,
	 complain_overflow_signed,
	 0,
	 "rel8",
	 true,
	 SRC_MASK (0x000000ff),
	 0x000000ff,
	 PCREL_OFFSET);

static ieee_symbol_index_type NOSYMBOL = {0, 0};

static void
parse_expression (ieee, value, symbol, pcrel, extra, section)
     ieee_data_type *ieee;
     bfd_vma *value;
     ieee_symbol_index_type *symbol;
     boolean *pcrel;
     unsigned int *extra;
     asection **section;

{
#define POS sp[1]
#define TOS sp[0]
#define NOS sp[-1]
#define INC sp++;
#define DEC sp--;

  boolean loop = true;
  ieee_value_type stack[10];

  /* The stack pointer always points to the next unused location */
#define PUSH(x,y,z) TOS.symbol=x;TOS.section=y;TOS.value=z;INC;
#define POP(x,y,z) DEC;x=TOS.symbol;y=TOS.section;z=TOS.value;
  ieee_value_type *sp = stack;

  while (loop)
    {
      switch (this_byte (&(ieee->h)))
	{
	case ieee_variable_P_enum:
	  /* P variable, current program counter for section n */
	  {
	    int section_n;
	    next_byte (&(ieee->h));
	    *pcrel = true;
	    section_n = must_parse_int (&(ieee->h));
	    PUSH (NOSYMBOL, bfd_abs_section_ptr, 0);
	    break;
	  }
	case ieee_variable_L_enum:
	  /* L variable  address of section N */
	  next_byte (&(ieee->h));
	  PUSH (NOSYMBOL, ieee->section_table[must_parse_int (&(ieee->h))], 0);
	  break;
	case ieee_variable_R_enum:
	  /* R variable, logical address of section module */
	  /* FIXME, this should be different to L */
	  next_byte (&(ieee->h));
	  PUSH (NOSYMBOL, ieee->section_table[must_parse_int (&(ieee->h))], 0);
	  break;
	case ieee_variable_S_enum:
	  /* S variable, size in MAUS of section module */
	  next_byte (&(ieee->h));
	  PUSH (NOSYMBOL,
		0,
		ieee->section_table[must_parse_int (&(ieee->h))]->_raw_size);
	  break;
	case ieee_variable_I_enum:
	  /* Push the address of variable n */
	  {
	    ieee_symbol_index_type sy;
	    next_byte (&(ieee->h));
	    sy.index = (int) must_parse_int (&(ieee->h));
	    sy.letter = 'I';

	    PUSH (sy, bfd_abs_section_ptr, 0);
	  }
	  break;
	case ieee_variable_X_enum:
	  /* Push the address of external variable n */
	  {
	    ieee_symbol_index_type sy;
	    next_byte (&(ieee->h));
	    sy.index = (int) (must_parse_int (&(ieee->h)));
	    sy.letter = 'X';

	    PUSH (sy, bfd_und_section_ptr, 0);
	  }
	  break;
	case ieee_function_minus_enum:
	  {
	    bfd_vma value1, value2;
	    asection *section1, *section_dummy;
	    ieee_symbol_index_type sy;
	    next_byte (&(ieee->h));

	    POP (sy, section1, value1);
	    POP (sy, section_dummy, value2);
	    PUSH (sy, section1 ? section1 : section_dummy, value2 - value1);
	  }
	  break;
	case ieee_function_plus_enum:
	  {
	    bfd_vma value1, value2;
	    asection *section1;
	    asection *section2;
	    ieee_symbol_index_type sy1;
	    ieee_symbol_index_type sy2;
	    next_byte (&(ieee->h));

	    POP (sy1, section1, value1);
	    POP (sy2, section2, value2);
	    PUSH (sy1.letter ? sy1 : sy2,
		  bfd_is_abs_section (section1) ? section2 : section1,
		  value1 + value2);
	  }
	  break;
	default:
	  {
	    bfd_vma va;
	    BFD_ASSERT (this_byte (&(ieee->h)) < (int) ieee_variable_A_enum
		    || this_byte (&(ieee->h)) > (int) ieee_variable_Z_enum);
	    if (parse_int (&(ieee->h), &va))
	      {
		PUSH (NOSYMBOL, bfd_abs_section_ptr, va);
	      }
	    else
	      {
		/*
		  Thats all that we can understand. As far as I can see
		  there is a bug in the Microtec IEEE output which I'm
		  using to scan, whereby the comma operator is omitted
		  sometimes in an expression, giving expressions with too
		  many terms. We can tell if that's the case by ensuring
		  that sp == stack here. If not, then we've pushed
		  something too far, so we keep adding.  */

		while (sp != stack + 1)
		  {
		    asection *section1;
		    ieee_symbol_index_type sy1;
		    POP (sy1, section1, *extra);
		  }
		{
		  asection *dummy;

		  POP (*symbol, dummy, *value);
		  if (section)
		    *section = dummy;
		}

		loop = false;
	      }
	  }
	}
    }
}


#define ieee_seek(abfd, offset) \
  IEEE_DATA(abfd)->h.input_p = IEEE_DATA(abfd)->h.first_byte + offset

#define ieee_pos(abfd) \
  (IEEE_DATA(abfd)->h.input_p - IEEE_DATA(abfd)->h.first_byte)

static unsigned int last_index;
static char last_type;		/* is the index for an X or a D */

static ieee_symbol_type *
get_symbol (abfd,
	    ieee,
	    last_symbol,
	    symbol_count,
	    pptr,
	    max_index,
	    this_type
)
     bfd *abfd;
     ieee_data_type *ieee;
     ieee_symbol_type *last_symbol;
     unsigned int *symbol_count;
     ieee_symbol_type ***pptr;
     unsigned int *max_index;
     char this_type
      ;
{
  /* Need a new symbol */
  unsigned int new_index = must_parse_int (&(ieee->h));
  if (new_index != last_index || this_type != last_type)
    {
      ieee_symbol_type *new_symbol = (ieee_symbol_type *) bfd_alloc (ieee->h.abfd,
						 sizeof (ieee_symbol_type));
      if (!new_symbol)
	return NULL;

      new_symbol->index = new_index;
      last_index = new_index;
      (*symbol_count)++;
      **pptr = new_symbol;
      *pptr = &new_symbol->next;
      if (new_index > *max_index)
	{
	  *max_index = new_index;
	}
      last_type = this_type;
      new_symbol->symbol.section = bfd_abs_section_ptr;
      return new_symbol;
    }
  return last_symbol;
}

static boolean
ieee_slurp_external_symbols (abfd)
     bfd *abfd;
{
  ieee_data_type *ieee = IEEE_DATA (abfd);
  file_ptr offset = ieee->w.r.external_part;

  ieee_symbol_type **prev_symbols_ptr = &ieee->external_symbols;
  ieee_symbol_type **prev_reference_ptr = &ieee->external_reference;
  ieee_symbol_type *symbol = (ieee_symbol_type *) NULL;
  unsigned int symbol_count = 0;
  boolean loop = true;
  last_index = 0xffffff;
  ieee->symbol_table_full = true;

  ieee_seek (abfd, offset);

  while (loop)
    {
      switch (this_byte (&(ieee->h)))
	{
	case ieee_nn_record:
	  next_byte (&(ieee->h));

	  symbol = get_symbol (abfd, ieee, symbol, &symbol_count,
			       &prev_symbols_ptr,
			       &ieee->external_symbol_max_index, 'I');
	  if (symbol == NULL)
	    return false;

	  symbol->symbol.the_bfd = abfd;
	  symbol->symbol.name = read_id (&(ieee->h));
	  symbol->symbol.udata.p = (PTR) NULL;
	  symbol->symbol.flags = BSF_NO_FLAGS;
	  break;
	case ieee_external_symbol_enum:
	  next_byte (&(ieee->h));

	  symbol = get_symbol (abfd, ieee, symbol, &symbol_count,
			       &prev_symbols_ptr,
			       &ieee->external_symbol_max_index, 'D');
	  if (symbol == NULL)
	    return false;

	  BFD_ASSERT (symbol->index >= ieee->external_symbol_min_index);

	  symbol->symbol.the_bfd = abfd;
	  symbol->symbol.name = read_id (&(ieee->h));
	  symbol->symbol.udata.p = (PTR) NULL;
	  symbol->symbol.flags = BSF_NO_FLAGS;
	  break;
	case ieee_attribute_record_enum >> 8:
	  {
	    unsigned int symbol_name_index;
	    unsigned int symbol_type_index;
	    unsigned int symbol_attribute_def;
	    bfd_vma value;
	    switch (read_2bytes (ieee))
	      {
	      case ieee_attribute_record_enum:
		symbol_name_index = must_parse_int (&(ieee->h));
		symbol_type_index = must_parse_int (&(ieee->h));
		symbol_attribute_def = must_parse_int (&(ieee->h));
		switch (symbol_attribute_def)
		  {
		  case 8:
		  case 19:
		    parse_int (&ieee->h, &value);
		    break;
		  default:
		    (*_bfd_error_handler)
		      ("%s: unimplemented ATI record  %u for symbol %u",
		       bfd_get_filename (abfd), symbol_attribute_def,
		       symbol_name_index);
		    bfd_set_error (bfd_error_bad_value);
		    return false;
		    break;
		  }
		break;
	      case ieee_external_reference_info_record_enum:
		/* Skip over ATX record. */
		parse_int (&(ieee->h), &value);
		parse_int (&(ieee->h), &value);
		parse_int (&(ieee->h), &value);
		parse_int (&(ieee->h), &value);
		break;
	      }
	  }
	  break;
	case ieee_value_record_enum >> 8:
	  {
	    unsigned int symbol_name_index;
	    ieee_symbol_index_type symbol_ignore;
	    boolean pcrel_ignore;
	    unsigned int extra;
	    next_byte (&(ieee->h));
	    next_byte (&(ieee->h));

	    symbol_name_index = must_parse_int (&(ieee->h));
	    parse_expression (ieee,
			      &symbol->symbol.value,
			      &symbol_ignore,
			      &pcrel_ignore,
			      &extra,
			      &symbol->symbol.section);

	    symbol->symbol.flags = BSF_GLOBAL | BSF_EXPORT;

	  }
	  break;
	case ieee_weak_external_reference_enum:
	  {
	    bfd_vma size;
	    bfd_vma value;
	    next_byte (&(ieee->h));
	    /* Throw away the external reference index */
	    (void) must_parse_int (&(ieee->h));
	    /* Fetch the default size if not resolved */
	    size = must_parse_int (&(ieee->h));
	    /* Fetch the defautlt value if available */
	    if (parse_int (&(ieee->h), &value) == false)
	      {
		value = 0;
	      }
	    /* This turns into a common */
	    symbol->symbol.section = bfd_com_section_ptr;
	    symbol->symbol.value = size;
	  }
	  break;

	case ieee_external_reference_enum:
	  next_byte (&(ieee->h));

	  symbol = get_symbol (abfd, ieee, symbol, &symbol_count,
			       &prev_reference_ptr,
			       &ieee->external_reference_max_index, 'X');
	  if (symbol == NULL)
	    return false;

	  symbol->symbol.the_bfd = abfd;
	  symbol->symbol.name = read_id (&(ieee->h));
	  symbol->symbol.udata.p = (PTR) NULL;
	  symbol->symbol.section = bfd_und_section_ptr;
	  symbol->symbol.value = (bfd_vma) 0;
	  symbol->symbol.flags = 0;

	  BFD_ASSERT (symbol->index >= ieee->external_reference_min_index);
	  break;

	default:
	  loop = false;
	}
    }

  if (ieee->external_symbol_max_index != 0)
    {
      ieee->external_symbol_count =
	ieee->external_symbol_max_index -
	ieee->external_symbol_min_index + 1;
    }
  else
    {
      ieee->external_symbol_count = 0;
    }

  if (ieee->external_reference_max_index != 0)
    {
      ieee->external_reference_count =
	ieee->external_reference_max_index -
	ieee->external_reference_min_index + 1;
    }
  else
    {
      ieee->external_reference_count = 0;
    }

  abfd->symcount =
    ieee->external_reference_count + ieee->external_symbol_count;

  if (symbol_count != abfd->symcount)
    {
      /* There are gaps in the table -- */
      ieee->symbol_table_full = false;
    }

  *prev_symbols_ptr = (ieee_symbol_type *) NULL;
  *prev_reference_ptr = (ieee_symbol_type *) NULL;

  return true;
}

static boolean
ieee_slurp_symbol_table (abfd)
     bfd *abfd;
{
  if (IEEE_DATA (abfd)->read_symbols == false)
    {
      if (! ieee_slurp_external_symbols (abfd))
	return false;
      IEEE_DATA (abfd)->read_symbols = true;
    }
  return true;
}

long
ieee_get_symtab_upper_bound (abfd)
     bfd *abfd;
{
  if (! ieee_slurp_symbol_table (abfd))
    return -1;

  return (abfd->symcount != 0) ?
    (abfd->symcount + 1) * (sizeof (ieee_symbol_type *)) : 0;
}

/*
Move from our internal lists to the canon table, and insert in
symbol index order
*/

extern const bfd_target ieee_vec;

long
ieee_get_symtab (abfd, location)
     bfd *abfd;
     asymbol **location;
{
  ieee_symbol_type *symp;
  static bfd dummy_bfd;
  static asymbol empty_symbol =
  /* the_bfd, name, value, attr, section */
  {&dummy_bfd, " ieee empty", (symvalue) 0, BSF_DEBUGGING, bfd_abs_section_ptr};

  if (abfd->symcount)
    {
      ieee_data_type *ieee = IEEE_DATA (abfd);
      dummy_bfd.xvec = &ieee_vec;
      if (! ieee_slurp_symbol_table (abfd))
	return -1;

      if (ieee->symbol_table_full == false)
	{
	  /* Arrgh - there are gaps in the table, run through and fill them */
	  /* up with pointers to a null place */
	  unsigned int i;
	  for (i = 0; i < abfd->symcount; i++)
	    {
	      location[i] = &empty_symbol;
	    }
	}

      ieee->external_symbol_base_offset = -ieee->external_symbol_min_index;
      for (symp = IEEE_DATA (abfd)->external_symbols;
	   symp != (ieee_symbol_type *) NULL;
	   symp = symp->next)
	{
	  /* Place into table at correct index locations */
	  location[symp->index + ieee->external_symbol_base_offset] = &symp->symbol;
	}

      /* The external refs are indexed in a bit */
      ieee->external_reference_base_offset =
	-ieee->external_reference_min_index + ieee->external_symbol_count;

      for (symp = IEEE_DATA (abfd)->external_reference;
	   symp != (ieee_symbol_type *) NULL;
	   symp = symp->next)
	{
	  location[symp->index + ieee->external_reference_base_offset] =
	    &symp->symbol;

	}
    }
  if (abfd->symcount)
    {
      location[abfd->symcount] = (asymbol *) NULL;
    }
  return abfd->symcount;
}

static asection *
get_section_entry (abfd, ieee, index)
     bfd *abfd;
     ieee_data_type *ieee;
     unsigned int index;
{
  if (ieee->section_table[index] == (asection *) NULL)
    {
      char *tmp = bfd_alloc (abfd, 11);
      asection *section;

      if (!tmp)
	return NULL;
      sprintf (tmp, " fsec%4d", index);
      section = bfd_make_section (abfd, tmp);
      ieee->section_table[index] = section;
      section->flags = SEC_NO_FLAGS;
      section->target_index = index;
      ieee->section_table[index] = section;
    }
  return ieee->section_table[index];
}

static void
ieee_slurp_sections (abfd)
     bfd *abfd;
{
  ieee_data_type *ieee = IEEE_DATA (abfd);
  file_ptr offset = ieee->w.r.section_part;
  asection *section = (asection *) NULL;
  char *name;

  if (offset != 0)
    {
      bfd_byte section_type[3];
      ieee_seek (abfd, offset);
      while (true)
	{
	  switch (this_byte (&(ieee->h)))
	    {
	    case ieee_section_type_enum:
	      {
		unsigned int section_index;
		next_byte (&(ieee->h));
		section_index = must_parse_int (&(ieee->h));
		/* Fixme to be nice about a silly number of sections */
		BFD_ASSERT (section_index < NSECTIONS);

		section = get_section_entry (abfd, ieee, section_index);

		section_type[0] = this_byte_and_next (&(ieee->h));

		/* Set minimal section attributes. Attributes are
		   extended later, based on section contents. */

		switch (section_type[0])
		  {
		  case 0xC1:
		    /* Normal attributes for absolute sections	*/
		    section_type[1] = this_byte (&(ieee->h));
		    section->flags = SEC_ALLOC;
		    switch (section_type[1])
		      {
		      case 0xD3:	/* AS Absolute section attributes */
			next_byte (&(ieee->h));
			section_type[2] = this_byte (&(ieee->h));
			switch (section_type[2])
			  {
			  case 0xD0:
			    /* Normal code */
			    next_byte (&(ieee->h));
			    section->flags |= SEC_CODE;
			    break;
			  case 0xC4:
			    /* Normal data */
			    next_byte (&(ieee->h));
			    section->flags |= SEC_DATA;
			    break;
			  case 0xD2:
			    next_byte (&(ieee->h));
			    /* Normal rom data */
			    section->flags |= SEC_ROM | SEC_DATA;
			    break;
			  default:
			    break;
			  }
		      }
		    break;
		  case 0xC3:	/* Named relocatable sections (type C) */
		    section_type[1] = this_byte (&(ieee->h));
		    section->flags = SEC_ALLOC;
		    switch (section_type[1])
		      {
		      case 0xD0:	/* Normal code (CP) */
			next_byte (&(ieee->h));
			section->flags |= SEC_CODE;
			break;
		      case 0xC4:	/* Normal data (CD) */
			next_byte (&(ieee->h));
			section->flags |= SEC_DATA;
			break;
		      case 0xD2:	/* Normal rom data (CR) */
			next_byte (&(ieee->h));
			section->flags |= SEC_ROM | SEC_DATA;
			break;
		      default:
			break;
		      }
		  }

		/* Read section name, use it if non empty. */
		name = read_id (&ieee->h);
		if (name[0])
		  section->name = name;

		/* Skip these fields, which we don't care about */
		{
		  bfd_vma parent, brother, context;
		  parse_int (&(ieee->h), &parent);
		  parse_int (&(ieee->h), &brother);
		  parse_int (&(ieee->h), &context);
		}
	      }
	      break;
	    case ieee_section_alignment_enum:
	      {
		unsigned int section_index;
		bfd_vma value;
		asection *section;
		next_byte (&(ieee->h));
		section_index = must_parse_int (&ieee->h);
		section = get_section_entry (abfd, ieee, section_index);
		if (section_index > ieee->section_count)
		  {
		    ieee->section_count = section_index;
		  }
		section->alignment_power =
		  bfd_log2 (must_parse_int (&ieee->h));
		(void) parse_int (&(ieee->h), &value);
	      }
	      break;
	    case ieee_e2_first_byte_enum:
	      {
		ieee_record_enum_type t = (ieee_record_enum_type) (read_2bytes (&(ieee->h)));

		switch (t)
		  {
		  case ieee_section_size_enum:
		    section = ieee->section_table[must_parse_int (&(ieee->h))];
		    section->_raw_size = must_parse_int (&(ieee->h));
		    break;
		  case ieee_physical_region_size_enum:
		    section = ieee->section_table[must_parse_int (&(ieee->h))];
		    section->_raw_size = must_parse_int (&(ieee->h));
		    break;
		  case ieee_region_base_address_enum:
		    section = ieee->section_table[must_parse_int (&(ieee->h))];
		    section->vma = must_parse_int (&(ieee->h));
		    section->lma = section->vma;
		    break;
		  case ieee_mau_size_enum:
		    must_parse_int (&(ieee->h));
		    must_parse_int (&(ieee->h));
		    break;
		  case ieee_m_value_enum:
		    must_parse_int (&(ieee->h));
		    must_parse_int (&(ieee->h));
		    break;
		  case ieee_section_base_address_enum:
		    section = ieee->section_table[must_parse_int (&(ieee->h))];
		    section->vma = must_parse_int (&(ieee->h));
		    section->lma = section->vma;
		    break;
		  case ieee_section_offset_enum:
		    (void) must_parse_int (&(ieee->h));
		    (void) must_parse_int (&(ieee->h));
		    break;
		  default:
		    return;
		  }
	      }
	      break;
	    default:
	      return;
	    }
	}
    }
}

/* Make a section for the debugging information, if any.  We don't try
   to interpret the debugging information; we just point the section
   at the area in the file so that program which understand can dig it
   out.  */

static boolean
ieee_slurp_debug (abfd)
     bfd *abfd;
{
  ieee_data_type *ieee = IEEE_DATA (abfd);
  asection *sec;

  if (ieee->w.r.debug_information_part == 0)
    return true;

  sec = bfd_make_section (abfd, ".debug");
  if (sec == NULL)
    return false;
  sec->flags |= SEC_DEBUGGING | SEC_HAS_CONTENTS;
  sec->filepos = ieee->w.r.debug_information_part;
  sec->_raw_size = ieee->w.r.data_part - ieee->w.r.debug_information_part;

  return true;
}

/***********************************************************************
*  archive stuff
*/

const bfd_target *
ieee_archive_p (abfd)
     bfd *abfd;
{
  char *library;
  boolean loop;
  unsigned int i;
  unsigned char buffer[512];
  file_ptr buffer_offset = 0;
  ieee_ar_data_type *save = abfd->tdata.ieee_ar_data;
  ieee_ar_data_type *ieee;
  abfd->tdata.ieee_ar_data = (ieee_ar_data_type *) bfd_alloc (abfd, sizeof (ieee_ar_data_type));
  if (!abfd->tdata.ieee_ar_data)
    return NULL;
  ieee = IEEE_AR_DATA (abfd);

  /* FIXME: Check return value.  I'm not sure whether it needs to read
     the entire buffer or not.  */
  bfd_read ((PTR) buffer, 1, sizeof (buffer), abfd);

  ieee->h.first_byte = buffer;
  ieee->h.input_p = buffer;

  ieee->h.abfd = abfd;

  if (this_byte (&(ieee->h)) != Module_Beginning)
    {
      abfd->tdata.ieee_ar_data = save;
      return (const bfd_target *) NULL;
    }

  next_byte (&(ieee->h));
  library = read_id (&(ieee->h));
  if (strcmp (library, "LIBRARY") != 0)
    {
      bfd_release (abfd, ieee);
      abfd->tdata.ieee_ar_data = save;
      return (const bfd_target *) NULL;
    }
  /* Throw away the filename */
  read_id (&(ieee->h));

  ieee->element_count = 0;
  ieee->element_index = 0;

  next_byte (&(ieee->h));	/* Drop the ad part */
  must_parse_int (&(ieee->h));	/* And the two dummy numbers */
  must_parse_int (&(ieee->h));

  loop = true;
  /* Read the index of the BB table */
  while (loop)
    {
      ieee_ar_obstack_type t;
      int rec = read_2bytes (&(ieee->h));
      if (rec == (int) ieee_assign_value_to_variable_enum)
	{
	  must_parse_int (&(ieee->h));
	  t.file_offset = must_parse_int (&(ieee->h));
	  t.abfd = (bfd *) NULL;
	  ieee->element_count++;

	  bfd_alloc_grow (abfd, (PTR) &t, sizeof t);

	  /* Make sure that we don't go over the end of the buffer */

	  if ((size_t) ieee_pos (abfd) > sizeof (buffer) / 2)
	    {
	      /* Past half way, reseek and reprime */
	      buffer_offset += ieee_pos (abfd);
	      if (bfd_seek (abfd, buffer_offset, SEEK_SET) != 0)
		return NULL;
	      /* FIXME: Check return value.  I'm not sure whether it
		 needs to read the entire buffer or not.  */
	      bfd_read ((PTR) buffer, 1, sizeof (buffer), abfd);
	      ieee->h.first_byte = buffer;
	      ieee->h.input_p = buffer;
	    }
	}
      else
	loop = false;
    }

  ieee->elements = (ieee_ar_obstack_type *) bfd_alloc_finish (abfd);
  if (!ieee->elements)
    return (const bfd_target *) NULL;

  /* Now scan the area again, and replace BB offsets with file */
  /* offsets */

  for (i = 2; i < ieee->element_count; i++)
    {
      if (bfd_seek (abfd, ieee->elements[i].file_offset, SEEK_SET) != 0)
	return NULL;
      /* FIXME: Check return value.  I'm not sure whether it needs to
	 read the entire buffer or not.  */
      bfd_read ((PTR) buffer, 1, sizeof (buffer), abfd);
      ieee->h.first_byte = buffer;
      ieee->h.input_p = buffer;

      next_byte (&(ieee->h));	/* Drop F8 */
      next_byte (&(ieee->h));	/* Drop 14 */
      must_parse_int (&(ieee->h));	/* Drop size of block */
      if (must_parse_int (&(ieee->h)) != 0)
	{
	  /* This object has been deleted */
	  ieee->elements[i].file_offset = 0;
	}
      else
	{
	  ieee->elements[i].file_offset = must_parse_int (&(ieee->h));
	}
    }

/*  abfd->has_armap = ;*/
  return abfd->xvec;
}

static boolean
ieee_mkobject (abfd)
     bfd *abfd;
{
  abfd->tdata.ieee_data = (ieee_data_type *) bfd_zalloc (abfd, sizeof (ieee_data_type));
  return abfd->tdata.ieee_data ? true : false;
}

const bfd_target *
ieee_object_p (abfd)
     bfd *abfd;
{
  char *processor;
  unsigned int part;
  ieee_data_type *ieee;
  unsigned char buffer[300];
  ieee_data_type *save = IEEE_DATA (abfd);

  abfd->tdata.ieee_data = 0;
  ieee_mkobject (abfd);

  ieee = IEEE_DATA (abfd);
  if (bfd_seek (abfd, (file_ptr) 0, SEEK_SET) != 0)
    goto fail;
  /* Read the first few bytes in to see if it makes sense */
  /* FIXME: Check return value.  I'm not sure whether it needs to read
     the entire buffer or not.  */
  bfd_read ((PTR) buffer, 1, sizeof (buffer), abfd);

  ieee->h.input_p = buffer;
  if (this_byte_and_next (&(ieee->h)) != Module_Beginning)
    goto got_wrong_format;

  ieee->read_symbols = false;
  ieee->read_data = false;
  ieee->section_count = 0;
  ieee->external_symbol_max_index = 0;
  ieee->external_symbol_min_index = IEEE_PUBLIC_BASE;
  ieee->external_reference_min_index = IEEE_REFERENCE_BASE;
  ieee->external_reference_max_index = 0;
  ieee->h.abfd = abfd;
  memset ((PTR) ieee->section_table, 0, sizeof (ieee->section_table));

  processor = ieee->mb.processor = read_id (&(ieee->h));
  if (strcmp (processor, "LIBRARY") == 0)
    goto got_wrong_format;
  ieee->mb.module_name = read_id (&(ieee->h));
  if (abfd->filename == (CONST char *) NULL)
    {
      abfd->filename = ieee->mb.module_name;
    }
  /* Determine the architecture and machine type of the object file.
     */
  {
    const bfd_arch_info_type *arch = bfd_scan_arch (processor);
    if (arch == 0)
      goto got_wrong_format;
    abfd->arch_info = arch;
  }

  if (this_byte (&(ieee->h)) != (int) ieee_address_descriptor_enum)
    {
      goto fail;
    }
  next_byte (&(ieee->h));

  if (parse_int (&(ieee->h), &ieee->ad.number_of_bits_mau) == false)
    {
      goto fail;
    }
  if (parse_int (&(ieee->h), &ieee->ad.number_of_maus_in_address) == false)
    {
      goto fail;
    }

  /* If there is a byte order info, take it */
  if (this_byte (&(ieee->h)) == (int) ieee_variable_L_enum ||
      this_byte (&(ieee->h)) == (int) ieee_variable_M_enum)
    next_byte (&(ieee->h));

  for (part = 0; part < N_W_VARIABLES; part++)
    {
      boolean ok;
      if (read_2bytes (&(ieee->h)) != (int) ieee_assign_value_to_variable_enum)
	{
	  goto fail;
	}
      if (this_byte_and_next (&(ieee->h)) != part)
	{
	  goto fail;
	}

      ieee->w.offset[part] = parse_i (&(ieee->h), &ok);
      if (ok == false)
	{
	  goto fail;
	}

    }

  if (ieee->w.r.external_part != 0)
    abfd->flags = HAS_SYMS;

  /* By now we know that this is a real IEEE file, we're going to read
     the whole thing into memory so that we can run up and down it
     quickly.  We can work out how big the file is from the trailer
     record */

  IEEE_DATA (abfd)->h.first_byte =
    (unsigned char *) bfd_alloc (ieee->h.abfd, ieee->w.r.me_record + 1);
  if (!IEEE_DATA (abfd)->h.first_byte)
    goto fail;
  if (bfd_seek (abfd, (file_ptr) 0, SEEK_SET) != 0)
    goto fail;
  /* FIXME: Check return value.  I'm not sure whether it needs to read
     the entire buffer or not.  */
  bfd_read ((PTR) (IEEE_DATA (abfd)->h.first_byte), 1,
	    ieee->w.r.me_record + 1, abfd);

  ieee_slurp_sections (abfd);

  if (! ieee_slurp_debug (abfd))
    goto fail;

  /* Parse section data to activate file and section flags implied by
     section contents. */

  if (! ieee_slurp_section_data (abfd))
    goto fail;
    
  return abfd->xvec;
got_wrong_format:
  bfd_set_error (bfd_error_wrong_format);
fail:
  (void) bfd_release (abfd, ieee);
  abfd->tdata.ieee_data = save;
  return (const bfd_target *) NULL;
}

void
ieee_get_symbol_info (ignore_abfd, symbol, ret)
     bfd *ignore_abfd;
     asymbol *symbol;
     symbol_info *ret;
{
  bfd_symbol_info (symbol, ret);
  if (symbol->name[0] == ' ')
    ret->name = "* empty table entry ";
  if (!symbol->section)
    ret->type = (symbol->flags & BSF_LOCAL) ? 'a' : 'A';
}

void
ieee_print_symbol (ignore_abfd, afile, symbol, how)
     bfd *ignore_abfd;
     PTR afile;
     asymbol *symbol;
     bfd_print_symbol_type how;
{
  FILE *file = (FILE *) afile;

  switch (how)
    {
    case bfd_print_symbol_name:
      fprintf (file, "%s", symbol->name);
      break;
    case bfd_print_symbol_more:
#if 0
      fprintf (file, "%4x %2x", aout_symbol (symbol)->desc & 0xffff,
	       aout_symbol (symbol)->other & 0xff);
#endif
      BFD_FAIL ();
      break;
    case bfd_print_symbol_all:
      {
	const char *section_name =
	  (symbol->section == (asection *) NULL
	   ? "*abs"
	   : symbol->section->name);
	if (symbol->name[0] == ' ')
	  {
	    fprintf (file, "* empty table entry ");
	  }
	else
	  {
	    bfd_print_symbol_vandf ((PTR) file, symbol);

	    fprintf (file, " %-5s %04x %02x %s",
		     section_name,
		     (unsigned) ieee_symbol (symbol)->index,
		     (unsigned) 0,
		     symbol->name);
	  }
      }
      break;
    }
}

static boolean
do_one (ieee, current_map, location_ptr, s, iterations)
     ieee_data_type *ieee;
     ieee_per_section_type *current_map;
     unsigned char *location_ptr;
     asection *s;
     int iterations;
{
  switch (this_byte (&(ieee->h)))
    {
    case ieee_load_constant_bytes_enum:
      {
	unsigned int number_of_maus;
	unsigned int i;
	next_byte (&(ieee->h));
	number_of_maus = must_parse_int (&(ieee->h));

	for (i = 0; i < number_of_maus; i++)
	  {
	    location_ptr[current_map->pc++] = this_byte (&(ieee->h));
	    next_byte (&(ieee->h));
	  }
      }
      break;

    case ieee_load_with_relocation_enum:
      {
	boolean loop = true;
	next_byte (&(ieee->h));
	while (loop)
	  {
	    switch (this_byte (&(ieee->h)))
	      {
	      case ieee_variable_R_enum:

	      case ieee_function_signed_open_b_enum:
	      case ieee_function_unsigned_open_b_enum:
	      case ieee_function_either_open_b_enum:
		{
		  unsigned int extra = 4;
		  boolean pcrel = false;
		  asection *section;
		  ieee_reloc_type *r =
		  (ieee_reloc_type *) bfd_alloc (ieee->h.abfd,
						 sizeof (ieee_reloc_type));
		  if (!r)
		    return false;

		  *(current_map->reloc_tail_ptr) = r;
		  current_map->reloc_tail_ptr = &r->next;
		  r->next = (ieee_reloc_type *) NULL;
		  next_byte (&(ieee->h));
/*			    abort();*/
		  r->relent.sym_ptr_ptr = 0;
		  parse_expression (ieee,
				    &r->relent.addend,
				    &r->symbol,
				    &pcrel, &extra, &section);
		  r->relent.address = current_map->pc;
		  s->flags |= SEC_RELOC;
		  s->owner->flags |= HAS_RELOC;
		  s->reloc_count++;
		  if (r->relent.sym_ptr_ptr == 0)
		    {
		      r->relent.sym_ptr_ptr = section->symbol_ptr_ptr;
		    }

		  if (this_byte (&(ieee->h)) == (int) ieee_comma)
		    {
		      next_byte (&(ieee->h));
		      /* Fetch number of bytes to pad */
		      extra = must_parse_int (&(ieee->h));
		    };

		  switch (this_byte (&(ieee->h)))
		    {
		    case ieee_function_signed_close_b_enum:
		      next_byte (&(ieee->h));
		      break;
		    case ieee_function_unsigned_close_b_enum:
		      next_byte (&(ieee->h));
		      break;
		    case ieee_function_either_close_b_enum:
		      next_byte (&(ieee->h));
		      break;
		    default:
		      break;
		    }
		  /* Build a relocation entry for this type */
		  /* If pc rel then stick -ve pc into instruction
		     and take out of reloc ..

		     I've changed this. It's all too complicated. I
		     keep 0 in the instruction now.  */

		  switch (extra)
		    {
		    case 0:
		    case 4:

		      if (pcrel == true)
			{
#if KEEPMINUSPCININST
			  bfd_put_32 (ieee->h.abfd, -current_map->pc, location_ptr +
				      current_map->pc);
			  r->relent.howto = &rel32_howto;
			  r->relent.addend -=
			    current_map->pc;
#else
			  bfd_put_32 (ieee->h.abfd, 0, location_ptr +
				      current_map->pc);
			  r->relent.howto = &rel32_howto;
#endif
			}
		      else
			{
			  bfd_put_32 (ieee->h.abfd, 0, location_ptr +
				      current_map->pc);
			  r->relent.howto = &abs32_howto;
			}
		      current_map->pc += 4;
		      break;
		    case 2:
		      if (pcrel == true)
			{
#if KEEPMINUSPCININST
			  bfd_put_16 (ieee->h.abfd, (int) (-current_map->pc), location_ptr + current_map->pc);
			  r->relent.addend -= current_map->pc;
			  r->relent.howto = &rel16_howto;
#else

			  bfd_put_16 (ieee->h.abfd, 0, location_ptr + current_map->pc);
			  r->relent.howto = &rel16_howto;
#endif
			}

		      else
			{
			  bfd_put_16 (ieee->h.abfd, 0, location_ptr + current_map->pc);
			  r->relent.howto = &abs16_howto;
			}
		      current_map->pc += 2;
		      break;
		    case 1:
		      if (pcrel == true)
			{
#if KEEPMINUSPCININST
			  bfd_put_8 (ieee->h.abfd, (int) (-current_map->pc), location_ptr + current_map->pc);
			  r->relent.addend -= current_map->pc;
			  r->relent.howto = &rel8_howto;
#else
			  bfd_put_8 (ieee->h.abfd, 0, location_ptr + current_map->pc);
			  r->relent.howto = &rel8_howto;
#endif
			}
		      else
			{
			  bfd_put_8 (ieee->h.abfd, 0, location_ptr + current_map->pc);
			  r->relent.howto = &abs8_howto;
			}
		      current_map->pc += 1;
		      break;

		    default:
		      BFD_FAIL ();
		      return false;
		    }
		}
		break;
	      default:
		{
		  bfd_vma this_size;
		  if (parse_int (&(ieee->h), &this_size) == true)
		    {
		      unsigned int i;
		      for (i = 0; i < this_size; i++)
			{
			  location_ptr[current_map->pc++] = this_byte (&(ieee->h));
			  next_byte (&(ieee->h));
			}
		    }
		  else
		    {
		      loop = false;
		    }
		}
	      }

	    /* Prevent more than the first load-item of an LR record
	       from being repeated (MRI convention). */
	    if (iterations != 1)
	      loop = false;
	  }
      }
    }
  return true;
}

/* Read in all the section data and relocation stuff too */
static boolean
ieee_slurp_section_data (abfd)
     bfd *abfd;
{
  bfd_byte *location_ptr = (bfd_byte *) NULL;
  ieee_data_type *ieee = IEEE_DATA (abfd);
  unsigned int section_number;

  ieee_per_section_type *current_map = (ieee_per_section_type *) NULL;
  asection *s;
  /* Seek to the start of the data area */
  if (ieee->read_data == true)
    return true;
  ieee->read_data = true;
  ieee_seek (abfd, ieee->w.r.data_part);

  /* Allocate enough space for all the section contents */

  for (s = abfd->sections; s != (asection *) NULL; s = s->next)
    {
      ieee_per_section_type *per = (ieee_per_section_type *) s->used_by_bfd;
      if ((s->flags & SEC_DEBUGGING) != 0)
	continue;
      per->data = (bfd_byte *) bfd_alloc (ieee->h.abfd, s->_raw_size);
      if (!per->data)
	return false;
      /*SUPPRESS 68*/
      per->reloc_tail_ptr =
	(ieee_reloc_type **) & (s->relocation);
    }

  while (true)
    {
      switch (this_byte (&(ieee->h)))
	{
	  /* IF we see anything strange then quit */
	default:
	  return true;

	case ieee_set_current_section_enum:
	  next_byte (&(ieee->h));
	  section_number = must_parse_int (&(ieee->h));
	  s = ieee->section_table[section_number];
	  s->flags |= SEC_LOAD | SEC_HAS_CONTENTS;
	  current_map = (ieee_per_section_type *) s->used_by_bfd;
	  location_ptr = current_map->data - s->vma;
	  /* The document I have says that Microtec's compilers reset */
	  /* this after a sec section, even though the standard says not */
	  /* to. SO .. */
	  current_map->pc = s->vma;
	  break;

	case ieee_e2_first_byte_enum:
	  next_byte (&(ieee->h));
	  switch (this_byte (&(ieee->h)))
	    {
	    case ieee_set_current_pc_enum & 0xff:
	      {
		bfd_vma value;
		ieee_symbol_index_type symbol;
		unsigned int extra;
		boolean pcrel;
		next_byte (&(ieee->h));
		must_parse_int (&(ieee->h));	/* Thow away section #*/
		parse_expression (ieee, &value,
				  &symbol,
				  &pcrel, &extra,
				  0);
		current_map->pc = value;
		BFD_ASSERT ((unsigned) (value - s->vma) <= s->_raw_size);
	      }
	      break;

	    case ieee_value_starting_address_enum & 0xff:
	      /* We've got to the end of the data now - */
	      return true;
	    default:
	      BFD_FAIL ();
	      return false;
	    }
	  break;
	case ieee_repeat_data_enum:
	  {
	    /* Repeat the following LD or LR n times - we do this by
		 remembering the stream pointer before running it and
		 resetting it and running it n times. We special case
		 the repetition of a repeat_data/load_constant
		 */

	    unsigned int iterations;
	    unsigned char *start;
	    next_byte (&(ieee->h));
	    iterations = must_parse_int (&(ieee->h));
	    start = ieee->h.input_p;
	    if (start[0] == (int) ieee_load_constant_bytes_enum &&
		start[1] == 1)
	      {
		while (iterations != 0)
		  {
		    location_ptr[current_map->pc++] = start[2];
		    iterations--;
		  }
		next_byte (&(ieee->h));
		next_byte (&(ieee->h));
		next_byte (&(ieee->h));
	      }
	    else
	      {
		while (iterations != 0)
		  {
		    ieee->h.input_p = start;
		    if (!do_one (ieee, current_map, location_ptr, s,
				 iterations))
		      return false;
		    iterations--;
		  }
	      }
	  }
	  break;
	case ieee_load_constant_bytes_enum:
	case ieee_load_with_relocation_enum:
	  {
	    if (!do_one (ieee, current_map, location_ptr, s, 1))
	      return false;
	  }
	}
    }
}

boolean
ieee_new_section_hook (abfd, newsect)
     bfd *abfd;
     asection *newsect;
{
  newsect->used_by_bfd = (PTR)
    bfd_alloc (abfd, sizeof (ieee_per_section_type));
  if (!newsect->used_by_bfd)
    return false;
  ieee_per_section (newsect)->data = (bfd_byte *) NULL;
  ieee_per_section (newsect)->section = newsect;
  return true;
}

long
ieee_get_reloc_upper_bound (abfd, asect)
     bfd *abfd;
     sec_ptr asect;
{
  if ((asect->flags & SEC_DEBUGGING) != 0)
    return 0;
  if (! ieee_slurp_section_data (abfd))
    return -1;
  return (asect->reloc_count + 1) * sizeof (arelent *);
}

static boolean
ieee_get_section_contents (abfd, section, location, offset, count)
     bfd *abfd;
     sec_ptr section;
     PTR location;
     file_ptr offset;
     bfd_size_type count;
{
  ieee_per_section_type *p = (ieee_per_section_type *) section->used_by_bfd;
  if ((section->flags & SEC_DEBUGGING) != 0)
    return _bfd_generic_get_section_contents (abfd, section, location,
					      offset, count);
  ieee_slurp_section_data (abfd);
  (void) memcpy ((PTR) location, (PTR) (p->data + offset), (unsigned) count);
  return true;
}

long
ieee_canonicalize_reloc (abfd, section, relptr, symbols)
     bfd *abfd;
     sec_ptr section;
     arelent **relptr;
     asymbol **symbols;
{
/*  ieee_per_section_type *p = (ieee_per_section_type *) section->used_by_bfd;*/
  ieee_reloc_type *src = (ieee_reloc_type *) (section->relocation);
  ieee_data_type *ieee = IEEE_DATA (abfd);

  if ((section->flags & SEC_DEBUGGING) != 0)
    return 0;

  while (src != (ieee_reloc_type *) NULL)
    {
      /* Work out which symbol to attach it this reloc to */
      switch (src->symbol.letter)
	{
	case 'I':
	  src->relent.sym_ptr_ptr =
	    symbols + src->symbol.index + ieee->external_symbol_base_offset;
	  break;
	case 'X':
	  src->relent.sym_ptr_ptr =
	    symbols + src->symbol.index + ieee->external_reference_base_offset;
	  break;
	case 0:
	  src->relent.sym_ptr_ptr =
	    src->relent.sym_ptr_ptr[0]->section->symbol_ptr_ptr;
	  break;
	default:

	  BFD_FAIL ();
	}
      *relptr++ = &src->relent;
      src = src->next;
    }
  *relptr = (arelent *) NULL;
  return section->reloc_count;
}

static int
comp (ap, bp)
     CONST PTR ap;
     CONST PTR bp;
{
  arelent *a = *((arelent **) ap);
  arelent *b = *((arelent **) bp);
  return a->address - b->address;
}

/* Write the section headers.  */

static boolean
ieee_write_section_part (abfd)
     bfd *abfd;
{
  ieee_data_type *ieee = IEEE_DATA (abfd);
  asection *s;
  ieee->w.r.section_part = bfd_tell (abfd);
  for (s = abfd->sections; s != (asection *) NULL; s = s->next)
    {
      if (! bfd_is_abs_section (s)
	  && (s->flags & SEC_DEBUGGING) == 0)
	{
	  if (! ieee_write_byte (abfd, ieee_section_type_enum)
	      || ! ieee_write_byte (abfd,
				    (bfd_byte) (s->index
						+ IEEE_SECTION_NUMBER_BASE)))
	    return false;

	  if (abfd->flags & EXEC_P)
	    {
	      /* This image is executable, so output absolute sections */
	      if (! ieee_write_byte (abfd, ieee_variable_A_enum)
		  || ! ieee_write_byte (abfd, ieee_variable_S_enum))
		return false;
	    }
	  else
	    {
	      if (! ieee_write_byte (abfd, ieee_variable_C_enum))
		return false;
	    }

	  switch (s->flags & (SEC_CODE | SEC_DATA | SEC_ROM))
	    {
	    case SEC_CODE | SEC_LOAD:
	    case SEC_CODE:
	      if (! ieee_write_byte (abfd, ieee_variable_P_enum))
		return false;
	      break;
	    case SEC_DATA:
	    default:
	      if (! ieee_write_byte (abfd, ieee_variable_D_enum))
		return false;
	      break;
	    case SEC_ROM:
	    case SEC_ROM | SEC_DATA:
	    case SEC_ROM | SEC_LOAD:
	    case SEC_ROM | SEC_DATA | SEC_LOAD:
	      if (! ieee_write_byte (abfd, ieee_variable_R_enum))
		return false;
	    }


	  if (! ieee_write_id (abfd, s->name))
	    return false;
#if 0
	  ieee_write_int (abfd, 0);	/* Parent */
	  ieee_write_int (abfd, 0);	/* Brother */
	  ieee_write_int (abfd, 0);	/* Context */
#endif
	  /* Alignment */
	  if (! ieee_write_byte (abfd, ieee_section_alignment_enum)
	      || ! ieee_write_byte (abfd,
				    (bfd_byte) (s->index
						+ IEEE_SECTION_NUMBER_BASE))
	      || ! ieee_write_int (abfd, 1 << s->alignment_power))
	    return false;

	  /* Size */
	  if (! ieee_write_2bytes (abfd, ieee_section_size_enum)
	      || ! ieee_write_byte (abfd,
				    (bfd_byte) (s->index
						+ IEEE_SECTION_NUMBER_BASE))
	      || ! ieee_write_int (abfd, s->_raw_size))
	    return false;
	  if (abfd->flags & EXEC_P)
	    {
	      /* Relocateable sections don't have asl records */
	      /* Vma */
	      if (! ieee_write_2bytes (abfd, ieee_section_base_address_enum)
		  || ! ieee_write_byte (abfd,
					((bfd_byte)
					 (s->index
					  + IEEE_SECTION_NUMBER_BASE)))
		  || ! ieee_write_int (abfd, s->vma))
		return false;
	    }
	}
    }

  return true;
}


static boolean
do_with_relocs (abfd, s)
     bfd *abfd;
     asection *s;
{
  unsigned int number_of_maus_in_address =
    bfd_arch_bits_per_address (abfd) / bfd_arch_bits_per_byte (abfd);
  unsigned int relocs_to_go = s->reloc_count;
  bfd_byte *stream = ieee_per_section (s)->data;
  arelent **p = s->orelocation;
  bfd_size_type current_byte_index = 0;

  qsort (s->orelocation,
	 relocs_to_go,
	 sizeof (arelent **),
	 comp);

  /* Output the section preheader */
  if (! ieee_write_byte (abfd, ieee_set_current_section_enum)
      || ! ieee_write_byte (abfd,
			    (bfd_byte) (s->index + IEEE_SECTION_NUMBER_BASE))
      || ! ieee_write_2bytes (abfd, ieee_set_current_pc_enum)
      || ! ieee_write_byte (abfd,
			    (bfd_byte) (s->index + IEEE_SECTION_NUMBER_BASE)))
    return false;
  if ((abfd->flags & EXEC_P) != 0 && relocs_to_go == 0)
    {
      if (! ieee_write_int (abfd, s->vma))
	return false;
    }
  else
    {
      if (! ieee_write_expression (abfd, 0, s->symbol, 0, 0))
	return false;
    }

  if (relocs_to_go == 0)
    {
      /* If there aren't any relocations then output the load constant
	 byte opcode rather than the load with relocation opcode */

      while (current_byte_index < s->_raw_size)
	{
	  bfd_size_type run;
	  unsigned int MAXRUN = 127;
	  run = MAXRUN;
	  if (run > s->_raw_size - current_byte_index)
	    {
	      run = s->_raw_size - current_byte_index;
	    }

	  if (run != 0)
	    {
	      if (! ieee_write_byte (abfd, ieee_load_constant_bytes_enum))
		return false;
	      /* Output a stream of bytes */
	      if (! ieee_write_int (abfd, run))
		return false;
	      if (bfd_write ((PTR) (stream + current_byte_index),
			     1,
			     run,
			     abfd)
		  != run)
		return false;
	      current_byte_index += run;
	    }
	}
    }
  else
    {
      if (! ieee_write_byte (abfd, ieee_load_with_relocation_enum))
	return false;

      /* Output the data stream as the longest sequence of bytes
	 possible, allowing for the a reasonable packet size and
	 relocation stuffs.  */

      if ((PTR) stream == (PTR) NULL)
	{
	  /* Outputting a section without data, fill it up */
	  stream = (unsigned char *) (bfd_alloc (abfd, s->_raw_size));
	  if (!stream)
	    return false;
	  memset ((PTR) stream, 0, (size_t) s->_raw_size);
	}
      while (current_byte_index < s->_raw_size)
	{
	  bfd_size_type run;
	  unsigned int MAXRUN = 127;
	  if (relocs_to_go)
	    {
	      run = (*p)->address - current_byte_index;
	      if (run > MAXRUN)
		run = MAXRUN;
	    }
	  else
	    {
	      run = MAXRUN;
	    }
	  if (run > s->_raw_size - current_byte_index)
	    {
	      run = s->_raw_size - current_byte_index;
	    }

	  if (run != 0)
	    {
	      /* Output a stream of bytes */
	      if (! ieee_write_int (abfd, run))
		return false;
	      if (bfd_write ((PTR) (stream + current_byte_index),
			     1,
			     run,
			     abfd)
		  != run)
		return false;
	      current_byte_index += run;
	    }
	  /* Output any relocations here */
	  if (relocs_to_go && (*p) && (*p)->address == current_byte_index)
	    {
	      while (relocs_to_go
		     && (*p) && (*p)->address == current_byte_index)
		{
		  arelent *r = *p;
		  bfd_signed_vma ov;

#if 0
		  if (r->howto->pc_relative)
		    {
		      r->addend += current_byte_index;
		    }
#endif

		  switch (r->howto->size)
		    {
		    case 2:

		      ov = bfd_get_signed_32 (abfd,
					      stream + current_byte_index);
		      current_byte_index += 4;
		      break;
		    case 1:
		      ov = bfd_get_signed_16 (abfd,
					      stream + current_byte_index);
		      current_byte_index += 2;
		      break;
		    case 0:
		      ov = bfd_get_signed_8 (abfd,
					     stream + current_byte_index);
		      current_byte_index++;
		      break;
		    default:
		      ov = 0;
		      BFD_FAIL ();
		      return false;
		    }

		  ov &= r->howto->src_mask;

		  if (r->howto->pc_relative
		      && ! r->howto->pcrel_offset)
		    ov += r->address;

		  if (! ieee_write_byte (abfd,
					 ieee_function_either_open_b_enum))
		    return false;

/*		  abort();*/

		  if (r->sym_ptr_ptr != (asymbol **) NULL)
		    {
		      if (! ieee_write_expression (abfd, r->addend + ov,
						   *(r->sym_ptr_ptr),
						   r->howto->pc_relative,
						   s->index))
			return false;
		    }
		  else
		    {
		      if (! ieee_write_expression (abfd, r->addend + ov,
						   (asymbol *) NULL,
						   r->howto->pc_relative,
						   s->index))
			return false;
		    }

		  if (number_of_maus_in_address
		      != bfd_get_reloc_size (r->howto))
		    {
		      if (! ieee_write_int (abfd,
					    bfd_get_reloc_size (r->howto)))
			return false;
		    }
		  if (! ieee_write_byte (abfd,
					 ieee_function_either_close_b_enum))
		    return false;

		  relocs_to_go--;
		  p++;
		}

	    }
	}
    }

  return true;
}

/* If there are no relocations in the output section then we can be
   clever about how we write.  We block items up into a max of 127
   bytes.  */

static boolean
do_as_repeat (abfd, s)
     bfd *abfd;
     asection *s;
{
  if (s->_raw_size)
    {
      if (! ieee_write_byte (abfd, ieee_set_current_section_enum)
	  || ! ieee_write_byte (abfd,
				(bfd_byte) (s->index
					    + IEEE_SECTION_NUMBER_BASE))
	  || ! ieee_write_byte (abfd, ieee_set_current_pc_enum >> 8)
	  || ! ieee_write_byte (abfd, ieee_set_current_pc_enum & 0xff)
	  || ! ieee_write_byte (abfd,
				(bfd_byte) (s->index
					    + IEEE_SECTION_NUMBER_BASE))
	  || ! ieee_write_int (abfd, s->vma)
	  || ! ieee_write_byte (abfd, ieee_repeat_data_enum)
	  || ! ieee_write_int (abfd, s->_raw_size)
	  || ! ieee_write_byte (abfd, ieee_load_constant_bytes_enum)
	  || ! ieee_write_byte (abfd, 1)
	  || ! ieee_write_byte (abfd, 0))
	return false;
    }

  return true;
}

static boolean
do_without_relocs (abfd, s)
     bfd *abfd;
     asection *s;
{
  bfd_byte *stream = ieee_per_section (s)->data;

  if (stream == 0 || ((s->flags & SEC_LOAD) == 0))
    {
      if (! do_as_repeat (abfd, s))
	return false;
    }
  else
    {
      unsigned int i;
      for (i = 0; i < s->_raw_size; i++)
	{
	  if (stream[i] != 0)
	    {
	      if (! do_with_relocs (abfd, s))
		return false;
	      return true;
	    }
	}
      if (! do_as_repeat (abfd, s))
	return false;
    }

  return true;
}


static unsigned char *output_ptr_start;
static unsigned char *output_ptr;
static unsigned char *output_ptr_end;
static unsigned char *input_ptr_start;
static unsigned char *input_ptr;
static unsigned char *input_ptr_end;
static bfd *input_bfd;
static bfd *output_bfd;
static int output_buffer;

static void
fill ()
{
  /* FIXME: Check return value.  I'm not sure whether it needs to read
     the entire buffer or not.  */
  bfd_read ((PTR) input_ptr_start, 1, input_ptr_end - input_ptr_start, input_bfd);
  input_ptr = input_ptr_start;
}
static void
flush ()
{
  if (bfd_write ((PTR) (output_ptr_start), 1, output_ptr - output_ptr_start,
		 output_bfd)
      != (bfd_size_type) (output_ptr - output_ptr_start))
    abort ();
  output_ptr = output_ptr_start;
  output_buffer++;
}

#define THIS() ( *input_ptr )
#define NEXT() { input_ptr++; if (input_ptr == input_ptr_end) fill(); }
#define OUT(x) { *output_ptr++ = (x); if(output_ptr == output_ptr_end)  flush(); }

static void
write_int (value)
     int value;
{
  if (value >= 0 && value <= 127)
    {
      OUT (value);
    }
  else
    {
      unsigned int length;
      /* How many significant bytes ? */
      /* FIXME FOR LONGER INTS */
      if (value & 0xff000000)
	{
	  length = 4;
	}
      else if (value & 0x00ff0000)
	{
	  length = 3;
	}
      else if (value & 0x0000ff00)
	{
	  length = 2;
	}
      else
	length = 1;

      OUT ((int) ieee_number_repeat_start_enum + length);
      switch (length)
	{
	case 4:
	  OUT (value >> 24);
	case 3:
	  OUT (value >> 16);
	case 2:
	  OUT (value >> 8);
	case 1:
	  OUT (value);
	}

    }
}

static void
copy_id ()
{
  int length = THIS ();
  char ch;
  OUT (length);
  NEXT ();
  while (length--)
    {
      ch = THIS ();
      OUT (ch);
      NEXT ();
    }
}

#define VAR(x) ((x | 0x80))
static void
copy_expression ()
{
  int stack[10];
  int *tos = stack;
  int value = 0;
  while (1)
    {
      switch (THIS ())
	{
	case 0x84:
	  NEXT ();
	  value = THIS ();
	  NEXT ();
	  value = (value << 8) | THIS ();
	  NEXT ();
	  value = (value << 8) | THIS ();
	  NEXT ();
	  value = (value << 8) | THIS ();
	  NEXT ();
	  *tos++ = value;
	  break;
	case 0x83:
	  NEXT ();
	  value = THIS ();
	  NEXT ();
	  value = (value << 8) | THIS ();
	  NEXT ();
	  value = (value << 8) | THIS ();
	  NEXT ();
	  *tos++ = value;
	  break;
	case 0x82:
	  NEXT ();
	  value = THIS ();
	  NEXT ();
	  value = (value << 8) | THIS ();
	  NEXT ();
	  *tos++ = value;
	  break;
	case 0x81:
	  NEXT ();
	  value = THIS ();
	  NEXT ();
	  *tos++ = value;
	  break;
	case 0x80:
	  NEXT ();
	  *tos++ = 0;
	  break;
	default:
	  if (THIS () > 0x84)
	    {
	      /* Not a number, just bug out with the answer */
	      write_int (*(--tos));
	      return;
	    }
	  *tos++ = THIS ();
	  NEXT ();
	  value = 0;
	  break;
	case 0xa5:
	  /* PLUS anything */
	  {
	    int value = *(--tos);
	    value += *(--tos);
	    *tos++ = value;
	    NEXT ();
	  }
	  break;
	case VAR ('R'):
	  {
	    int section_number;
	    ieee_data_type *ieee;
	    asection *s;
	    NEXT ();
	    section_number = THIS ();

	    NEXT ();
	    ieee = IEEE_DATA (input_bfd);
	    s = ieee->section_table[section_number];
	    if (s->output_section)
	      {
		value = s->output_section->vma;
	      }
	    else
	      {
		value = 0;
	      }
	    value += s->output_offset;
	    *tos++ = value;
	    value = 0;
	  }
	  break;
	case 0x90:
	  {
	    NEXT ();
	    write_int (*(--tos));
	    OUT (0x90);
	    return;

	  }
	}
    }

}

/* Drop the int in the buffer, and copy a null into the gap, which we
   will overwrite later */

struct output_buffer_struct
{
  unsigned char *ptrp;
  int buffer;
};

static void
fill_int (buf)
     struct output_buffer_struct *buf;
{
  if (buf->buffer == output_buffer)
    {
      /* Still a chance to output the size */
      int value = output_ptr - buf->ptrp + 3;
      buf->ptrp[0] = value >> 24;
      buf->ptrp[1] = value >> 16;
      buf->ptrp[2] = value >> 8;
      buf->ptrp[3] = value >> 0;
    }
}

static void
drop_int (buf)
     struct output_buffer_struct *buf;
{
  int type = THIS ();
  int ch;
  if (type <= 0x84)
    {
      NEXT ();
      switch (type)
	{
	case 0x84:
	  ch = THIS ();
	  NEXT ();
	case 0x83:
	  ch = THIS ();
	  NEXT ();
	case 0x82:
	  ch = THIS ();
	  NEXT ();
	case 0x81:
	  ch = THIS ();
	  NEXT ();
	case 0x80:
	  break;
	}
    }
  OUT (0x84);
  buf->ptrp = output_ptr;
  buf->buffer = output_buffer;
  OUT (0);
  OUT (0);
  OUT (0);
  OUT (0);
}

static void
copy_int ()
{
  int type = THIS ();
  int ch;
  if (type <= 0x84)
    {
      OUT (type);
      NEXT ();
      switch (type)
	{
	case 0x84:
	  ch = THIS ();
	  NEXT ();
	  OUT (ch);
	case 0x83:
	  ch = THIS ();
	  NEXT ();
	  OUT (ch);
	case 0x82:
	  ch = THIS ();
	  NEXT ();
	  OUT (ch);
	case 0x81:
	  ch = THIS ();
	  NEXT ();
	  OUT (ch);
	case 0x80:
	  break;
	}
    }
}

#define ID copy_id()
#define INT copy_int()
#define EXP copy_expression()
static void copy_till_end ();
#define INTn(q) copy_int()
#define EXPn(q) copy_expression()

static void
f1_record ()
{
  int ch;
  /* ATN record */
  NEXT ();
  ch = THIS ();
  switch (ch)
    {
    default:
      OUT (0xf1);
      OUT (ch);
      break;
    case 0xc9:
      NEXT ();
      OUT (0xf1);
      OUT (0xc9);
      INT;
      INT;
      ch = THIS ();
      switch (ch)
	{
	case 0x16:
	  NEXT ();
	  break;
	case 0x01:
	  NEXT ();
	  break;
	case 0x00:
	  NEXT ();
	  INT;
	  break;
	case 0x03:
	  NEXT ();
	  INT;
	  break;
	case 0x13:
	  EXPn (instruction address);
	  break;
	default:
	  break;
	}
      break;
    case 0xd8:
      /* EXternal ref */
      NEXT ();
      OUT (0xf1);
      OUT (0xd8);
      EXP;
      EXP;
      EXP;
      EXP;
      break;
    case 0xce:
      NEXT ();
      OUT (0xf1);
      OUT (0xce);
      INT;
      INT;
      ch = THIS ();
      INT;
      switch (ch)
	{
	case 0x01:
	  INT;
	  INT;
	  break;
	case 0x02:
	  INT;
	  break;
	case 0x04:
	  EXPn (external function);
	  break;
	case 0x05:
	  break;
	case 0x07:
	  INTn (line number);
	  INT;
	case 0x08:
	  break;
	case 0x0a:
	  INTn (locked register);
	  INT;
	  break;
	case 0x3f:
	  copy_till_end ();
	  break;
	case 0x3e:
	  copy_till_end ();
	  break;
	case 0x40:
	  copy_till_end ();
	  break;
	case 0x41:
	  ID;
	  break;
	}
    }

}

static void
f0_record ()
{
  /* Attribute record */
  NEXT ();
  OUT (0xf0);
  INTn (Symbol name);
  ID;
}

static void
copy_till_end ()
{
  int ch = THIS ();
  while (1)
    {
      while (ch <= 0x80)
	{
	  OUT (ch);
	  NEXT ();
	  ch = THIS ();
	}
      switch (ch)
	{
	case 0x84:
	  OUT (THIS ());
	  NEXT ();
	case 0x83:
	  OUT (THIS ());
	  NEXT ();
	case 0x82:
	  OUT (THIS ());
	  NEXT ();
	case 0x81:
	  OUT (THIS ());
	  NEXT ();
	  OUT (THIS ());
	  NEXT ();

	  ch = THIS ();
	  break;
	default:
	  return;
	}
    }

}

static void
f2_record ()
{
  NEXT ();
  OUT (0xf2);
  INT;
  NEXT ();
  OUT (0xce);
  INT;
  copy_till_end ();
}


static void block ();
static void
f8_record ()
{
  int ch;
  NEXT ();
  ch = THIS ();
  switch (ch)
    {
    case 0x01:
    case 0x02:
    case 0x03:
      /* Unique typedefs for module */
      /* GLobal typedefs  */
      /* High level module scope beginning */
      {
	struct output_buffer_struct ob;
	NEXT ();
	OUT (0xf8);
	OUT (ch);
	drop_int (&ob);
	ID;

	block ();

	NEXT ();
	fill_int (&ob);
	OUT (0xf9);
      }
      break;
    case 0x04:
      /* Global function */
      {
	struct output_buffer_struct ob;
	NEXT ();
	OUT (0xf8);
	OUT (0x04);
	drop_int (&ob);
	ID;
	INTn (stack size);
	INTn (ret val);
	EXPn (offset);

	block ();

	NEXT ();
	OUT (0xf9);
	EXPn (size of block);
	fill_int (&ob);
      }
      break;

    case 0x05:
      /* File name for source line numbers */
      {
	struct output_buffer_struct ob;
	NEXT ();
	OUT (0xf8);
	OUT (0x05);
	drop_int (&ob);
	ID;
	INTn (year);
	INTn (month);
	INTn (day);
	INTn (hour);
	INTn (monute);
	INTn (second);
	block ();
	NEXT ();
	OUT (0xf9);
	fill_int (&ob);
      }
      break;

    case 0x06:
      /* Local function */
      {
	struct output_buffer_struct ob;
	NEXT ();
	OUT (0xf8);
	OUT (0x06);
	drop_int (&ob);
	ID;
	INTn (stack size);
	INTn (type return);
	EXPn (offset);
	block ();
	NEXT ();
	OUT (0xf9);
	EXPn (size);
	fill_int (&ob);
      }
      break;

    case 0x0a:
      /* Assembler module scope beginning -*/
      {
	struct output_buffer_struct ob;

	NEXT ();
	OUT (0xf8);
	OUT (0x0a);
	drop_int (&ob);
	ID;
	ID;
	INT;
	ID;
	INT;
	INT;
	INT;
	INT;
	INT;
	INT;

	block ();

	NEXT ();
	OUT (0xf9);
	fill_int (&ob);
      }
      break;
    case 0x0b:
      {
	struct output_buffer_struct ob;
	NEXT ();
	OUT (0xf8);
	OUT (0x0b);
	drop_int (&ob);
	ID;
	INT;
	INTn (section index);
	EXPn (offset);
	INTn (stuff);

	block ();

	OUT (0xf9);
	NEXT ();
	EXPn (Size in Maus);
	fill_int (&ob);
      }
      break;
    }
}

static void
e2_record ()
{
  OUT (0xe2);
  NEXT ();
  OUT (0xce);
  NEXT ();
  INT;
  EXP;
}

static void
block ()
{
  int ch;
  while (1)
    {
      ch = THIS ();
      switch (ch)
	{
	case 0xe1:
	case 0xe5:
	  return;
	case 0xf9:
	  return;
	case 0xf0:
	  f0_record ();
	  break;
	case 0xf1:
	  f1_record ();
	  break;
	case 0xf2:
	  f2_record ();
	  break;
	case 0xf8:
	  f8_record ();
	  break;
	case 0xe2:
	  e2_record ();
	  break;

	}
    }
}



/* relocate_debug,
   moves all the debug information from the source bfd to the output
   bfd, and relocates any expressions it finds
*/

static void
relocate_debug (output, input)
     bfd *output;
     bfd *input;
{
#define IBS 400
#define OBS 400
  unsigned char input_buffer[IBS];

  input_ptr_start = input_ptr = input_buffer;
  input_ptr_end = input_buffer + IBS;
  input_bfd = input;
  /* FIXME: Check return value.  I'm not sure whether it needs to read
     the entire buffer or not.  */
  bfd_read ((PTR) input_ptr_start, 1, IBS, input);
  block ();
}

/*
  During linking, we we told about the bfds which made up our
  contents, we have a list of them. They will still be open, so go to
  the debug info in each, and copy it out, relocating it as we go.
*/

static boolean
ieee_write_debug_part (abfd)
     bfd *abfd;
{
  ieee_data_type *ieee = IEEE_DATA (abfd);
  bfd_chain_type *chain = ieee->chain_root;
  unsigned char output_buffer[OBS];
  boolean some_debug = false;
  file_ptr here = bfd_tell (abfd);

  output_ptr_start = output_ptr = output_buffer;
  output_ptr_end = output_buffer + OBS;
  output_ptr = output_buffer;
  output_bfd = abfd;

  if (chain == (bfd_chain_type *) NULL)
    {
      asection *s;

      for (s = abfd->sections; s != NULL; s = s->next)
	if ((s->flags & SEC_DEBUGGING) != 0)
	  break;
      if (s == NULL)
	{
	  ieee->w.r.debug_information_part = 0;
	  return true;
	}

      ieee->w.r.debug_information_part = here;
      if (bfd_write (s->contents, 1, s->_raw_size, abfd) != s->_raw_size)
	return false;
    }
  else
    {
      while (chain != (bfd_chain_type *) NULL)
	{
	  bfd *entry = chain->this;
	  ieee_data_type *entry_ieee = IEEE_DATA (entry);
	  if (entry_ieee->w.r.debug_information_part)
	    {
	      if (bfd_seek (entry, entry_ieee->w.r.debug_information_part,
			    SEEK_SET)
		  != 0)
		return false;
	      relocate_debug (abfd, entry);
	    }

	  chain = chain->next;
	}
      if (some_debug)
	{
	  ieee->w.r.debug_information_part = here;
	}
      else
	{
	  ieee->w.r.debug_information_part = 0;
	}

      flush ();
    }

  return true;
}

/* Write the data in an ieee way.  */

static boolean
ieee_write_data_part (abfd)
     bfd *abfd;
{
  asection *s;
  ieee_data_type *ieee = IEEE_DATA (abfd);
  ieee->w.r.data_part = bfd_tell (abfd);
  for (s = abfd->sections; s != (asection *) NULL; s = s->next)
    {
      /* Skip sections that have no loadable contents (.bss,
         debugging, etc.)  */
      if ((s->flags & SEC_LOAD) == 0)
	continue;

      /* Sort the reloc records so we can insert them in the correct
	 places */
      if (s->reloc_count != 0)
	{
	  if (! do_with_relocs (abfd, s))
	    return false;
	}
      else
	{
	  if (! do_without_relocs (abfd, s))
	    return false;
	}
    }

  return true;
}


static boolean
init_for_output (abfd)
     bfd *abfd;
{
  asection *s;
  for (s = abfd->sections; s != (asection *) NULL; s = s->next)
    {
      if ((s->flags & SEC_DEBUGGING) != 0)
	continue;
      if (s->_raw_size != 0)
	{
	  ieee_per_section (s)->data = (bfd_byte *) (bfd_alloc (abfd, s->_raw_size));
	  if (!ieee_per_section (s)->data)
	    return false;
	}
    }
  return true;
}

/** exec and core file sections */

/* set section contents is complicated with IEEE since the format is
* not a byte image, but a record stream.
*/
boolean
ieee_set_section_contents (abfd, section, location, offset, count)
     bfd *abfd;
     sec_ptr section;
     PTR location;
     file_ptr offset;
     bfd_size_type count;
{
  if ((section->flags & SEC_DEBUGGING) != 0)
    {
      if (section->contents == NULL)
	{
	  section->contents = bfd_alloc (abfd, section->_raw_size);
	  if (section->contents == NULL)
	    return false;
	}
      /* bfd_set_section_contents has already checked that everything
         is within range.  */
      memcpy (section->contents + offset, location, count);
      return true;
    }

  if (ieee_per_section (section)->data == (bfd_byte *) NULL)
    {
      if (!init_for_output (abfd))
	return false;
    }
  memcpy ((PTR) (ieee_per_section (section)->data + offset),
	  (PTR) location,
	  (unsigned int) count);
  return true;
}

/* Write the external symbols of a file.  IEEE considers two sorts of
   external symbols, public, and referenced.  It uses to internal
   forms to index them as well.  When we write them out we turn their
   symbol values into indexes from the right base.  */

static boolean
ieee_write_external_part (abfd)
     bfd *abfd;
{
  asymbol **q;
  ieee_data_type *ieee = IEEE_DATA (abfd);

  unsigned int reference_index = IEEE_REFERENCE_BASE;
  unsigned int public_index = IEEE_PUBLIC_BASE + 2;
  file_ptr here = bfd_tell (abfd);
  boolean hadone = false;
  if (abfd->outsymbols != (asymbol **) NULL)
    {

      for (q = abfd->outsymbols; *q != (asymbol *) NULL; q++)
	{
	  asymbol *p = *q;
	  hadone = true;
	  if (bfd_is_und_section (p->section))
	    {
	      /* This must be a symbol reference .. */
	      if (! ieee_write_byte (abfd, ieee_external_reference_enum)
		  || ! ieee_write_int (abfd, reference_index)
		  || ! ieee_write_id (abfd, p->name))
		return false;
	      p->value = reference_index;
	      reference_index++;
	    }
	  else if (bfd_is_com_section (p->section))
	    {
	      /* This is a weak reference */
	      if (! ieee_write_byte (abfd, ieee_external_reference_enum)
		  || ! ieee_write_int (abfd, reference_index)
		  || ! ieee_write_id (abfd, p->name)
		  || ! ieee_write_byte (abfd,
					ieee_weak_external_reference_enum)
		  || ! ieee_write_int (abfd, reference_index)
		  || ! ieee_write_int (abfd, p->value))
		return false;
	      p->value = reference_index;
	      reference_index++;
	    }
	  else if (p->flags & BSF_GLOBAL)
	    {
	      /* This must be a symbol definition */

	      if (! ieee_write_byte (abfd, ieee_external_symbol_enum)
		  || ! ieee_write_int (abfd, public_index)
		  || ! ieee_write_id (abfd, p->name)
		  || ! ieee_write_2bytes (abfd, ieee_attribute_record_enum)
		  || ! ieee_write_int (abfd, public_index)
		  || ! ieee_write_byte (abfd, 15) /* instruction address */
		  || ! ieee_write_byte (abfd, 19) /* static symbol */
		  || ! ieee_write_byte (abfd, 1)) /* one of them */
		return false;

	      /* Write out the value */
	      if (! ieee_write_2bytes (abfd, ieee_value_record_enum)
		  || ! ieee_write_int (abfd, public_index))
		return false;
	      if (! bfd_is_abs_section (p->section))
		{
		  if (abfd->flags & EXEC_P)
		    {
		      /* If fully linked, then output all symbols
			 relocated */
		      if (! (ieee_write_int
			     (abfd,
			      (p->value
			       + p->section->output_offset
			       + p->section->output_section->vma))))
			return false;
		    }
		  else
		    {
		      if (! (ieee_write_expression
			     (abfd,
			      p->value + p->section->output_offset,
			      p->section->output_section->symbol,
			      false, 0)))
			return false;
		    }
		}
	      else
		{
		  if (! ieee_write_expression (abfd,
					       p->value,
					       bfd_abs_section_ptr->symbol,
					       false, 0))
		    return false;
		}
	      p->value = public_index;
	      public_index++;
	    }
	  else
	    {
	      /* This can happen - when there are gaps in the symbols read */
	      /* from an input ieee file */
	    }
	}
    }
  if (hadone)
    ieee->w.r.external_part = here;

  return true;
}


static CONST unsigned char exten[] =
{
  0xf0, 0x20, 0x00,
  0xf1, 0xce, 0x20, 0x00, 37, 3, 3,	/* Set version 3 rev 3   	*/
  0xf1, 0xce, 0x20, 0x00, 39, 2,/* keep symbol in  original case */
  0xf1, 0xce, 0x20, 0x00, 38	/* set object type relocateable to x */
};

static CONST unsigned char envi[] =
{
  0xf0, 0x21, 0x00,

/*    0xf1, 0xce, 0x21, 00, 50, 0x82, 0x07, 0xc7, 0x09, 0x11, 0x11,
    0x19, 0x2c,
*/
  0xf1, 0xce, 0x21, 00, 52, 0x00,	/* exec ok */

  0xf1, 0xce, 0x21, 0, 53, 0x03,/* host unix */
/*    0xf1, 0xce, 0x21, 0, 54, 2,1,1	tool & version # */
};

static boolean
ieee_write_me_part (abfd)
     bfd *abfd;
{
  ieee_data_type *ieee = IEEE_DATA (abfd);
  ieee->w.r.trailer_part = bfd_tell (abfd);
  if (abfd->start_address)
    {
      if (! ieee_write_2bytes (abfd, ieee_value_starting_address_enum)
	  || ! ieee_write_byte (abfd, ieee_function_either_open_b_enum)
	  || ! ieee_write_int (abfd, abfd->start_address)
	  || ! ieee_write_byte (abfd, ieee_function_either_close_b_enum))
	return false;
    }
  ieee->w.r.me_record = bfd_tell (abfd);
  if (! ieee_write_byte (abfd, ieee_module_end_enum))
    return false;
  return true;
}

/* Write out the IEEE processor ID.  */

static boolean
ieee_write_processor (abfd)
     bfd *abfd;
{
  const bfd_arch_info_type *arch;

  arch = bfd_get_arch_info (abfd);
  switch (arch->arch)
    {
    default:
      if (! ieee_write_id (abfd, bfd_printable_name (abfd)))
	return false;
      break;

    case bfd_arch_a29k:
      if (! ieee_write_id (abfd, "29000"))
	return false;
      break;

    case bfd_arch_h8300:
      if (! ieee_write_id (abfd, "H8/300"))
	return false;
      break;

    case bfd_arch_h8500:
      if (! ieee_write_id (abfd, "H8/500"))
	return false;
      break;

    case bfd_arch_i960:
      switch (arch->mach)
	{
	default:
	case bfd_mach_i960_core:
	case bfd_mach_i960_ka_sa:
	  if (! ieee_write_id (abfd, "80960KA"))
	    return false;
	  break;

	case bfd_mach_i960_kb_sb:
	  if (! ieee_write_id (abfd, "80960KB"))
	    return false;
	  break;

	case bfd_mach_i960_ca:
	  if (! ieee_write_id (abfd, "80960CA"))
	    return false;
	  break;

	case bfd_mach_i960_mc:
	case bfd_mach_i960_xa:
	  if (! ieee_write_id (abfd, "80960MC"))
	    return false;
	  break;
	}
      break;

    case bfd_arch_m68k:
      {
	char ab[20];

	sprintf (ab, "%lu", arch->mach);
	if (! ieee_write_id (abfd, ab))
	  return false;
      }
      break;
    }

  return true;
}

boolean
ieee_write_object_contents (abfd)
     bfd *abfd;
{
  ieee_data_type *ieee = IEEE_DATA (abfd);
  unsigned int i;
  file_ptr old;

  /* Fast forward over the header area */
  if (bfd_seek (abfd, (file_ptr) 0, SEEK_SET) != 0)
    return false;

  if (! ieee_write_byte (abfd, ieee_module_beginning_enum)
      || ! ieee_write_processor (abfd)
      || ! ieee_write_id (abfd, abfd->filename))
    return false;

  /* Fast forward over the variable bits */
  if (! ieee_write_byte (abfd, ieee_address_descriptor_enum))
    return false;

  /* Bits per MAU */
  if (! ieee_write_byte (abfd, (bfd_byte) (bfd_arch_bits_per_byte (abfd))))
    return false;
  /* MAU's per address */
  if (! ieee_write_byte (abfd,
			 (bfd_byte) (bfd_arch_bits_per_address (abfd)
				     / bfd_arch_bits_per_byte (abfd))))
    return false;

  old = bfd_tell (abfd);
  if (bfd_seek (abfd, (file_ptr) (8 * N_W_VARIABLES), SEEK_CUR) != 0)
    return false;

  ieee->w.r.extension_record = bfd_tell (abfd);
  if (bfd_write ((char *) exten, 1, sizeof (exten), abfd) != sizeof (exten))
    return false;
  if (abfd->flags & EXEC_P)
    {
      if (! ieee_write_byte (abfd, 0x1)) /* Absolute */
	return false;
    }
  else
    {
      if (! ieee_write_byte (abfd, 0x2)) /* Relocateable */
	return false;
    }

  ieee->w.r.environmental_record = bfd_tell (abfd);
  if (bfd_write ((char *) envi, 1, sizeof (envi), abfd) != sizeof (envi))
    return false;
  output_bfd = abfd;

  flush ();

  if (! ieee_write_section_part (abfd))
    return false;
  /* First write the symbols.  This changes their values into table
    indeces so we cant use it after this point.  */
  if (! ieee_write_external_part (abfd))
    return false;

  /*  ieee_write_byte(abfd, ieee_record_seperator_enum);*/

  /*  ieee_write_byte(abfd, ieee_record_seperator_enum);*/


  /* Write any debugs we have been told about.  */
  if (! ieee_write_debug_part (abfd))
    return false;

  /* Can only write the data once the symbols have been written, since
     the data contains relocation information which points to the
     symbols.  */
  if (! ieee_write_data_part (abfd))
    return false;

  /* At the end we put the end!  */
  if (! ieee_write_me_part (abfd))
    return false;

  /* Generate the header */
  if (bfd_seek (abfd, old, SEEK_SET) != 0)
    return false;

  for (i = 0; i < N_W_VARIABLES; i++)
    {
      if (! ieee_write_2bytes (abfd, ieee_assign_value_to_variable_enum)
	  || ! ieee_write_byte (abfd, (bfd_byte) i)
	  || ! ieee_write_int5_out (abfd, ieee->w.offset[i]))
	return false;
    }

  return true;
}

/* Native-level interface to symbols. */

/* We read the symbols into a buffer, which is discarded when this
   function exits.  We read the strings into a buffer large enough to
   hold them all plus all the cached symbol entries. */

asymbol *
ieee_make_empty_symbol (abfd)
     bfd *abfd;
{
  ieee_symbol_type *new =
    (ieee_symbol_type *) bfd_zmalloc (sizeof (ieee_symbol_type));
  if (!new)
    return NULL;
  new->symbol.the_bfd = abfd;
  return &new->symbol;
}

static bfd *
ieee_openr_next_archived_file (arch, prev)
     bfd *arch;
     bfd *prev;
{
  ieee_ar_data_type *ar = IEEE_AR_DATA (arch);
  /* take the next one from the arch state, or reset */
  if (prev == (bfd *) NULL)
    {
      /* Reset the index - the first two entries are bogus*/
      ar->element_index = 2;
    }
  while (true)
    {
      ieee_ar_obstack_type *p = ar->elements + ar->element_index;
      ar->element_index++;
      if (ar->element_index <= ar->element_count)
	{
	  if (p->file_offset != (file_ptr) 0)
	    {
	      if (p->abfd == (bfd *) NULL)
		{
		  p->abfd = _bfd_create_empty_archive_element_shell (arch);
		  p->abfd->origin = p->file_offset;
		}
	      return p->abfd;
	    }
	}
      else
	{
	  bfd_set_error (bfd_error_no_more_archived_files);
	  return (bfd *) NULL;
	}

    }
}

static boolean
ieee_find_nearest_line (abfd,
			section,
			symbols,
			offset,
			filename_ptr,
			functionname_ptr,
			line_ptr)
     bfd *abfd;
     asection *section;
     asymbol **symbols;
     bfd_vma offset;
     char **filename_ptr;
     char **functionname_ptr;
     int *line_ptr;
{
  return false;
}

static int
ieee_generic_stat_arch_elt (abfd, buf)
     bfd *abfd;
     struct stat *buf;
{
  ieee_ar_data_type *ar = abfd->my_archive->tdata.ieee_ar_data;
  if (ar == (ieee_ar_data_type *) NULL)
    {
      bfd_set_error (bfd_error_invalid_operation);
      return -1;
    }
  else if (ieee_object_p (abfd))
    {
      ieee_data_type *ieee = IEEE_DATA (abfd);

      buf->st_size = ieee->w.r.me_record + 1;
      buf->st_mode = 0644;
      return 0;
    }
  else
    return -1;
}

static int
ieee_sizeof_headers (abfd, x)
     bfd *abfd;
     boolean x;
{
  return 0;
}


/* The debug info routines are never used.  */
#if 0

static void
ieee_bfd_debug_info_start (abfd)
     bfd *abfd;
{

}

static void
ieee_bfd_debug_info_end (abfd)
     bfd *abfd;
{

}


/* Add this section to the list of sections we have debug info for, to
   be ready to output it at close time
   */
static void
ieee_bfd_debug_info_accumulate (abfd, section)
     bfd *abfd;
     asection *section;
{
  ieee_data_type *ieee = IEEE_DATA (section->owner);
  ieee_data_type *output_ieee = IEEE_DATA (abfd);
  /* can only accumulate data from other ieee bfds */
  if (section->owner->xvec != abfd->xvec)
    return;
  /* Only bother once per bfd */
  if (ieee->done_debug == true)
    return;
  ieee->done_debug = true;

  /* Don't bother if there is no debug info */
  if (ieee->w.r.debug_information_part == 0)
    return;


  /* Add to chain */
  {
    bfd_chain_type *n = (bfd_chain_type *) bfd_alloc (abfd, sizeof (bfd_chain_type));
    if (!n)
      abort ();		/* FIXME */
    n->this = section->owner;
    n->next = (bfd_chain_type *) NULL;

    if (output_ieee->chain_head)
      {
	output_ieee->chain_head->next = n;
      }
    else
      {
	output_ieee->chain_root = n;

      }
    output_ieee->chain_head = n;
  }
}

#endif

#define	ieee_close_and_cleanup _bfd_generic_close_and_cleanup
#define ieee_bfd_free_cached_info _bfd_generic_bfd_free_cached_info

#define ieee_slurp_armap bfd_true
#define ieee_slurp_extended_name_table bfd_true
#define ieee_construct_extended_name_table \
  ((boolean (*) PARAMS ((bfd *, char **, bfd_size_type *, const char **))) \
   bfd_true)
#define ieee_truncate_arname bfd_dont_truncate_arname
#define ieee_write_armap \
  ((boolean (*) \
    PARAMS ((bfd *, unsigned int, struct orl *, unsigned int, int))) \
   bfd_true)
#define ieee_read_ar_hdr bfd_nullvoidptr
#define ieee_update_armap_timestamp bfd_true
#define ieee_get_elt_at_index _bfd_generic_get_elt_at_index

#define ieee_bfd_is_local_label bfd_generic_is_local_label
#define ieee_get_lineno _bfd_nosymbols_get_lineno
#define ieee_bfd_make_debug_symbol _bfd_nosymbols_bfd_make_debug_symbol
#define ieee_read_minisymbols _bfd_generic_read_minisymbols
#define ieee_minisymbol_to_symbol _bfd_generic_minisymbol_to_symbol

#define ieee_bfd_reloc_type_lookup _bfd_norelocs_bfd_reloc_type_lookup

#define ieee_set_arch_mach _bfd_generic_set_arch_mach

#define ieee_get_section_contents_in_window \
  _bfd_generic_get_section_contents_in_window
#define ieee_bfd_get_relocated_section_contents \
  bfd_generic_get_relocated_section_contents
#define ieee_bfd_relax_section bfd_generic_relax_section
#define ieee_bfd_link_hash_table_create _bfd_generic_link_hash_table_create
#define ieee_bfd_link_add_symbols _bfd_generic_link_add_symbols
#define ieee_bfd_final_link _bfd_generic_final_link
#define ieee_bfd_link_split_section  _bfd_generic_link_split_section

/*SUPPRESS 460 */
const bfd_target ieee_vec =
{
  "ieee",			/* name */
  bfd_target_ieee_flavour,
  BFD_ENDIAN_UNKNOWN,		/* target byte order */
  BFD_ENDIAN_UNKNOWN,		/* target headers byte order */
  (HAS_RELOC | EXEC_P |		/* object flags */
   HAS_LINENO | HAS_DEBUG |
   HAS_SYMS | HAS_LOCALS | WP_TEXT | D_PAGED),
  (SEC_CODE | SEC_DATA | SEC_ROM | SEC_HAS_CONTENTS
   | SEC_ALLOC | SEC_LOAD | SEC_RELOC),	/* section flags */
  0,				/* leading underscore */
  ' ',				/* ar_pad_char */
  16,				/* ar_max_namelen */
  bfd_getb64, bfd_getb_signed_64, bfd_putb64,
  bfd_getb32, bfd_getb_signed_32, bfd_putb32,
  bfd_getb16, bfd_getb_signed_16, bfd_putb16,	/* data */
  bfd_getb64, bfd_getb_signed_64, bfd_putb64,
  bfd_getb32, bfd_getb_signed_32, bfd_putb32,
  bfd_getb16, bfd_getb_signed_16, bfd_putb16,	/* hdrs */

  {_bfd_dummy_target,
   ieee_object_p,		/* bfd_check_format */
   ieee_archive_p,
   _bfd_dummy_target,
  },
  {
    bfd_false,
    ieee_mkobject,
    _bfd_generic_mkarchive,
    bfd_false
  },
  {
    bfd_false,
    ieee_write_object_contents,
    _bfd_write_archive_contents,
    bfd_false,
  },

  BFD_JUMP_TABLE_GENERIC (ieee),
  BFD_JUMP_TABLE_COPY (_bfd_generic),
  BFD_JUMP_TABLE_CORE (_bfd_nocore),
  BFD_JUMP_TABLE_ARCHIVE (ieee),
  BFD_JUMP_TABLE_SYMBOLS (ieee),
  BFD_JUMP_TABLE_RELOCS (ieee),
  BFD_JUMP_TABLE_WRITE (ieee),
  BFD_JUMP_TABLE_LINK (ieee),
  BFD_JUMP_TABLE_DYNAMIC (_bfd_nodynamic),

  (PTR) 0
};
