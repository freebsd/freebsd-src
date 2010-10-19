/* Disassemble h8300 instructions.
   Copyright 1993, 1994, 1996, 1998, 2000, 2001, 2002, 2003, 2004, 2005
   Free Software Foundation, Inc.

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

#define DEFINE_TABLE

#include "sysdep.h"
#define h8_opcodes h8ops
#include "opcode/h8300.h"
#include "dis-asm.h"
#include "opintl.h"
#include "libiberty.h"

struct h8_instruction
{
  int length;
  const struct h8_opcode *opcode;
};

struct h8_instruction *h8_instructions;

/* Run through the opcodes and sort them into order to make them easy
   to disassemble.  */

static void
bfd_h8_disassemble_init (void)
{
  unsigned int i;
  unsigned int nopcodes;
  const struct h8_opcode *p;
  struct h8_instruction *pi;

  nopcodes = sizeof (h8_opcodes) / sizeof (struct h8_opcode);

  h8_instructions = xmalloc (nopcodes * sizeof (struct h8_instruction));

  for (p = h8_opcodes, pi = h8_instructions; p->name; p++, pi++)
    {
      int n1 = 0;
      int n2 = 0;

      if ((int) p->data.nib[0] < 16)
	n1 = (int) p->data.nib[0];
      else
	n1 = 0;

      if ((int) p->data.nib[1] < 16)
	n2 = (int) p->data.nib[1];
      else
	n2 = 0;

      /* Just make sure there are an even number of nibbles in it, and
	 that the count is the same as the length.  */
      for (i = 0; p->data.nib[i] != (op_type) E; i++)
	;

      if (i & 1)
	{
	  fprintf (stderr, "Internal error, h8_disassemble_init.\n");
	  abort ();
	}

      pi->length = i / 2;
      pi->opcode = p;
    }

  /* Add entry for the NULL vector terminator.  */
  pi->length = 0;
  pi->opcode = p;
}

static void
extract_immediate (FILE *stream,
		   op_type looking_for,
		   int thisnib,
		   unsigned char *data,
		   int *cst,
		   int *len,
		   const struct h8_opcode *q)
{
  switch (looking_for & SIZE)
    {
    case L_2:
      *len = 2;
      *cst = thisnib & 3;

      /* DISP2 special treatment.  */
      if ((looking_for & MODE) == DISP)
	{
	  if (OP_KIND (q->how) == O_MOVAB
	      || OP_KIND (q->how) == O_MOVAW
	      || OP_KIND (q->how) == O_MOVAL)
	    {
	      /* Handling for mova insn.  */
	      switch (q->args.nib[0] & MODE)
		{
		case INDEXB:
		default:
		  break;
		case INDEXW:
		  *cst *= 2;
		  break;
		case INDEXL:
		  *cst *= 4;
		  break;
		}
	    }
	  else
	    {
	      /* Handling for non-mova insn.  */
	      switch (OP_SIZE (q->how))
		{
		default: break;
		case SW:
		  *cst *= 2;
		  break;
		case SL:
		  *cst *= 4;
		  break;
		}
	    }
	}
      break;
    case L_8:
      *len = 8;
      *cst = data[0];
      break;
    case L_16:
    case L_16U:
      *len = 16;
      *cst = (data[0] << 8) + data [1];
#if 0
      if ((looking_for & SIZE) == L_16)
	*cst = (short) *cst;	/* Sign extend.  */
#endif
      break;
    case L_32:
      *len = 32;
      *cst = (data[0] << 24) + (data[1] << 16) + (data[2] << 8) + data[3];
      break;
    default:
      *len = 0;
      *cst = 0;
      fprintf (stream, "DISP bad size\n");
      break;
    }
}

static const char *regnames[] =
{
  "r0h", "r1h", "r2h", "r3h", "r4h", "r5h", "r6h", "r7h",
  "r0l", "r1l", "r2l", "r3l", "r4l", "r5l", "r6l", "r7l"
};
static const char *wregnames[] =
{
  "r0", "r1", "r2", "r3", "r4", "r5", "r6", "r7",
  "e0", "e1", "e2", "e3", "e4", "e5", "e6", "e7"
};
static const char *lregnames[] =
{
  "er0", "er1", "er2", "er3", "er4", "er5", "er6", "er7",
  "er0", "er1", "er2", "er3", "er4", "er5", "er6", "er7"
};
static const char *cregnames[] =
{
  "ccr", "exr", "mach", "macl", "", "", "vbr", "sbr"
};

static void
print_one_arg (disassemble_info *info,
	       bfd_vma addr,
	       op_type x,
	       int cst,
	       int cstlen,
	       int rdisp_n,
	       int rn,
	       const char **pregnames,
	       int len)
{
  void * stream = info->stream;
  fprintf_ftype outfn = info->fprintf_func;

  if ((x & SIZE) == L_3 || (x & SIZE) == L_3NZ)
    outfn (stream, "#0x%x", (unsigned) cst);
  else if ((x & MODE) == IMM)
    outfn (stream, "#0x%x", (unsigned) cst);
  else if ((x & MODE) == DBIT || (x & MODE) == KBIT)
    outfn (stream, "#%d", (unsigned) cst);
  else if ((x & MODE) == CONST_2)
    outfn (stream, "#2");
  else if ((x & MODE) == CONST_4)
    outfn (stream, "#4");
  else if ((x & MODE) == CONST_8)
    outfn (stream, "#8");
  else if ((x & MODE) == CONST_16)
    outfn (stream, "#16");
  else if ((x & MODE) == REG)
    {
      switch (x & SIZE)
	{
	case L_8:
	  outfn (stream, "%s", regnames[rn]);
	  break;
	case L_16:
	case L_16U:
	  outfn (stream, "%s", wregnames[rn]);
	  break;
	case L_P:
	case L_32:
	  outfn (stream, "%s", lregnames[rn]);
	  break;
	}
    }
  else if ((x & MODE) == LOWREG)
    {
      switch (x & SIZE)
	{
	case L_8:
	  /* Always take low half of reg.  */
	  outfn (stream, "%s.b", regnames[rn < 8 ? rn + 8 : rn]);
	  break;
	case L_16:
	case L_16U:
	  /* Always take low half of reg.  */
	  outfn (stream, "%s.w", wregnames[rn < 8 ? rn : rn - 8]);
	  break;
	case L_P:
	case L_32:
	  outfn (stream, "%s.l", lregnames[rn]);
	  break;
	}
    }
  else if ((x & MODE) == POSTINC)
    outfn (stream, "@%s+", pregnames[rn]);

  else if ((x & MODE) == POSTDEC)
    outfn (stream, "@%s-", pregnames[rn]);

  else if ((x & MODE) == PREINC)
    outfn (stream, "@+%s", pregnames[rn]);

  else if ((x & MODE) == PREDEC)
    outfn (stream, "@-%s", pregnames[rn]);

  else if ((x & MODE) == IND)
    outfn (stream, "@%s", pregnames[rn]);

  else if ((x & MODE) == ABS || (x & ABSJMP))
    outfn (stream, "@0x%x:%d", (unsigned) cst, cstlen);

  else if ((x & MODE) == MEMIND)
    outfn (stream, "@@%d (0x%x)", cst, cst);

  else if ((x & MODE) == VECIND)
    {
      /* FIXME Multiplier should be 2 or 4, depending on processor mode,
	 by which is meant "normal" vs. "middle", "advanced", "maximum".  */

      int offset = (cst + 0x80) * 4;
      outfn (stream, "@@%d (0x%x)", offset, offset);
    }
  else if ((x & MODE) == PCREL)
    {
      if ((x & SIZE) == L_16 ||
	  (x & SIZE) == L_16U)
	{
	  outfn (stream, ".%s%d (0x%lx)",
		   (short) cst > 0 ? "+" : "",
		   (short) cst, 
		   (long)(addr + (short) cst + len));
	}
      else
	{
	  outfn (stream, ".%s%d (0x%lx)",
		   (char) cst > 0 ? "+" : "",
		   (char) cst, 
		   (long)(addr + (char) cst + len));
	}
    }
  else if ((x & MODE) == DISP)
    outfn (stream, "@(0x%x:%d,%s)", cst, cstlen, pregnames[rdisp_n]);

  else if ((x & MODE) == INDEXB)
    /* Always take low half of reg.  */
    outfn (stream, "@(0x%x:%d,%s.b)", cst, cstlen, 
	   regnames[rdisp_n < 8 ? rdisp_n + 8 : rdisp_n]);

  else if ((x & MODE) == INDEXW)
    /* Always take low half of reg.  */
    outfn (stream, "@(0x%x:%d,%s.w)", cst, cstlen, 
	   wregnames[rdisp_n < 8 ? rdisp_n : rdisp_n - 8]);

  else if ((x & MODE) == INDEXL)
    outfn (stream, "@(0x%x:%d,%s.l)", cst, cstlen, lregnames[rdisp_n]);

  else if (x & CTRL)
    outfn (stream, cregnames[rn]);

  else if ((x & MODE) == CCR)
    outfn (stream, "ccr");

  else if ((x & MODE) == EXR)
    outfn (stream, "exr");

  else if ((x & MODE) == MACREG)
    outfn (stream, "mac%c", cst ? 'l' : 'h');

  else
    /* xgettext:c-format */
    outfn (stream, _("Hmmmm 0x%x"), x);
}

static unsigned int
bfd_h8_disassemble (bfd_vma addr, disassemble_info *info, int mach)
{
  /* Find the first entry in the table for this opcode.  */
  int regno[3] = { 0, 0, 0 };
  int dispregno[3] = { 0, 0, 0 };
  int cst[3] = { 0, 0, 0 };
  int cstlen[3] = { 0, 0, 0 };
  static bfd_boolean init = 0;
  const struct h8_instruction *qi;
  char const **pregnames = mach != 0 ? lregnames : wregnames;
  int status;
  unsigned int l;
  unsigned char data[MAX_CODE_NIBBLES];
  void *stream = info->stream;
  fprintf_ftype outfn = info->fprintf_func;

  if (!init)
    {
      bfd_h8_disassemble_init ();
      init = 1;
    }

  status = info->read_memory_func (addr, data, 2, info);
  if (status != 0)
    {
      info->memory_error_func (status, addr, info);
      return -1;
    }

  for (l = 2; status == 0 && l < sizeof (data) / 2; l += 2)
    status = info->read_memory_func (addr + l, data + l, 2, info);

  /* Find the exact opcode/arg combo.  */
  for (qi = h8_instructions; qi->opcode->name; qi++)
    {
      const struct h8_opcode *q = qi->opcode;
      op_type *nib = q->data.nib;
      unsigned int len = 0;

      while (1)
	{
	  op_type looking_for = *nib;
	  int thisnib = data[len / 2];
	  int opnr;

	  thisnib = (len & 1) ? (thisnib & 0xf) : ((thisnib / 16) & 0xf);
	  opnr = ((looking_for & OP3) == OP3 ? 2
		  : (looking_for & DST) == DST ? 1 : 0);

	  if (looking_for < 16 && looking_for >= 0)
	    {
	      if (looking_for != thisnib)
		goto fail;
	    }
	  else
	    {
	      if ((int) looking_for & (int) B31)
		{
		  if (!((thisnib & 0x8) != 0))
		    goto fail;

		  looking_for = (op_type) ((int) looking_for & ~(int) B31);
		  thisnib &= 0x7;
		}
	      else if ((int) looking_for & (int) B30)
		{
		  if (!((thisnib & 0x8) == 0))
		    goto fail;

		  looking_for = (op_type) ((int) looking_for & ~(int) B30);
		}

	      if ((int) looking_for & (int) B21)
		{
		  if (!((thisnib & 0x4) != 0))
		    goto fail;

		  looking_for = (op_type) ((int) looking_for & ~(int) B21);
		  thisnib &= 0xb;
		}
	      else if ((int) looking_for & (int) B20)
		{
		  if (!((thisnib & 0x4) == 0))
		    goto fail;

		  looking_for = (op_type) ((int) looking_for & ~(int) B20);
		}
	      if ((int) looking_for & (int) B11)
		{
		  if (!((thisnib & 0x2) != 0))
		    goto fail;

		  looking_for = (op_type) ((int) looking_for & ~(int) B11);
		  thisnib &= 0xd;
		}
	      else if ((int) looking_for & (int) B10)
		{
		  if (!((thisnib & 0x2) == 0))
		    goto fail;

		  looking_for = (op_type) ((int) looking_for & ~(int) B10);
		}

	      if ((int) looking_for & (int) B01)
		{
		  if (!((thisnib & 0x1) != 0))
		    goto fail;

		  looking_for = (op_type) ((int) looking_for & ~(int) B01);
		  thisnib &= 0xe;
		}
	      else if ((int) looking_for & (int) B00)
		{
		  if (!((thisnib & 0x1) == 0))
		    goto fail;

		  looking_for = (op_type) ((int) looking_for & ~(int) B00);
		}

	      if (looking_for & IGNORE)
		{
		  /* Hitachi has declared that IGNORE must be zero.  */
		  if (thisnib != 0)
		    goto fail;
		}
	      else if ((looking_for & MODE) == DATA)
		{
		  ;			/* Skip embedded data.  */
		}
	      else if ((looking_for & MODE) == DBIT)
		{
		  /* Exclude adds/subs by looking at bit 0 and 2, and
                     make sure the operand size, either w or l,
                     matches by looking at bit 1.  */
		  if ((looking_for & 7) != (thisnib & 7))
		    goto fail;

		  cst[opnr] = (thisnib & 0x8) ? 2 : 1;
		}
	      else if ((looking_for & MODE) == DISP
		       || (looking_for & MODE) == ABS
		       || (looking_for & MODE) == PCREL
		       || (looking_for & MODE) == INDEXB
		       || (looking_for & MODE) == INDEXW
		       || (looking_for & MODE) == INDEXL)
		{
		  extract_immediate (stream, looking_for, thisnib, 
				     data + len / 2, cst + opnr, 
				     cstlen + opnr, q);
		  /* Even address == bra, odd == bra/s.  */
		  if (q->how == O (O_BRAS, SB))
		    cst[opnr] -= 1;
		}
	      else if ((looking_for & MODE) == REG
		       || (looking_for & MODE) == LOWREG
		       || (looking_for & MODE) == IND
		       || (looking_for & MODE) == PREINC
		       || (looking_for & MODE) == POSTINC
		       || (looking_for & MODE) == PREDEC
		       || (looking_for & MODE) == POSTDEC)
		{
		  regno[opnr] = thisnib;
		}
	      else if (looking_for & CTRL)	/* Control Register.  */
		{
		  thisnib &= 7;
		  if (((looking_for & MODE) == CCR  && (thisnib != C_CCR))
		      || ((looking_for & MODE) == EXR  && (thisnib != C_EXR))
		      || ((looking_for & MODE) == MACH && (thisnib != C_MACH))
		      || ((looking_for & MODE) == MACL && (thisnib != C_MACL))
		      || ((looking_for & MODE) == VBR  && (thisnib != C_VBR))
		      || ((looking_for & MODE) == SBR  && (thisnib != C_SBR)))
		    goto fail;
		  if (((looking_for & MODE) == CCR_EXR
		       && (thisnib != C_CCR && thisnib != C_EXR))
		      || ((looking_for & MODE) == VBR_SBR
			  && (thisnib != C_VBR && thisnib != C_SBR))
		      || ((looking_for & MODE) == MACREG
			  && (thisnib != C_MACH && thisnib != C_MACL)))
		    goto fail;
		  if (((looking_for & MODE) == CC_EX_VB_SB
		       && (thisnib != C_CCR && thisnib != C_EXR
			   && thisnib != C_VBR && thisnib != C_SBR)))
		    goto fail;

		  regno[opnr] = thisnib;
		}
	      else if ((looking_for & SIZE) == L_5)
		{
		  cst[opnr] = data[len / 2] & 31;
		  cstlen[opnr] = 5;
		}
	      else if ((looking_for & SIZE) == L_4)
		{
		  cst[opnr] = thisnib;
		  cstlen[opnr] = 4;
		}
	      else if ((looking_for & SIZE) == L_16
		       || (looking_for & SIZE) == L_16U)
		{
		  cst[opnr] = (data[len / 2]) * 256 + data[(len + 2) / 2];
		  cstlen[opnr] = 16;
		}
	      else if ((looking_for & MODE) == MEMIND)
		{
		  cst[opnr] = data[1];
		}
	      else if ((looking_for & MODE) == VECIND)
		{
		  cst[opnr] = data[1] & 0x7f;
		}
	      else if ((looking_for & SIZE) == L_32)
		{
		  int i = len / 2;

		  cst[opnr] = ((data[i] << 24) 
			       | (data[i + 1] << 16) 
			       | (data[i + 2] << 8)
			       | (data[i + 3]));

		  cstlen[opnr] = 32;
		}
	      else if ((looking_for & SIZE) == L_24)
		{
		  int i = len / 2;

		  cst[opnr] = 
		    (data[i] << 16) | (data[i + 1] << 8) | (data[i + 2]);
		  cstlen[opnr] = 24;
		}
	      else if (looking_for & IGNORE)
		{
		  ;
		}
	      else if (looking_for & DISPREG)
		{
		  dispregno[opnr] = thisnib & 7;
		}
	      else if ((looking_for & MODE) == KBIT)
		{
		  switch (thisnib)
		    {
		    case 9:
		      cst[opnr] = 4;
		      break;
		    case 8:
		      cst[opnr] = 2;
		      break;
		    case 0:
		      cst[opnr] = 1;
		      break;
		    default:
		      goto fail;
		    }
		}
	      else if ((looking_for & SIZE) == L_8)
		{
		  cstlen[opnr] = 8;
		  cst[opnr] = data[len / 2];
		}
	      else if ((looking_for & SIZE) == L_3
		       || (looking_for & SIZE) == L_3NZ)
		{
		  cst[opnr] = thisnib & 0x7;
		  if (cst[opnr] == 0 && (looking_for & SIZE) == L_3NZ)
		    goto fail;
		}
	      else if ((looking_for & SIZE) == L_2)
		{
		  cstlen[opnr] = 2;
		  cst[opnr] = thisnib & 0x3;
		}
	      else if ((looking_for & MODE) == MACREG)
		{
		  cst[opnr] = (thisnib == 3);
		}
	      else if (looking_for == (op_type) E)
		{
		  outfn (stream, "%s\t", q->name);

		  /* Gross.  Disgusting.  */
		  if (strcmp (q->name, "ldm.l") == 0)
		    {
		      int count, high;

		      count = (data[1] / 16) & 0x3;
		      high = regno[1];

		      outfn (stream, "@sp+,er%d-er%d", high - count, high);
		      return qi->length;
		    }

		  if (strcmp (q->name, "stm.l") == 0)
		    {
		      int count, low;

		      count = (data[1] / 16) & 0x3;
		      low = regno[0];

		      outfn (stream, "er%d-er%d,@-sp", low, low + count);
		      return qi->length;
		    }
		  if (strcmp (q->name, "rte/l") == 0
		      || strcmp (q->name, "rts/l") == 0)
		    {
		      if (regno[0] == 0)
			outfn (stream, "er%d", regno[1]);
		      else
			outfn (stream, "er%d-er%d", regno[1] - regno[0],
			       regno[1]);
		      return qi->length;
		    }
		  if (strncmp (q->name, "mova", 4) == 0)
		    {
		      op_type *args = q->args.nib;

		      if (args[1] == (op_type) E)
			{
			  /* Short form.  */
			  print_one_arg (info, addr, args[0], cst[0], 
					 cstlen[0], dispregno[0], regno[0], 
					 pregnames, qi->length);
			  outfn (stream, ",er%d", dispregno[0]);
			}
		      else
			{
			  outfn (stream, "@(0x%x:%d,", cst[0], cstlen[0]);
			  print_one_arg (info, addr, args[1], cst[1], 
					 cstlen[1], dispregno[1], regno[1], 
					 pregnames, qi->length);
			  outfn (stream, ".%c),",
				 (args[0] & MODE) == INDEXB ? 'b' : 'w');
			  print_one_arg (info, addr, args[2], cst[2], 
					 cstlen[2], dispregno[2], regno[2], 
					 pregnames, qi->length);
			}
		      return qi->length;
		    }
		  /* Fill in the args.  */
		  {
		    op_type *args = q->args.nib;
		    int hadone = 0;
		    int nargs;

		    /* Special case handling for the adds and subs instructions
		       since in H8 mode thay can only take the r0-r7 registers
		       but in other (higher) modes they can take the er0-er7
		       registers as well.  */
		    if (strcmp (qi->opcode->name, "adds") == 0
			|| strcmp (qi->opcode->name, "subs") == 0)
		      {
			outfn (stream, "#%d,%s", cst[0], pregnames[regno[1] & 0x7]);
			return qi->length;
		      }

		    for (nargs = 0; 
			 nargs < 3 && args[nargs] != (op_type) E;
			 nargs++)
		      {
			int x = args[nargs];

			if (hadone)
			  outfn (stream, ",");

			print_one_arg (info, addr, x,
				       cst[nargs], cstlen[nargs],
				       dispregno[nargs], regno[nargs],
				       pregnames, qi->length);

			hadone = 1;
		      }
		  }

		  return qi->length;
		}
	      else
		/* xgettext:c-format */
		outfn (stream, _("Don't understand 0x%x \n"), looking_for);
	    }

	  len++;
	  nib++;
	}

    fail:
      ;
    }

  /* Fell off the end.  */
  outfn (stream, ".word\tH'%x,H'%x", data[0], data[1]);
  return 2;
}

int
print_insn_h8300 (bfd_vma addr, disassemble_info *info)
{
  return bfd_h8_disassemble (addr, info, 0);
}

int
print_insn_h8300h (bfd_vma addr, disassemble_info *info)
{
  return bfd_h8_disassemble (addr, info, 1);
}

int
print_insn_h8300s (bfd_vma addr, disassemble_info *info)
{
  return bfd_h8_disassemble (addr, info, 2);
}
