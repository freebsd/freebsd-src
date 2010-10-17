/* xtensa-dis.c.  Disassembly functions for Xtensa.
   Copyright 2003 Free Software Foundation, Inc.
   Contributed by Bob Wilson at Tensilica, Inc. (bwilson@tensilica.com)

   This file is part of GDB, GAS, and the GNU binutils.

   GDB, GAS, and the GNU binutils are free software; you can redistribute
   them and/or modify them under the terms of the GNU General Public
   License as published by the Free Software Foundation; either version 2,
   or (at your option) any later version.

   GDB, GAS, and the GNU binutils are distributed in the hope that they
   will be useful, but WITHOUT ANY WARRANTY; without even the implied
   warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See
   the GNU General Public License for more details.

   You should have received a copy of the GNU General Public License along
   with this file; see the file COPYING.  If not, write to the Free
   Software Foundation, 59 Temple Place - Suite 330, Boston, MA 02111-1307,
   USA.  */

#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <string.h>
#include "xtensa-isa.h"
#include "ansidecl.h"
#include "sysdep.h"
#include "dis-asm.h"

#include <setjmp.h>

#ifndef MAX
#define MAX(a,b) (a > b ? a : b)
#endif

static char* state_names[256] =
{
  "lbeg", 		   	/* 0 */
  "lend",  		   	/* 1 */
  "lcount", 		   	/* 2 */
  "sar",		   	/* 3 */
  "br",			      	/* 4 */

  "reserved_5",			/* 5 */
  "reserved_6",			/* 6 */
  "reserved_7",			/* 7 */

  "av",				/* 8 */
  "avh",		       	/* 9 */
  "bv",				/* 10 */
  "sav",			/* 11 */
  "scompare1",		        /* 12 */

  "reserved_13",	       	/* 13 */
  "reserved_14",		/* 14 */
  "reserved_15",		/* 15 */

  "acclo",			/* 16 */
  "acchi",			/* 17 */

  "reserved_18",	        /* 18 */
  "reserved_19",	       	/* 19 */
  "reserved_20",		/* 20 */
  "reserved_21",		/* 21 */
  "reserved_22",	        /* 22 */
  "reserved_23",	       	/* 23 */
  "reserved_24",		/* 24 */
  "reserved_25",		/* 25 */
  "reserved_26",		/* 26 */
  "reserved_27",		/* 27 */
  "reserved_28",	        /* 28 */
  "reserved_29",	       	/* 29 */
  "reserved_30",		/* 30 */
  "reserved_31",		/* 31 */

  "mr0",			/* 32 */
  "mr1", 			/* 33 */
  "mr2",		      	/* 34 */
  "mr3",			/* 35 */

  "reserved_36",	        /* 36 */
  "reserved_37",	       	/* 37 */
  "reserved_38",		/* 38 */
  "reserved_39",		/* 39 */
  "reserved_40",	        /* 40 */
  "reserved_41",	       	/* 41 */
  "reserved_42",		/* 42 */
  "reserved_43",		/* 43 */
  "reserved_44",		/* 44 */
  "reserved_45",		/* 45 */
  "reserved_46",	        /* 46 */
  "reserved_47",	       	/* 47 */
  "reserved_48",		/* 48 */
  "reserved_49",		/* 49 */
  "reserved_50",	        /* 50 */
  "reserved_51",	       	/* 51 */
  "reserved_52",		/* 52 */
  "reserved_53",		/* 53 */
  "reserved_54",	        /* 54 */
  "reserved_55",	       	/* 55 */
  "reserved_56",		/* 56 */
  "reserved_57",		/* 57 */
  "reserved_58",		/* 58 */
  "reserved_59",		/* 59 */
  "reserved_60",	        /* 60 */
  "reserved_61",	       	/* 61 */
  "reserved_62",		/* 62 */
  "reserved_63",		/* 63 */

  "reserved_64",		/* 64 */
  "reserved_65",		/* 65 */
  "reserved_66",		/* 66 */
  "reserved_67",		/* 67 */
  "reserved_68",	        /* 68 */
  "reserved_69",	       	/* 69 */
  "reserved_70",		/* 70 */
  "reserved_71",		/* 71 */

  "wb",				/* 72 */
  "ws",				/* 73 */

  "reserved_74",		/* 74 */
  "reserved_75",		/* 75 */
  "reserved_76",	        /* 76 */
  "reserved_77",	       	/* 77 */
  "reserved_78",		/* 78 */
  "reserved_79",		/* 79 */
  "reserved_80",	        /* 80 */
  "reserved_81",	       	/* 81 */
  "reserved_82",		/* 82 */

  "ptevaddr",			/* 83 */

  "reserved_84",	        /* 84 */
  "reserved_85",	       	/* 85 */
  "reserved_86",		/* 86 */
  "reserved_87",		/* 87 */
  "reserved_88",		/* 88 */
  "reserved_89",		/* 89 */

  "rasid",		        /* 90 */
  "itlbcfg",		       	/* 91 */
  "dtlbcfg",			/* 92 */

  "reserved_93",		/* 93 */
  "reserved_94",		/* 94 */
  "reserved_95",		/* 95 */

  "ibreakenable",		/* 96 */

  "reserved_97",		/* 97 */

  "cacheattr",			/* 98 */

  "reserved_99",		/* 99 */
  "reserved_100",	       	/* 100 */
  "reserved_101",	       	/* 101 */
  "reserved_102",		/* 102 */
  "reserved_103",		/* 103 */

  "ddr",			/* 104 */

  "reserved_105",		/* 105 */
  "reserved_106",	        /* 106 */
  "reserved_107",	       	/* 107 */
  "reserved_108",		/* 108 */
  "reserved_109",		/* 109 */
  "reserved_110",	        /* 110 */
  "reserved_111",	       	/* 111 */
  "reserved_112",		/* 112 */
  "reserved_113",		/* 113 */
  "reserved_114",	        /* 114 */
  "reserved_115",	       	/* 115 */
  "reserved_116",		/* 116 */
  "reserved_117",		/* 117 */
  "reserved_118",		/* 118 */
  "reserved_119",		/* 119 */
  "reserved_120",	        /* 120 */
  "reserved_121",	       	/* 121 */
  "reserved_122",		/* 122 */
  "reserved_123",		/* 123 */
  "reserved_124",		/* 124 */
  "reserved_125",		/* 125 */
  "reserved_126",		/* 126 */
  "reserved_127",		/* 127 */

  "ibreaka0",			/* 128 */
  "ibreaka1",			/* 129 */
  "ibreaka2",			/* 130 */
  "ibreaka3",			/* 131 */
  "ibreaka4",			/* 132 */
  "ibreaka5",			/* 133 */
  "ibreaka6",			/* 134 */
  "ibreaka7",			/* 135 */
  "ibreaka8",			/* 136 */
  "ibreaka9",			/* 137 */
  "ibreaka10",       		/* 138 */
  "ibreaka11",       		/* 139 */
  "ibreaka12",       		/* 140 */
  "ibreaka13",       		/* 141 */
  "ibreaka14",       		/* 142 */
  "ibreaka15",       		/* 143 */
			   
  "dbreaka0",			/* 144 */
  "dbreaka1",			/* 145 */
  "dbreaka2",			/* 146 */
  "dbreaka3",			/* 147 */
  "dbreaka4",			/* 148 */
  "dbreaka5",			/* 149 */
  "dbreaka6",			/* 150 */
  "dbreaka7",			/* 151 */
  "dbreaka8",			/* 152 */
  "dbreaka9",			/* 153 */
  "dbreaka10",       		/* 154 */
  "dbreaka11",       		/* 155 */
  "dbreaka12",       		/* 156 */
  "dbreaka13",       		/* 157 */
  "dbreaka14",       		/* 158 */
  "dbreaka15",       		/* 159 */
			   
  "dbreakc0",			/* 160 */
  "dbreakc1",			/* 161 */
  "dbreakc2",			/* 162 */
  "dbreakc3",			/* 163 */
  "dbreakc4",			/* 164 */
  "dbreakc5",			/* 165 */
  "dbreakc6",			/* 166 */
  "dbreakc7",			/* 167 */
  "dbreakc8",			/* 168 */
  "dbreakc9",			/* 169 */
  "dbreakc10",       		/* 170 */
  "dbreakc11",       		/* 171 */
  "dbreakc12",       		/* 172 */
  "dbreakc13",       		/* 173 */
  "dbreakc14",       		/* 174 */
  "dbreakc15",       		/* 175 */

  "reserved_176",		/* 176 */

  "epc1",			/* 177 */
  "epc2",			/* 178 */
  "epc3",			/* 179 */
  "epc4",			/* 180 */
  "epc5",			/* 181 */
  "epc6",			/* 182 */
  "epc7",			/* 183 */
  "epc8",			/* 184 */
  "epc9",			/* 185 */
  "epc10",			/* 186 */
  "epc11",			/* 187 */
  "epc12",			/* 188 */
  "epc13",			/* 189 */
  "epc14",			/* 190 */
  "epc15",			/* 191 */
  "depc",			/* 192 */

  "reserved_193",		/* 193 */

  "eps2",	        	/* 194 */
  "eps3",	        	/* 195 */
  "eps4",	        	/* 196 */
  "eps5",	        	/* 197 */
  "eps6",	        	/* 198 */
  "eps7",	        	/* 199 */
  "eps8",	        	/* 200 */
  "eps9",	        	/* 201 */
  "eps10",			/* 202 */
  "eps11",			/* 203 */
  "eps12",			/* 204 */
  "eps13",			/* 205 */
  "eps14",			/* 206 */
  "eps15",			/* 207 */

  "reserved_208",		/* 208 */

  "excsave1",			/* 209 */
  "excsave2",			/* 210 */
  "excsave3",			/* 211 */
  "excsave4",			/* 212 */
  "excsave5",			/* 213 */
  "excsave6",			/* 214 */
  "excsave7",			/* 215 */
  "excsave8",			/* 216 */
  "excsave9",			/* 217 */
  "excsave10",       		/* 218 */
  "excsave11",       		/* 219 */
  "excsave12",       		/* 220 */
  "excsave13",       		/* 221 */
  "excsave14",       		/* 222 */
  "excsave15",       		/* 223 */
  "cpenable",			/* 224 */

  "reserved_225",		/* 225 */

  "interrupt",			/* 226 */
  "interrupt2",			/* 227 */
  "intenable",			/* 228 */

  "reserved_229",		/* 229 */

  "ps",				/* 230 */

  "reserved_231",		/* 231 */

  "exccause",			/* 232 */
  "debugcause",			/* 233 */
  "ccount",			/* 234 */
  "prid",			/* 235 */
  "icount",			/* 236 */
  "icountlvl", 			/* 237 */
  "excvaddr",			/* 238 */

  "reserved_239",		/* 239 */

  "ccompare0",			/* 240 */
  "ccompare1",			/* 241 */
  "ccompare2",			/* 242 */
  "ccompare3",			/* 243 */

  "misc0",			/* 244 */
  "misc1",	 		/* 245 */
  "misc2",			/* 246 */
  "misc3",			/* 247 */

  "reserved_248",		/* 248 */
  "reserved_249",	        /* 249 */
  "reserved_250",	       	/* 250 */
  "reserved_251",		/* 251 */
  "reserved_252",		/* 252 */
  "reserved_253",	        /* 253 */
  "reserved_254",	       	/* 254 */
  "reserved_255",	       	/* 255 */
};


int show_raw_fields;

static int fetch_data
  PARAMS ((struct disassemble_info *info, bfd_vma memaddr));
static void print_xtensa_operand
  PARAMS ((bfd_vma, struct disassemble_info *, xtensa_operand,
	   unsigned operand_val, int print_sr_name));

struct dis_private {
  bfd_byte *byte_buf;
  jmp_buf bailout;
};

static int
fetch_data (info, memaddr)
     struct disassemble_info *info;
     bfd_vma memaddr;
{
  int length, status = 0;
  struct dis_private *priv = (struct dis_private *) info->private_data;
  int insn_size = xtensa_insn_maxlength (xtensa_default_isa);

  /* Read the maximum instruction size, padding with zeros if we go past
     the end of the text section.  This code will automatically adjust
     length when we hit the end of the buffer.  */

  memset (priv->byte_buf, 0, insn_size);
  for (length = insn_size; length > 0; length--)
    {
      status = (*info->read_memory_func) (memaddr, priv->byte_buf, length,
					  info);
      if (status == 0)
	return length;
    }
  (*info->memory_error_func) (status, memaddr, info);
  longjmp (priv->bailout, 1);
  /*NOTREACHED*/
}


static void
print_xtensa_operand (memaddr, info, opnd, operand_val, print_sr_name)
     bfd_vma memaddr;
     struct disassemble_info *info;
     xtensa_operand opnd;
     unsigned operand_val;
     int print_sr_name;
{
  char *kind = xtensa_operand_kind (opnd);
  int signed_operand_val;
    
  if (show_raw_fields)
    {
      if (operand_val < 0xa)
	(*info->fprintf_func) (info->stream, "%u", operand_val);
      else
	(*info->fprintf_func) (info->stream, "0x%x", operand_val);
      return;
    }

  operand_val = xtensa_operand_decode (opnd, operand_val);
  signed_operand_val = (int) operand_val;

  if (xtensa_operand_isPCRelative (opnd))
    {
      operand_val = xtensa_operand_undo_reloc (opnd, operand_val, memaddr);
      info->target = operand_val;
      (*info->print_address_func) (info->target, info);
    }
  else if (!strcmp (kind, "i"))
    {
      if (print_sr_name
	  && signed_operand_val >= 0
	  && signed_operand_val <= 255)
	(*info->fprintf_func) (info->stream, "%s",
			       state_names[signed_operand_val]);
      else if ((signed_operand_val > -256) && (signed_operand_val < 256))
	(*info->fprintf_func) (info->stream, "%d", signed_operand_val);
      else
	(*info->fprintf_func) (info->stream, "0x%x",signed_operand_val);
    }
  else
    (*info->fprintf_func) (info->stream, "%s%u", kind, operand_val);
}


/* Print the Xtensa instruction at address MEMADDR on info->stream.
   Returns length of the instruction in bytes.  */

int
print_insn_xtensa (memaddr, info)
     bfd_vma memaddr;
     struct disassemble_info *info;
{
  unsigned operand_val;
  int bytes_fetched, size, maxsize, i, noperands;
  xtensa_isa isa;
  xtensa_opcode opc;
  char *op_name;
  int print_sr_name;
  struct dis_private priv;
  static bfd_byte *byte_buf = NULL;
  static xtensa_insnbuf insn_buffer = NULL;

  if (!xtensa_default_isa)
    (void) xtensa_isa_init ();

  info->target = 0;
  maxsize = xtensa_insn_maxlength (xtensa_default_isa);

  /* Set bytes_per_line to control the amount of whitespace between the hex
     values and the opcode.  For Xtensa, we always print one "chunk" and we
     vary bytes_per_chunk to determine how many bytes to print.  (objdump
     would apparently prefer that we set bytes_per_chunk to 1 and vary
     bytes_per_line but that makes it hard to fit 64-bit instructions on
     an 80-column screen.)  The value of bytes_per_line here is not exactly
     right, because objdump adds an extra space for each chunk so that the
     amount of whitespace depends on the chunk size.  Oh well, it's good
     enough....  Note that we set the minimum size to 4 to accomodate
     literal pools.  */
  info->bytes_per_line = MAX (maxsize, 4);

  /* Allocate buffers the first time through.  */
  if (!insn_buffer)
    insn_buffer = xtensa_insnbuf_alloc (xtensa_default_isa);
  if (!byte_buf)
    byte_buf = (bfd_byte *) malloc (MAX (maxsize, 4));

  priv.byte_buf = byte_buf;

  info->private_data = (PTR) &priv;
  if (setjmp (priv.bailout) != 0)
      /* Error return.  */
      return -1;

  /* Don't set "isa" before the setjmp to keep the compiler from griping.  */
  isa = xtensa_default_isa;

  /* Fetch the maximum size instruction.  */
  bytes_fetched = fetch_data (info, memaddr);

  /* Copy the bytes into the decode buffer.  */
  memset (insn_buffer, 0, (xtensa_insnbuf_size (isa) *
			   sizeof (xtensa_insnbuf_word)));
  xtensa_insnbuf_from_chars (isa, insn_buffer, priv.byte_buf);

  opc = xtensa_decode_insn (isa, insn_buffer);
  if (opc == XTENSA_UNDEFINED
      || ((size = xtensa_insn_length (isa, opc)) > bytes_fetched))
    {
      (*info->fprintf_func) (info->stream, ".byte %#02x", priv.byte_buf[0]);
      return 1;
    }

  op_name = (char *) xtensa_opcode_name (isa, opc);
  (*info->fprintf_func) (info->stream, "%s", op_name);

  print_sr_name = (!strcasecmp (op_name, "wsr")
		   || !strcasecmp (op_name, "xsr")
		   || !strcasecmp (op_name, "rsr"));

  /* Print the operands (if any).  */
  noperands = xtensa_num_operands (isa, opc);
  if (noperands > 0)
    {
      int first = 1;

      (*info->fprintf_func) (info->stream, "\t");
      for (i = 0; i < noperands; i++)
	{
	  xtensa_operand opnd = xtensa_get_operand (isa, opc, i);

	  if (first)
	    first = 0;
	  else
	    (*info->fprintf_func) (info->stream, ", ");
	  operand_val = xtensa_operand_get_field (opnd, insn_buffer);
	  print_xtensa_operand (memaddr, info, opnd, operand_val,
				print_sr_name);
        }
    }

  info->bytes_per_chunk = size;
  info->display_endian = info->endian;

  return size;
}

