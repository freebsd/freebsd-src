/* Print National Semiconductor 32000 instructions.
   Copyright 1986, 1988, 1991, 1992, 1994, 1998, 2001, 2002, 2005
   Free Software Foundation, Inc.

   This file is part of opcodes library.

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
   Foundation, Inc., 51 Franklin Street - Fifth Floor, Boston,
   MA 02110-1301, USA.  */


#include "bfd.h"
#include "sysdep.h"
#include "dis-asm.h"
#if !defined(const) && !defined(__STDC__)
#define const
#endif
#include "opcode/ns32k.h"
#include "opintl.h"

static disassemble_info *dis_info;

/* Hacks to get it to compile <= READ THESE AS FIXES NEEDED.  */
#define INVALID_FLOAT(val, size) invalid_float ((bfd_byte *) val, size)

static long
read_memory_integer (unsigned char * addr, int nr)
{
  long val;
  int i;

  for (val = 0, i = nr - 1; i >= 0; i--)
    {
      val =  (val << 8);
      val |= (0xff & *(addr + i));
    }
  return val;
}

/* 32000 instructions are never longer than this.  */
#define MAXLEN 62

#include <setjmp.h>

struct private
{
  /* Points to first byte not fetched.  */
  bfd_byte *max_fetched;
  bfd_byte the_buffer[MAXLEN];
  bfd_vma insn_start;
  jmp_buf bailout;
};


/* Make sure that bytes from INFO->PRIVATE_DATA->BUFFER (inclusive)
   to ADDR (exclusive) are valid.  Returns 1 for success, longjmps
   on error.  */
#define FETCH_DATA(info, addr) \
  ((addr) <= ((struct private *)(info->private_data))->max_fetched \
   ? 1 : fetch_data ((info), (addr)))

static int
fetch_data (struct disassemble_info *info, bfd_byte *addr)
{
  int status;
  struct private *priv = (struct private *) info->private_data;
  bfd_vma start = priv->insn_start + (priv->max_fetched - priv->the_buffer);

  status = (*info->read_memory_func) (start,
				      priv->max_fetched,
				      addr - priv->max_fetched,
				      info);
  if (status != 0)
    {
      (*info->memory_error_func) (status, start, info);
      longjmp (priv->bailout, 1);
    }
  else
    priv->max_fetched = addr;
  return 1;
}

/* Number of elements in the opcode table.  */
#define NOPCODES (sizeof ns32k_opcodes / sizeof ns32k_opcodes[0])

#define NEXT_IS_ADDR	'|'


struct ns32k_option
{
  char *pattern;		/* The option itself.  */
  unsigned long value;		/* Binary value of the option.  */
  unsigned long match;		/* These bits must match.  */
};


static const struct ns32k_option opt_u[]= /* Restore, exit.  */
{
  { "r0",	0x80,	0x80	},
  { "r1",	0x40,	0x40	},
  { "r2",	0x20,	0x20	},
  { "r3",	0x10,	0x10	},
  { "r4",	0x08,	0x08	},
  { "r5",	0x04,	0x04	},
  { "r6",	0x02,	0x02	},
  { "r7",	0x01,	0x01	},
  {  0 ,	0x00,	0x00	}
};

static const struct ns32k_option opt_U[]= /* Save, enter.  */
{
  { "r0",	0x01,	0x01	},
  { "r1",	0x02,	0x02	},
  { "r2",	0x04,	0x04	},
  { "r3",	0x08,	0x08	},
  { "r4",	0x10,	0x10	},
  { "r5",	0x20,	0x20	},
  { "r6",	0x40,	0x40	},
  { "r7",	0x80,	0x80	},
  {  0 ,	0x00,	0x00	}
};

static const struct ns32k_option opt_O[]= /* Setcfg.  */
{
  { "c",	0x8,	0x8	},
  { "m",	0x4,	0x4	},
  { "f",	0x2,	0x2	},
  { "i",	0x1,	0x1	},
  {  0 ,	0x0,	0x0	}
};

static const struct ns32k_option opt_C[]= /* Cinv.  */
{
  { "a",	0x4,	0x4	},
  { "i",	0x2,	0x2	},
  { "d",	0x1,	0x1	},
  {  0 ,	0x0,	0x0	}
};

static const struct ns32k_option opt_S[]= /* String inst.  */
{
  { "b",	0x1,	0x1	},
  { "u",	0x6,	0x6	},
  { "w",	0x2,	0x2	},
  {  0 ,	0x0,	0x0	}
};

static const struct ns32k_option list_P532[]= /* Lpr spr.  */
{
  { "us",	0x0,	0xf	},
  { "dcr",	0x1,	0xf	},
  { "bpc",	0x2,	0xf	},
  { "dsr",	0x3,	0xf	},
  { "car",	0x4,	0xf	},
  { "fp",	0x8,	0xf	},
  { "sp",	0x9,	0xf	},
  { "sb",	0xa,	0xf	},
  { "usp",	0xb,	0xf	},
  { "cfg",	0xc,	0xf	},
  { "psr",	0xd,	0xf	},
  { "intbase",	0xe,	0xf	},
  { "mod",	0xf,	0xf	},
  {  0 ,	0x00,	0xf	}
};

static const struct ns32k_option list_M532[]= /* Lmr smr.  */
{
  { "mcr",	0x9,	0xf	},
  { "msr",	0xa,	0xf	},
  { "tear",	0xb,	0xf	},
  { "ptb0",	0xc,	0xf	},
  { "ptb1",	0xd,	0xf	},
  { "ivar0",	0xe,	0xf	},
  { "ivar1",	0xf,	0xf	},
  {  0 ,	0x0,	0xf	}
};

static const struct ns32k_option list_P032[]= /* Lpr spr.  */
{
  { "upsr",	0x0,	0xf	},
  { "fp",	0x8,	0xf	},
  { "sp",	0x9,	0xf	},
  { "sb",	0xa,	0xf	},
  { "psr",	0xb,	0xf	},
  { "intbase",	0xe,	0xf	},
  { "mod",	0xf,	0xf	},
  {  0 ,	0x0,	0xf	}
};

static const struct ns32k_option list_M032[]= /* Lmr smr.  */
{
  { "bpr0",	0x0,	0xf	},
  { "bpr1",	0x1,	0xf	},
  { "pf0",	0x4,	0xf	},
  { "pf1",	0x5,	0xf	},
  { "sc",	0x8,	0xf	},
  { "msr",	0xa,	0xf	},
  { "bcnt",	0xb,	0xf	},
  { "ptb0",	0xc,	0xf	},
  { "ptb1",	0xd,	0xf	},
  { "eia",	0xf,	0xf	},
  {  0 ,	0x0,	0xf	}
};


/* Figure out which options are present.   */

static void
optlist (int options, const struct ns32k_option * optionP, char * result)
{
  if (options == 0)
    {
      sprintf (result, "[]");
      return;
    }

  sprintf (result, "[");

  for (; (options != 0) && optionP->pattern; optionP++)
    {
      if ((options & optionP->match) == optionP->value)
	{
	  /* We found a match, update result and options.  */
	  strcat (result, optionP->pattern);
	  options &= ~optionP->value;
	  if (options != 0)	/* More options to come.  */
	    strcat (result, ",");
	}
    }

  if (options != 0)
    strcat (result, "undefined");

  strcat (result, "]");
}

static void
list_search (int reg_value, const struct ns32k_option *optionP, char *result)
{
  for (; optionP->pattern; optionP++)
    {
      if ((reg_value & optionP->match) == optionP->value)
	{
	  sprintf (result, "%s", optionP->pattern);
	  return;
	}
    }
  sprintf (result, "undefined");
}

/* Extract "count" bits starting "offset" bits into buffer.  */

static int
bit_extract (bfd_byte *buffer, int offset, int count)
{
  int result;
  int bit;

  buffer += offset >> 3;
  offset &= 7;
  bit = 1;
  result = 0;
  while (count--)
    {
      FETCH_DATA (dis_info, buffer + 1);
      if ((*buffer & (1 << offset)))
	result |= bit;
      if (++offset == 8)
	{
	  offset = 0;
	  buffer++;
	}
      bit <<= 1;
    }
  return result;
}

/* Like bit extract but the buffer is valid and doen't need to be fetched.  */

static int
bit_extract_simple (bfd_byte *buffer, int offset, int count)
{
  int result;
  int bit;

  buffer += offset >> 3;
  offset &= 7;
  bit = 1;
  result = 0;
  while (count--)
    {
      if ((*buffer & (1 << offset)))
	result |= bit;
      if (++offset == 8)
	{
	  offset = 0;
	  buffer++;
	}
      bit <<= 1;
    }
  return result;
}

static void
bit_copy (bfd_byte *buffer, int offset, int count, char *to)
{
  for (; count > 8; count -= 8, to++, offset += 8)
    *to = bit_extract (buffer, offset, 8);
  *to = bit_extract (buffer, offset, count);
}

static int
sign_extend (int value, int bits)
{
  value = value & ((1 << bits) - 1);
  return (value & (1 << (bits - 1))
	  ? value | (~((1 << bits) - 1))
	  : value);
}

static void
flip_bytes (char *ptr, int count)
{
  char tmp;

  while (count > 0)
    {
      tmp = ptr[0];
      ptr[0] = ptr[count - 1];
      ptr[count - 1] = tmp;
      ptr++;
      count -= 2;
    }
}

/* Given a character C, does it represent a general addressing mode?  */
#define Is_gen(c) \
  ((c) == 'F' || (c) == 'L' || (c) == 'B' \
   || (c) == 'W' || (c) == 'D' || (c) == 'A' || (c) == 'I' || (c) == 'Z')

/* Adressing modes.  */
#define Adrmod_index_byte        0x1c
#define Adrmod_index_word        0x1d
#define Adrmod_index_doubleword  0x1e
#define Adrmod_index_quadword    0x1f

/* Is MODE an indexed addressing mode?  */
#define Adrmod_is_index(mode) \
  (   mode == Adrmod_index_byte \
   || mode == Adrmod_index_word \
   || mode == Adrmod_index_doubleword \
   || mode == Adrmod_index_quadword)


static int
get_displacement (bfd_byte *buffer, int *aoffsetp)
{
  int Ivalue;
  short Ivalue2;

  Ivalue = bit_extract (buffer, *aoffsetp, 8);
  switch (Ivalue & 0xc0)
    {
    case 0x00:
    case 0x40:
      Ivalue = sign_extend (Ivalue, 7);
      *aoffsetp += 8;
      break;
    case 0x80:
      Ivalue2 = bit_extract (buffer, *aoffsetp, 16);
      flip_bytes ((char *) & Ivalue2, 2);
      Ivalue = sign_extend (Ivalue2, 14);
      *aoffsetp += 16;
      break;
    case 0xc0:
      Ivalue = bit_extract (buffer, *aoffsetp, 32);
      flip_bytes ((char *) & Ivalue, 4);
      Ivalue = sign_extend (Ivalue, 30);
      *aoffsetp += 32;
      break;
    }
  return Ivalue;
}

#if 1 /* A version that should work on ns32k f's&d's on any machine.  */
static int
invalid_float (bfd_byte *p, int len)
{
  int val;

  if (len == 4)
    val = (bit_extract_simple (p, 23, 8)/*exponent*/ == 0xff
	   || (bit_extract_simple (p, 23, 8)/*exponent*/ == 0
	       && bit_extract_simple (p, 0, 23)/*mantisa*/ != 0));
  else if (len == 8)
    val = (bit_extract_simple (p, 52, 11)/*exponent*/ == 0x7ff
	   || (bit_extract_simple (p, 52, 11)/*exponent*/ == 0
	       && (bit_extract_simple (p, 0, 32)/*low mantisa*/ != 0
		   || bit_extract_simple (p, 32, 20)/*high mantisa*/ != 0)));
  else
    val = 1;
  return (val);
}
#else
/* Assumes the bytes have been swapped to local order.  */
typedef union
{ 
  double d;
  float f;
  struct { unsigned m:23, e:8, :1;} sf;
  struct { unsigned lm; unsigned m:20, e:11, :1;} sd;
} float_type_u;

static int
invalid_float (float_type_u *p, int len)
{
  int val;

  if (len == sizeof (float))
    val = (p->sf.e == 0xff
	   || (p->sf.e == 0 && p->sf.m != 0));
  else if (len == sizeof (double))
    val = (p->sd.e == 0x7ff
	   || (p->sd.e == 0 && (p->sd.m != 0 || p->sd.lm != 0)));
  else
    val = 1;
  return val;
}
#endif

/* Print an instruction operand of category given by d.  IOFFSET is
   the bit position below which small (<1 byte) parts of the operand can
   be found (usually in the basic instruction, but for indexed
   addressing it can be in the index byte).  AOFFSETP is a pointer to the
   bit position of the addressing extension.  BUFFER contains the
   instruction.  ADDR is where BUFFER was read from.  Put the disassembled
   version of the operand in RESULT.  INDEX_OFFSET is the bit position
   of the index byte (it contains garbage if this operand is not a
   general operand using scaled indexed addressing mode).  */

static int
print_insn_arg (int d,
		int ioffset,
		int *aoffsetp,
		bfd_byte *buffer,
		bfd_vma addr,
		char *result,
		int index_offset)
{
  union
  {
    float f;
    double d;
    int i[2];
  } value;
  int Ivalue;
  int addr_mode;
  int disp1, disp2;
  int index;
  int size;

  switch (d)
    {
    case 'f':
      /* A "gen" operand but 5 bits from the end of instruction.  */
      ioffset -= 5;
    case 'Z':
    case 'F':
    case 'L':
    case 'I':
    case 'B':
    case 'W':
    case 'D':
    case 'A':
      addr_mode = bit_extract (buffer, ioffset - 5, 5);
      ioffset -= 5;
      switch (addr_mode)
	{
	case 0x0: case 0x1: case 0x2: case 0x3:
	case 0x4: case 0x5: case 0x6: case 0x7:
	  /* Register mode R0 -- R7.  */
	  switch (d)
	    {
	    case 'F':
	    case 'L':
	    case 'Z':
	      sprintf (result, "f%d", addr_mode);
	      break;
	    default:
	      sprintf (result, "r%d", addr_mode);
	    }
	  break;
	case 0x8: case 0x9: case 0xa: case 0xb:
	case 0xc: case 0xd: case 0xe: case 0xf:
	  /* Register relative disp(R0 -- R7).  */
	  disp1 = get_displacement (buffer, aoffsetp);
	  sprintf (result, "%d(r%d)", disp1, addr_mode & 7);
	  break;
	case 0x10:
	case 0x11:
	case 0x12:
	  /* Memory relative disp2(disp1(FP, SP, SB)).  */
	  disp1 = get_displacement (buffer, aoffsetp);
	  disp2 = get_displacement (buffer, aoffsetp);
	  sprintf (result, "%d(%d(%s))", disp2, disp1,
		   addr_mode == 0x10 ? "fp" : addr_mode == 0x11 ? "sp" : "sb");
	  break;
	case 0x13:
	  /* Reserved.  */
	  sprintf (result, "reserved");
	  break;
	case 0x14:
	  /* Immediate.  */
	  switch (d)
	    {
	    case 'I':
	    case 'Z':
	    case 'A':
	      /* I and Z are output operands and can`t be immediate
	         A is an address and we can`t have the address of
	         an immediate either. We don't know how much to increase
	         aoffsetp by since whatever generated this is broken
	         anyway!  */
	      sprintf (result, _("$<undefined>"));
	      break;
	    case 'B':
	      Ivalue = bit_extract (buffer, *aoffsetp, 8);
	      Ivalue = sign_extend (Ivalue, 8);
	      *aoffsetp += 8;
	      sprintf (result, "$%d", Ivalue);
	      break;
	    case 'W':
	      Ivalue = bit_extract (buffer, *aoffsetp, 16);
	      flip_bytes ((char *) & Ivalue, 2);
	      *aoffsetp += 16;
	      Ivalue = sign_extend (Ivalue, 16);
	      sprintf (result, "$%d", Ivalue);
	      break;
	    case 'D':
	      Ivalue = bit_extract (buffer, *aoffsetp, 32);
	      flip_bytes ((char *) & Ivalue, 4);
	      *aoffsetp += 32;
	      sprintf (result, "$%d", Ivalue);
	      break;
	    case 'F':
	      bit_copy (buffer, *aoffsetp, 32, (char *) &value.f);
	      flip_bytes ((char *) &value.f, 4);
	      *aoffsetp += 32;
	      if (INVALID_FLOAT (&value.f, 4))
		sprintf (result, "<<invalid float 0x%.8x>>", value.i[0]);
	      else /* Assume host has ieee float.  */
		sprintf (result, "$%g", value.f);
	      break;
	    case 'L':
	      bit_copy (buffer, *aoffsetp, 64, (char *) &value.d);
	      flip_bytes ((char *) &value.d, 8);
	      *aoffsetp += 64;
	      if (INVALID_FLOAT (&value.d, 8))
		sprintf (result, "<<invalid double 0x%.8x%.8x>>",
			 value.i[1], value.i[0]);
	      else /* Assume host has ieee float.  */
		sprintf (result, "$%g", value.d);
	      break;
	    }
	  break;
	case 0x15:
	  /* Absolute @disp.  */
	  disp1 = get_displacement (buffer, aoffsetp);
	  sprintf (result, "@|%d|", disp1);
	  break;
	case 0x16:
	  /* External EXT(disp1) + disp2 (Mod table stuff).  */
	  disp1 = get_displacement (buffer, aoffsetp);
	  disp2 = get_displacement (buffer, aoffsetp);
	  sprintf (result, "EXT(%d) + %d", disp1, disp2);
	  break;
	case 0x17:
	  /* Top of stack tos.  */
	  sprintf (result, "tos");
	  break;
	case 0x18:
	  /* Memory space disp(FP).  */
	  disp1 = get_displacement (buffer, aoffsetp);
	  sprintf (result, "%d(fp)", disp1);
	  break;
	case 0x19:
	  /* Memory space disp(SP).  */
	  disp1 = get_displacement (buffer, aoffsetp);
	  sprintf (result, "%d(sp)", disp1);
	  break;
	case 0x1a:
	  /* Memory space disp(SB).  */
	  disp1 = get_displacement (buffer, aoffsetp);
	  sprintf (result, "%d(sb)", disp1);
	  break;
	case 0x1b:
	  /* Memory space disp(PC).  */
	  disp1 = get_displacement (buffer, aoffsetp);
	  *result++ = NEXT_IS_ADDR;
	  sprintf_vma (result, addr + disp1);
	  result += strlen (result);
	  *result++ = NEXT_IS_ADDR;
	  *result = '\0';
	  break;
	case 0x1c:
	case 0x1d:
	case 0x1e:
	case 0x1f:
	  /* Scaled index basemode[R0 -- R7:B,W,D,Q].  */
	  index = bit_extract (buffer, index_offset - 8, 3);
	  print_insn_arg (d, index_offset, aoffsetp, buffer, addr,
			  result, 0);
	  {
	    static const char *ind = "bwdq";
	    char *off;

	    off = result + strlen (result);
	    sprintf (off, "[r%d:%c]", index,
		     ind[addr_mode & 3]);
	  }
	  break;
	}
      break;
    case 'H':
    case 'q':
      Ivalue = bit_extract (buffer, ioffset-4, 4);
      Ivalue = sign_extend (Ivalue, 4);
      sprintf (result, "%d", Ivalue);
      ioffset -= 4;
      break;
    case 'r':
      Ivalue = bit_extract (buffer, ioffset-3, 3);
      sprintf (result, "r%d", Ivalue&7);
      ioffset -= 3;
      break;
    case 'd':
      sprintf (result, "%d", get_displacement (buffer, aoffsetp));
      break;
    case 'b':
      Ivalue = get_displacement (buffer, aoffsetp);
      /* Warning!!  HACK ALERT!
         Operand type 'b' is only used by the cmp{b,w,d} and
         movm{b,w,d} instructions; we need to know whether
         it's a `b' or `w' or `d' instruction; and for both
         cmpm and movm it's stored at the same place so we
         just grab two bits of the opcode and look at it...  */
      size = bit_extract(buffer, ioffset-6, 2);
      if (size == 0)		/* 00 => b.  */
	size = 1;
      else if (size == 1)	/* 01 => w.  */
	size = 2;
      else
	size = 4;		/* 11 => d.  */

      sprintf (result, "%d", (Ivalue / size) + 1);
      break;
    case 'p':
      *result++ = NEXT_IS_ADDR;
      sprintf_vma (result, addr + get_displacement (buffer, aoffsetp));
      result += strlen (result);
      *result++ = NEXT_IS_ADDR;
      *result = '\0';
      break;
    case 'i':
      Ivalue = bit_extract (buffer, *aoffsetp, 8);
      *aoffsetp += 8;
      sprintf (result, "0x%x", Ivalue);
      break;
    case 'u':
      Ivalue = bit_extract (buffer, *aoffsetp, 8);
      optlist (Ivalue, opt_u, result);
      *aoffsetp += 8;
      break;
    case 'U':
      Ivalue = bit_extract (buffer, *aoffsetp, 8);
      optlist (Ivalue, opt_U, result);
      *aoffsetp += 8;
      break;
    case 'O':
      Ivalue = bit_extract (buffer, ioffset - 9, 9);
      optlist (Ivalue, opt_O, result);
      ioffset -= 9;
      break;
    case 'C':
      Ivalue = bit_extract (buffer, ioffset - 4, 4);
      optlist (Ivalue, opt_C, result);
      ioffset -= 4;
      break;
    case 'S':
      Ivalue = bit_extract (buffer, ioffset - 8, 8);
      optlist (Ivalue, opt_S, result);
      ioffset -= 8;
      break;
    case 'M':
      Ivalue = bit_extract (buffer, ioffset - 4, 4);
      list_search (Ivalue, 0 ? list_M032 : list_M532, result);
      ioffset -= 4;
      break;
    case 'P':
      Ivalue = bit_extract (buffer, ioffset - 4, 4);
      list_search (Ivalue, 0 ? list_P032 : list_P532, result);
      ioffset -= 4;
      break;
    case 'g':
      Ivalue = bit_extract (buffer, *aoffsetp, 3);
      sprintf (result, "%d", Ivalue);
      *aoffsetp += 3;
      break;
    case 'G':
      Ivalue = bit_extract(buffer, *aoffsetp, 5);
      sprintf (result, "%d", Ivalue + 1);
      *aoffsetp += 5;
      break;
    }
  return ioffset;
}


/* Print the 32000 instruction at address MEMADDR in debugged memory,
   on STREAM.  Returns length of the instruction, in bytes.  */

int
print_insn_ns32k (bfd_vma memaddr, disassemble_info *info)
{
  unsigned int i;
  const char *d;
  unsigned short first_word;
  int ioffset;		/* Bits into instruction.  */
  int aoffset;		/* Bits into arguments.  */
  char arg_bufs[MAX_ARGS+1][ARG_LEN];
  int argnum;
  int maxarg;
  struct private priv;
  bfd_byte *buffer = priv.the_buffer;
  dis_info = info;

  info->private_data = & priv;
  priv.max_fetched = priv.the_buffer;
  priv.insn_start = memaddr;
  if (setjmp (priv.bailout) != 0)
    /* Error return.  */
    return -1;

  /* Look for 8bit opcodes first. Other wise, fetching two bytes could take
     us over the end of accessible data unnecessarilly.  */
  FETCH_DATA (info, buffer + 1);
  for (i = 0; i < NOPCODES; i++)
    if (ns32k_opcodes[i].opcode_id_size <= 8
	&& ((buffer[0]
	     & (((unsigned long) 1 << ns32k_opcodes[i].opcode_id_size) - 1))
	    == ns32k_opcodes[i].opcode_seed))
      break;
  if (i == NOPCODES)
    {
      /* Maybe it is 9 to 16 bits big.  */
      FETCH_DATA (info, buffer + 2);
      first_word = read_memory_integer(buffer, 2);

      for (i = 0; i < NOPCODES; i++)
	if ((first_word
	     & (((unsigned long) 1 << ns32k_opcodes[i].opcode_id_size) - 1))
	    == ns32k_opcodes[i].opcode_seed)
	  break;

      /* Handle undefined instructions.  */
      if (i == NOPCODES)
	{
	  (*dis_info->fprintf_func)(dis_info->stream, "0%o", buffer[0]);
	  return 1;
	}
    }

  (*dis_info->fprintf_func)(dis_info->stream, "%s", ns32k_opcodes[i].name);

  ioffset = ns32k_opcodes[i].opcode_size;
  aoffset = ns32k_opcodes[i].opcode_size;
  d = ns32k_opcodes[i].operands;

  if (*d)
    {
      /* Offset in bits of the first thing beyond each index byte.
	 Element 0 is for operand A and element 1 is for operand B.
	 The rest are irrelevant, but we put them here so we don't
	 index outside the array.  */
      int index_offset[MAX_ARGS];

      /* 0 for operand A, 1 for operand B, greater for other args.  */
      int whicharg = 0;
      
      (*dis_info->fprintf_func)(dis_info->stream, "\t");

      maxarg = 0;

      /* First we have to find and keep track of the index bytes,
	 if we are using scaled indexed addressing mode, since the index
	 bytes occur right after the basic instruction, not as part
	 of the addressing extension.  */
      if (Is_gen(d[1]))
	{
	  int addr_mode = bit_extract (buffer, ioffset - 5, 5);

	  if (Adrmod_is_index (addr_mode))
	    {
	      aoffset += 8;
	      index_offset[0] = aoffset;
	    }
	}

      if (d[2] && Is_gen(d[3]))
	{
	  int addr_mode = bit_extract (buffer, ioffset - 10, 5);

	  if (Adrmod_is_index (addr_mode))
	    {
	      aoffset += 8;
	      index_offset[1] = aoffset;
	    }
	}

      while (*d)
	{
	  argnum = *d - '1';
	  d++;
	  if (argnum > maxarg && argnum < MAX_ARGS)
	    maxarg = argnum;
	  ioffset = print_insn_arg (*d, ioffset, &aoffset, buffer,
				    memaddr, arg_bufs[argnum],
				    index_offset[whicharg]);
	  d++;
	  whicharg++;
	}
      for (argnum = 0; argnum <= maxarg; argnum++)
	{
	  bfd_vma addr;
	  char *ch;

	  for (ch = arg_bufs[argnum]; *ch;)
	    {
	      if (*ch == NEXT_IS_ADDR)
		{
		  ++ch;
		  addr = bfd_scan_vma (ch, NULL, 16);
		  (*dis_info->print_address_func) (addr, dis_info);
		  while (*ch && *ch != NEXT_IS_ADDR)
		    ++ch;
		  if (*ch)
		    ++ch;
		}
	      else
		(*dis_info->fprintf_func)(dis_info->stream, "%c", *ch++);
	    }
	  if (argnum < maxarg)
	    (*dis_info->fprintf_func)(dis_info->stream, ", ");
	}
    }
  return aoffset / 8;
}
