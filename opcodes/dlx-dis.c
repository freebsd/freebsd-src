/* Instruction printing code for the DLX Microprocessor
   Copyright 2002, 2005 Free Software Foundation, Inc.
   Contributed by Kuang Hwa Lin.  Written by Kuang Hwa Lin, 03/2002.

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

#include "sysdep.h"
#include "dis-asm.h"
#include "opcode/dlx.h"

#define R_ERROR     0x1
#define R_TYPE      0x2
#define ILD_TYPE    0x3
#define IST_TYPE    0x4
#define IAL_TYPE    0x5
#define IBR_TYPE    0x6
#define IJ_TYPE     0x7
#define IJR_TYPE    0x8
#define NIL         0x9

#define OPC(x)      ((x >> 26) & 0x3F)
#define FUNC(x)     (x & 0x7FF)

unsigned char opc, rs1, rs2, rd;
unsigned long imm26, imm16, func, current_insn_addr;

/* Print one instruction from MEMADDR on INFO->STREAM.
   Return the size of the instruction (always 4 on dlx).  */

static unsigned char
dlx_get_opcode (unsigned long opcode)
{
  return (unsigned char) ((opcode >> 26) & 0x3F);
}

static unsigned char
dlx_get_rs1 (unsigned long opcode)
{
  return (unsigned char) ((opcode >> 21) & 0x1F);
}

static unsigned char
dlx_get_rs2 (unsigned long opcode)
{
  return (unsigned char) ((opcode >> 16) & 0x1F);
}

static unsigned char
dlx_get_rdR (unsigned long opcode)
{
  return (unsigned char) ((opcode >> 11) & 0x1F);
}

static unsigned long
dlx_get_func (unsigned long opcode)
{
  return (unsigned char) (opcode & 0x7FF);
}

static unsigned long
dlx_get_imm16 (unsigned long opcode)
{
  return (unsigned long) (opcode & 0xFFFF);
}

static unsigned long
dlx_get_imm26 (unsigned long opcode)
{
  return (unsigned long) (opcode & 0x03FFFFFF);
}

/* Fill the opcode to the max length.  */

static void
operand_deliminator (struct disassemble_info *info, char *ptr)
{
  int difft = 8 - (int) strlen (ptr);

  while (difft > 0)
    {
      (*info->fprintf_func) (info->stream, "%c", ' ');
      difft -= 1;
    }
}

/* Process the R-type opcode.  */

static unsigned char
dlx_r_type (struct disassemble_info *info)
{
  unsigned char r_opc[] = { OPC(ALUOP) }; /* Fix ME */
  int r_opc_num = (sizeof r_opc) / (sizeof (char));
  struct _r_opcode
  {
    unsigned long func;
    char *name;
  }
  dlx_r_opcode[] =
  {
    { NOPF,     "nop"    },  /* NOP                          */
    { ADDF,     "add"    },  /* Add                          */
    { ADDUF,    "addu"   },  /* Add Unsigned                 */
    { SUBF,     "sub"    },  /* SUB                          */
    { SUBUF,    "subu"   },  /* Sub Unsigned                 */
    { MULTF,    "mult"   },  /* MULTIPLY                     */
    { MULTUF,   "multu"  },  /* MULTIPLY Unsigned            */
    { DIVF,     "div"    },  /* DIVIDE                       */
    { DIVUF,    "divu"   },  /* DIVIDE Unsigned              */
    { ANDF,     "and"    },  /* AND                          */
    { ORF,      "or"     },  /* OR                           */
    { XORF,     "xor"    },  /* Exclusive OR                 */
    { SLLF,     "sll"    },  /* SHIFT LEFT LOGICAL           */
    { SRAF,     "sra"    },  /* SHIFT RIGHT ARITHMETIC       */
    { SRLF,     "srl"    },  /* SHIFT RIGHT LOGICAL          */
    { SEQF,     "seq"    },  /* Set if equal                 */
    { SNEF,     "sne"    },  /* Set if not equal             */
    { SLTF,     "slt"    },  /* Set if less                  */
    { SGTF,     "sgt"    },  /* Set if greater               */
    { SLEF,     "sle"    },  /* Set if less or equal         */
    { SGEF,     "sge"    },  /* Set if greater or equal      */
    { SEQUF,    "sequ"   },  /* Set if equal                 */
    { SNEUF,    "sneu"   },  /* Set if not equal             */
    { SLTUF,    "sltu"   },  /* Set if less                  */
    { SGTUF,    "sgtu"   },  /* Set if greater               */
    { SLEUF,    "sleu"   },  /* Set if less or equal         */
    { SGEUF,    "sgeu"   },  /* Set if greater or equal      */
    { MVTSF,    "mvts"   },  /* Move to special register     */
    { MVFSF,    "mvfs"   },  /* Move from special register   */
    { BSWAPF,   "bswap"  },  /* Byte swap ??                 */
    { LUTF,     "lut"    }   /* ????????? ??                 */
  };
  int dlx_r_opcode_num = (sizeof dlx_r_opcode) / (sizeof dlx_r_opcode[0]);
  int idx;

  for (idx = 0; idx < r_opc_num; idx++)
    {
      if (r_opc[idx] != opc)
	continue;
      else
	break;
    }

  if (idx == r_opc_num)
    return NIL;

  for (idx = 0 ; idx < dlx_r_opcode_num; idx++)
    if (dlx_r_opcode[idx].func == func)
      {
	(*info->fprintf_func) (info->stream, "%s", dlx_r_opcode[idx].name);

	if (func != NOPF)
	  {
	    /* This is not a nop.  */
	    operand_deliminator (info, dlx_r_opcode[idx].name);
	    (*info->fprintf_func) (info->stream, "r%d,", (int)rd);
	    (*info->fprintf_func) (info->stream, "r%d", (int)rs1);
	    if (func != MVTSF && func != MVFSF)
	      (*info->fprintf_func) (info->stream, ",r%d", (int)rs2);
	  }
	return (unsigned char) R_TYPE;
      }

  return (unsigned char) R_ERROR;
}

/* Process the memory read opcode.  */

static unsigned char
dlx_load_type (struct disassemble_info* info)
{
  struct _load_opcode
  {
    unsigned long opcode;
    char *name;
  }
  dlx_load_opcode[] =
  {
    { OPC(LHIOP),   "lhi" },  /* Load HI to register.           */
    { OPC(LBOP),     "lb" },  /* load byte sign extended.       */
    { OPC(LBUOP),   "lbu" },  /* load byte unsigned.            */
    { OPC(LSBUOP),"ldstbu"},  /* load store byte unsigned.      */
    { OPC(LHOP),     "lh" },  /* load halfword sign extended.   */
    { OPC(LHUOP),   "lhu" },  /* load halfword unsigned.        */
    { OPC(LSHUOP),"ldsthu"},  /* load store halfword unsigned.  */
    { OPC(LWOP),     "lw" },  /* load word.                     */
    { OPC(LSWOP), "ldstw" }   /* load store word.               */
  };
  int dlx_load_opcode_num =
    (sizeof dlx_load_opcode) / (sizeof dlx_load_opcode[0]);
  int idx;

  for (idx = 0 ; idx < dlx_load_opcode_num; idx++)
    if (dlx_load_opcode[idx].opcode == opc)
      {
	if (opc == OPC (LHIOP))
	  {
	    (*info->fprintf_func) (info->stream, "%s", dlx_load_opcode[idx].name);
	    operand_deliminator (info, dlx_load_opcode[idx].name);
	    (*info->fprintf_func) (info->stream, "r%d,", (int)rs2);
	    (*info->fprintf_func) (info->stream, "0x%04x", (int)imm16);
	  }
	else
	  {
	    (*info->fprintf_func) (info->stream, "%s", dlx_load_opcode[idx].name);
	    operand_deliminator (info, dlx_load_opcode[idx].name);
	    (*info->fprintf_func) (info->stream, "r%d,", (int)rs2);
	    (*info->fprintf_func) (info->stream, "0x%04x[r%d]", (int)imm16, (int)rs1);
	  }

	return (unsigned char) ILD_TYPE;
    }

  return (unsigned char) NIL;
}

/* Process the memory store opcode.  */

static unsigned char
dlx_store_type (struct disassemble_info* info)
{
  struct _store_opcode
  {
    unsigned long opcode;
    char *name;
  }
  dlx_store_opcode[] =
  {
    { OPC(SBOP),     "sb" },  /* Store byte.      */
    { OPC(SHOP),     "sh" },  /* Store halfword.  */
    { OPC(SWOP),     "sw" },  /* Store word.      */
  };
  int dlx_store_opcode_num =
    (sizeof dlx_store_opcode) / (sizeof dlx_store_opcode[0]);
  int idx;

  for (idx = 0 ; idx < dlx_store_opcode_num; idx++)
    if (dlx_store_opcode[idx].opcode == opc)
      {
	(*info->fprintf_func) (info->stream, "%s", dlx_store_opcode[idx].name);
	operand_deliminator (info, dlx_store_opcode[idx].name);
	(*info->fprintf_func) (info->stream, "0x%04x[r%d],", (int)imm16, (int)rs1);
	(*info->fprintf_func) (info->stream, "r%d", (int)rs2);
	return (unsigned char) IST_TYPE;
      }

  return (unsigned char) NIL;
}

/* Process the Arithmetic and Logical I-TYPE opcode.  */

static unsigned char
dlx_aluI_type (struct disassemble_info* info)
{
  struct _aluI_opcode
  {
    unsigned long opcode;
    char *name;
  }
  dlx_aluI_opcode[] =
  {
    { OPC(ADDIOP),   "addi"  },  /* Store byte.      */
    { OPC(ADDUIOP),  "addui" },  /* Store halfword.  */
    { OPC(SUBIOP),   "subi"  },  /* Store word.      */
    { OPC(SUBUIOP),  "subui" },  /* Store word.      */
    { OPC(ANDIOP),   "andi"  },  /* Store word.      */
    { OPC(ORIOP),    "ori"   },  /* Store word.      */
    { OPC(XORIOP),   "xori"  },  /* Store word.      */
    { OPC(SLLIOP),   "slli"  },  /* Store word.      */
    { OPC(SRAIOP),   "srai"  },  /* Store word.      */
    { OPC(SRLIOP),   "srli"  },  /* Store word.      */
    { OPC(SEQIOP),   "seqi"  },  /* Store word.      */
    { OPC(SNEIOP),   "snei"  },  /* Store word.      */
    { OPC(SLTIOP),   "slti"  },  /* Store word.      */
    { OPC(SGTIOP),   "sgti"  },  /* Store word.      */
    { OPC(SLEIOP),   "slei"  },  /* Store word.      */
    { OPC(SGEIOP),   "sgei"  },  /* Store word.      */
    { OPC(SEQUIOP),  "sequi" },  /* Store word.      */
    { OPC(SNEUIOP),  "sneui" },  /* Store word.      */
    { OPC(SLTUIOP),  "sltui" },  /* Store word.      */
    { OPC(SGTUIOP),  "sgtui" },  /* Store word.      */
    { OPC(SLEUIOP),  "sleui" },  /* Store word.      */
    { OPC(SGEUIOP),  "sgeui" },  /* Store word.      */
#if 0						       
    { OPC(MVTSOP),   "mvts"  },  /* Store word.      */
    { OPC(MVFSOP),   "mvfs"  },  /* Store word.      */
#endif
  };
  int dlx_aluI_opcode_num =
    (sizeof dlx_aluI_opcode) / (sizeof dlx_aluI_opcode[0]);
  int idx;

  for (idx = 0 ; idx < dlx_aluI_opcode_num; idx++)
    if (dlx_aluI_opcode[idx].opcode == opc)
      {
	(*info->fprintf_func) (info->stream, "%s", dlx_aluI_opcode[idx].name);
	operand_deliminator (info, dlx_aluI_opcode[idx].name);
	(*info->fprintf_func) (info->stream, "r%d,", (int)rs2);
	(*info->fprintf_func) (info->stream, "r%d,", (int)rs1);
	(*info->fprintf_func) (info->stream, "0x%04x", (int)imm16);

	return (unsigned char) IAL_TYPE;
      }

  return (unsigned char) NIL;
}

/* Process the branch instruction.  */

static unsigned char
dlx_br_type (struct disassemble_info* info)
{
  struct _br_opcode
  {
    unsigned long opcode;
    char *name;
  }
  dlx_br_opcode[] =
  {
    { OPC(BEQOP), "beqz" }, /* Store byte.  */
    { OPC(BNEOP), "bnez" }  /* Store halfword.  */
  };
  int dlx_br_opcode_num =
    (sizeof dlx_br_opcode) / (sizeof dlx_br_opcode[0]);
  int idx;

  for (idx = 0 ; idx < dlx_br_opcode_num; idx++)
    if (dlx_br_opcode[idx].opcode == opc)
      {
	if (imm16 & 0x00008000)
	  imm16 |= 0xFFFF0000;

	imm16 += (current_insn_addr + 4);
	(*info->fprintf_func) (info->stream, "%s", dlx_br_opcode[idx].name);
	operand_deliminator (info, dlx_br_opcode[idx].name);
	(*info->fprintf_func) (info->stream, "r%d,", (int) rs1);
	(*info->fprintf_func) (info->stream, "0x%08x", (int) imm16);

	return (unsigned char) IBR_TYPE;
      }

  return (unsigned char) NIL;
}

/* Process the jump instruction.  */

static unsigned char
dlx_jmp_type (struct disassemble_info* info)
{
  struct _jmp_opcode
  {
    unsigned long opcode;
    char *name;
  }
  dlx_jmp_opcode[] =
  {
    { OPC(JOP),         "j" },  /* Store byte.      */
    { OPC(JALOP),     "jal" },  /* Store halfword.  */
    { OPC(BREAKOP), "break" },  /* Store halfword.  */
    { OPC(TRAPOP),   "trap" },  /* Store halfword.  */
    { OPC(RFEOP),     "rfe" }   /* Store halfword.  */
  };
  int dlx_jmp_opcode_num =
    (sizeof dlx_jmp_opcode) / (sizeof dlx_jmp_opcode[0]);
  int idx;

  for (idx = 0 ; idx < dlx_jmp_opcode_num; idx++)
    if (dlx_jmp_opcode[idx].opcode == opc)
      {
	if (imm26 & 0x02000000)
	  imm26 |= 0xFC000000;

	imm26 += (current_insn_addr + 4);

	(*info->fprintf_func) (info->stream, "%s", dlx_jmp_opcode[idx].name);
	operand_deliminator (info, dlx_jmp_opcode[idx].name);
	(*info->fprintf_func) (info->stream, "0x%08x", (int)imm26);

	return (unsigned char) IJ_TYPE;
      }

  return (unsigned char) NIL;
}

/* Process the jump register instruction.  */

static unsigned char
dlx_jr_type (struct disassemble_info* info)
{
  struct _jr_opcode
  {
    unsigned long opcode;
    char *name;
  }
  dlx_jr_opcode[] =
  {
    { OPC(JROP),   "jr"    },  /* Store byte.  */
    { OPC(JALROP), "jalr"  }   /* Store halfword.  */
  };
  int dlx_jr_opcode_num =
    (sizeof dlx_jr_opcode) / (sizeof dlx_jr_opcode[0]);
  int idx;

  for (idx = 0 ; idx < dlx_jr_opcode_num; idx++)
    if (dlx_jr_opcode[idx].opcode == opc)
      {
	(*info->fprintf_func) (info->stream, "%s", dlx_jr_opcode[idx].name);
	operand_deliminator (info, dlx_jr_opcode[idx].name);
	(*info->fprintf_func) (info->stream, "r%d", (int)rs1);
	return (unsigned char) IJR_TYPE;
      }

  return (unsigned char) NIL;
}

typedef unsigned char (* dlx_insn) (struct disassemble_info *);

/* This is the main DLX insn handling routine.  */

int
print_insn_dlx (bfd_vma memaddr, struct disassemble_info* info)
{
  bfd_byte buffer[4];
  int insn_idx;
  unsigned long insn_word;
  unsigned char rtn_code;
  unsigned long dlx_insn_type[] =
  {
    (unsigned long) dlx_r_type,
    (unsigned long) dlx_load_type,
    (unsigned long) dlx_store_type,
    (unsigned long) dlx_aluI_type,
    (unsigned long) dlx_br_type,
    (unsigned long) dlx_jmp_type,
    (unsigned long) dlx_jr_type,
    (unsigned long) NULL
  };
  int dlx_insn_type_num = ((sizeof dlx_insn_type) / (sizeof (unsigned long))) - 1;
  int status =
    (*info->read_memory_func) (memaddr, (bfd_byte *) &buffer[0], 4, info);

  if (status != 0)
    {
      (*info->memory_error_func) (status, memaddr, info);
      return -1;
    }

  /* Now decode the insn    */
  insn_word = bfd_getb32 (buffer);
  opc  = dlx_get_opcode (insn_word);
  rs1  = dlx_get_rs1 (insn_word);
  rs2  = dlx_get_rs2 (insn_word);
  rd   = dlx_get_rdR (insn_word);
  func = dlx_get_func (insn_word);
  imm16= dlx_get_imm16 (insn_word);
  imm26= dlx_get_imm26 (insn_word);

#if 0
  printf ("print_insn_big_dlx: opc = 0x%02x\n"
	  "                    rs1 = 0x%02x\n"
	  "                    rs2 = 0x%02x\n"
	  "                    rd  = 0x%02x\n"
	  "                  func  = 0x%08x\n"
	  "                 imm16  = 0x%08x\n"
	  "                 imm26  = 0x%08x\n",
	  opc, rs1, rs2, rd, func, imm16, imm26);
#endif

  /* Scan through all the insn type and print the insn out.  */
  rtn_code = 0;
  current_insn_addr = (unsigned long) memaddr;

  for (insn_idx = 0; dlx_insn_type[insn_idx] != 0x0; insn_idx++)
    switch (((dlx_insn) (dlx_insn_type[insn_idx])) (info))
      {
	/* Found the correct opcode   */
      case R_TYPE:
      case ILD_TYPE:
      case IST_TYPE:
      case IAL_TYPE:
      case IBR_TYPE:
      case IJ_TYPE:
      case IJR_TYPE:
	return 4;

	/* Wrong insn type check next one. */
      default:
      case NIL:
	continue;

	/* All rest of the return code are not recongnized, treat it as error */
	/* we should never get here,  I hope! */
      case R_ERROR:
	return -1;
      }

  if (insn_idx ==  dlx_insn_type_num)
    /* Well, does not recoganize this opcode.  */
    (*info->fprintf_func) (info->stream, "<%s>", "Unrecognized Opcode");

  return 4;
}
