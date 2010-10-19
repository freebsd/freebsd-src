/* Semantic operand instances for m32r.

THIS FILE IS MACHINE GENERATED WITH CGEN.

Copyright 1996-2005 Free Software Foundation, Inc.

This file is part of the GNU Binutils and/or GDB, the GNU debugger.

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2, or (at your option)
any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License along
with this program; if not, write to the Free Software Foundation, Inc.,
51 Franklin Street - Fifth Floor, Boston, MA 02110-1301, USA.

*/

#include "sysdep.h"
#include "ansidecl.h"
#include "bfd.h"
#include "symcat.h"
#include "m32r-desc.h"
#include "m32r-opc.h"

/* Operand references.  */

#if defined (__STDC__) || defined (ALMOST_STDC) || defined (HAVE_STRINGIZE)
#define OP_ENT(op) M32R_OPERAND_##op
#else
#define OP_ENT(op) M32R_OPERAND_/**/op
#endif
#define INPUT CGEN_OPINST_INPUT
#define OUTPUT CGEN_OPINST_OUTPUT
#define END CGEN_OPINST_END
#define COND_REF CGEN_OPINST_COND_REF

static const CGEN_OPINST sfmt_empty_ops[] ATTRIBUTE_UNUSED = {
  { END, (const char *)0, (enum cgen_hw_type)0, (enum cgen_mode)0, (enum cgen_operand_type)0, 0, 0 }
};

static const CGEN_OPINST sfmt_add_ops[] ATTRIBUTE_UNUSED = {
  { INPUT, "dr", HW_H_GR, CGEN_MODE_SI, OP_ENT (DR), 0, 0 },
  { INPUT, "sr", HW_H_GR, CGEN_MODE_SI, OP_ENT (SR), 0, 0 },
  { OUTPUT, "dr", HW_H_GR, CGEN_MODE_SI, OP_ENT (DR), 0, 0 },
  { END, (const char *)0, (enum cgen_hw_type)0, (enum cgen_mode)0, (enum cgen_operand_type)0, 0, 0 }
};

static const CGEN_OPINST sfmt_add3_ops[] ATTRIBUTE_UNUSED = {
  { INPUT, "slo16", HW_H_SLO16, CGEN_MODE_INT, OP_ENT (SLO16), 0, 0 },
  { INPUT, "sr", HW_H_GR, CGEN_MODE_SI, OP_ENT (SR), 0, 0 },
  { OUTPUT, "dr", HW_H_GR, CGEN_MODE_SI, OP_ENT (DR), 0, 0 },
  { END, (const char *)0, (enum cgen_hw_type)0, (enum cgen_mode)0, (enum cgen_operand_type)0, 0, 0 }
};

static const CGEN_OPINST sfmt_and3_ops[] ATTRIBUTE_UNUSED = {
  { INPUT, "sr", HW_H_GR, CGEN_MODE_SI, OP_ENT (SR), 0, 0 },
  { INPUT, "uimm16", HW_H_UINT, CGEN_MODE_UINT, OP_ENT (UIMM16), 0, 0 },
  { OUTPUT, "dr", HW_H_GR, CGEN_MODE_SI, OP_ENT (DR), 0, 0 },
  { END, (const char *)0, (enum cgen_hw_type)0, (enum cgen_mode)0, (enum cgen_operand_type)0, 0, 0 }
};

static const CGEN_OPINST sfmt_or3_ops[] ATTRIBUTE_UNUSED = {
  { INPUT, "sr", HW_H_GR, CGEN_MODE_SI, OP_ENT (SR), 0, 0 },
  { INPUT, "ulo16", HW_H_ULO16, CGEN_MODE_UINT, OP_ENT (ULO16), 0, 0 },
  { OUTPUT, "dr", HW_H_GR, CGEN_MODE_SI, OP_ENT (DR), 0, 0 },
  { END, (const char *)0, (enum cgen_hw_type)0, (enum cgen_mode)0, (enum cgen_operand_type)0, 0, 0 }
};

static const CGEN_OPINST sfmt_addi_ops[] ATTRIBUTE_UNUSED = {
  { INPUT, "dr", HW_H_GR, CGEN_MODE_SI, OP_ENT (DR), 0, 0 },
  { INPUT, "simm8", HW_H_SINT, CGEN_MODE_INT, OP_ENT (SIMM8), 0, 0 },
  { OUTPUT, "dr", HW_H_GR, CGEN_MODE_SI, OP_ENT (DR), 0, 0 },
  { END, (const char *)0, (enum cgen_hw_type)0, (enum cgen_mode)0, (enum cgen_operand_type)0, 0, 0 }
};

static const CGEN_OPINST sfmt_addv_ops[] ATTRIBUTE_UNUSED = {
  { INPUT, "dr", HW_H_GR, CGEN_MODE_SI, OP_ENT (DR), 0, 0 },
  { INPUT, "sr", HW_H_GR, CGEN_MODE_SI, OP_ENT (SR), 0, 0 },
  { OUTPUT, "condbit", HW_H_COND, CGEN_MODE_BI, 0, 0, 0 },
  { OUTPUT, "dr", HW_H_GR, CGEN_MODE_SI, OP_ENT (DR), 0, 0 },
  { END, (const char *)0, (enum cgen_hw_type)0, (enum cgen_mode)0, (enum cgen_operand_type)0, 0, 0 }
};

static const CGEN_OPINST sfmt_addv3_ops[] ATTRIBUTE_UNUSED = {
  { INPUT, "simm16", HW_H_SINT, CGEN_MODE_INT, OP_ENT (SIMM16), 0, 0 },
  { INPUT, "sr", HW_H_GR, CGEN_MODE_SI, OP_ENT (SR), 0, 0 },
  { OUTPUT, "condbit", HW_H_COND, CGEN_MODE_BI, 0, 0, 0 },
  { OUTPUT, "dr", HW_H_GR, CGEN_MODE_SI, OP_ENT (DR), 0, 0 },
  { END, (const char *)0, (enum cgen_hw_type)0, (enum cgen_mode)0, (enum cgen_operand_type)0, 0, 0 }
};

static const CGEN_OPINST sfmt_addx_ops[] ATTRIBUTE_UNUSED = {
  { INPUT, "condbit", HW_H_COND, CGEN_MODE_BI, 0, 0, 0 },
  { INPUT, "dr", HW_H_GR, CGEN_MODE_SI, OP_ENT (DR), 0, 0 },
  { INPUT, "sr", HW_H_GR, CGEN_MODE_SI, OP_ENT (SR), 0, 0 },
  { OUTPUT, "condbit", HW_H_COND, CGEN_MODE_BI, 0, 0, 0 },
  { OUTPUT, "dr", HW_H_GR, CGEN_MODE_SI, OP_ENT (DR), 0, 0 },
  { END, (const char *)0, (enum cgen_hw_type)0, (enum cgen_mode)0, (enum cgen_operand_type)0, 0, 0 }
};

static const CGEN_OPINST sfmt_bc8_ops[] ATTRIBUTE_UNUSED = {
  { INPUT, "condbit", HW_H_COND, CGEN_MODE_BI, 0, 0, 0 },
  { INPUT, "disp8", HW_H_IADDR, CGEN_MODE_USI, OP_ENT (DISP8), 0, COND_REF },
  { OUTPUT, "pc", HW_H_PC, CGEN_MODE_USI, 0, 0, COND_REF },
  { END, (const char *)0, (enum cgen_hw_type)0, (enum cgen_mode)0, (enum cgen_operand_type)0, 0, 0 }
};

static const CGEN_OPINST sfmt_bc24_ops[] ATTRIBUTE_UNUSED = {
  { INPUT, "condbit", HW_H_COND, CGEN_MODE_BI, 0, 0, 0 },
  { INPUT, "disp24", HW_H_IADDR, CGEN_MODE_USI, OP_ENT (DISP24), 0, COND_REF },
  { OUTPUT, "pc", HW_H_PC, CGEN_MODE_USI, 0, 0, COND_REF },
  { END, (const char *)0, (enum cgen_hw_type)0, (enum cgen_mode)0, (enum cgen_operand_type)0, 0, 0 }
};

static const CGEN_OPINST sfmt_beq_ops[] ATTRIBUTE_UNUSED = {
  { INPUT, "disp16", HW_H_IADDR, CGEN_MODE_USI, OP_ENT (DISP16), 0, COND_REF },
  { INPUT, "src1", HW_H_GR, CGEN_MODE_SI, OP_ENT (SRC1), 0, 0 },
  { INPUT, "src2", HW_H_GR, CGEN_MODE_SI, OP_ENT (SRC2), 0, 0 },
  { OUTPUT, "pc", HW_H_PC, CGEN_MODE_USI, 0, 0, COND_REF },
  { END, (const char *)0, (enum cgen_hw_type)0, (enum cgen_mode)0, (enum cgen_operand_type)0, 0, 0 }
};

static const CGEN_OPINST sfmt_beqz_ops[] ATTRIBUTE_UNUSED = {
  { INPUT, "disp16", HW_H_IADDR, CGEN_MODE_USI, OP_ENT (DISP16), 0, COND_REF },
  { INPUT, "src2", HW_H_GR, CGEN_MODE_SI, OP_ENT (SRC2), 0, 0 },
  { OUTPUT, "pc", HW_H_PC, CGEN_MODE_USI, 0, 0, COND_REF },
  { END, (const char *)0, (enum cgen_hw_type)0, (enum cgen_mode)0, (enum cgen_operand_type)0, 0, 0 }
};

static const CGEN_OPINST sfmt_bl8_ops[] ATTRIBUTE_UNUSED = {
  { INPUT, "disp8", HW_H_IADDR, CGEN_MODE_USI, OP_ENT (DISP8), 0, 0 },
  { INPUT, "pc", HW_H_PC, CGEN_MODE_USI, 0, 0, 0 },
  { OUTPUT, "h_gr_SI_14", HW_H_GR, CGEN_MODE_SI, 0, 14, 0 },
  { OUTPUT, "pc", HW_H_PC, CGEN_MODE_USI, 0, 0, 0 },
  { END, (const char *)0, (enum cgen_hw_type)0, (enum cgen_mode)0, (enum cgen_operand_type)0, 0, 0 }
};

static const CGEN_OPINST sfmt_bl24_ops[] ATTRIBUTE_UNUSED = {
  { INPUT, "disp24", HW_H_IADDR, CGEN_MODE_USI, OP_ENT (DISP24), 0, 0 },
  { INPUT, "pc", HW_H_PC, CGEN_MODE_USI, 0, 0, 0 },
  { OUTPUT, "h_gr_SI_14", HW_H_GR, CGEN_MODE_SI, 0, 14, 0 },
  { OUTPUT, "pc", HW_H_PC, CGEN_MODE_USI, 0, 0, 0 },
  { END, (const char *)0, (enum cgen_hw_type)0, (enum cgen_mode)0, (enum cgen_operand_type)0, 0, 0 }
};

static const CGEN_OPINST sfmt_bcl8_ops[] ATTRIBUTE_UNUSED = {
  { INPUT, "condbit", HW_H_COND, CGEN_MODE_BI, 0, 0, 0 },
  { INPUT, "disp8", HW_H_IADDR, CGEN_MODE_USI, OP_ENT (DISP8), 0, COND_REF },
  { INPUT, "pc", HW_H_PC, CGEN_MODE_USI, 0, 0, COND_REF },
  { OUTPUT, "h_gr_SI_14", HW_H_GR, CGEN_MODE_SI, 0, 14, COND_REF },
  { OUTPUT, "pc", HW_H_PC, CGEN_MODE_USI, 0, 0, COND_REF },
  { END, (const char *)0, (enum cgen_hw_type)0, (enum cgen_mode)0, (enum cgen_operand_type)0, 0, 0 }
};

static const CGEN_OPINST sfmt_bcl24_ops[] ATTRIBUTE_UNUSED = {
  { INPUT, "condbit", HW_H_COND, CGEN_MODE_BI, 0, 0, 0 },
  { INPUT, "disp24", HW_H_IADDR, CGEN_MODE_USI, OP_ENT (DISP24), 0, COND_REF },
  { INPUT, "pc", HW_H_PC, CGEN_MODE_USI, 0, 0, COND_REF },
  { OUTPUT, "h_gr_SI_14", HW_H_GR, CGEN_MODE_SI, 0, 14, COND_REF },
  { OUTPUT, "pc", HW_H_PC, CGEN_MODE_USI, 0, 0, COND_REF },
  { END, (const char *)0, (enum cgen_hw_type)0, (enum cgen_mode)0, (enum cgen_operand_type)0, 0, 0 }
};

static const CGEN_OPINST sfmt_bra8_ops[] ATTRIBUTE_UNUSED = {
  { INPUT, "disp8", HW_H_IADDR, CGEN_MODE_USI, OP_ENT (DISP8), 0, 0 },
  { OUTPUT, "pc", HW_H_PC, CGEN_MODE_USI, 0, 0, 0 },
  { END, (const char *)0, (enum cgen_hw_type)0, (enum cgen_mode)0, (enum cgen_operand_type)0, 0, 0 }
};

static const CGEN_OPINST sfmt_bra24_ops[] ATTRIBUTE_UNUSED = {
  { INPUT, "disp24", HW_H_IADDR, CGEN_MODE_USI, OP_ENT (DISP24), 0, 0 },
  { OUTPUT, "pc", HW_H_PC, CGEN_MODE_USI, 0, 0, 0 },
  { END, (const char *)0, (enum cgen_hw_type)0, (enum cgen_mode)0, (enum cgen_operand_type)0, 0, 0 }
};

static const CGEN_OPINST sfmt_cmp_ops[] ATTRIBUTE_UNUSED = {
  { INPUT, "src1", HW_H_GR, CGEN_MODE_SI, OP_ENT (SRC1), 0, 0 },
  { INPUT, "src2", HW_H_GR, CGEN_MODE_SI, OP_ENT (SRC2), 0, 0 },
  { OUTPUT, "condbit", HW_H_COND, CGEN_MODE_BI, 0, 0, 0 },
  { END, (const char *)0, (enum cgen_hw_type)0, (enum cgen_mode)0, (enum cgen_operand_type)0, 0, 0 }
};

static const CGEN_OPINST sfmt_cmpi_ops[] ATTRIBUTE_UNUSED = {
  { INPUT, "simm16", HW_H_SINT, CGEN_MODE_INT, OP_ENT (SIMM16), 0, 0 },
  { INPUT, "src2", HW_H_GR, CGEN_MODE_SI, OP_ENT (SRC2), 0, 0 },
  { OUTPUT, "condbit", HW_H_COND, CGEN_MODE_BI, 0, 0, 0 },
  { END, (const char *)0, (enum cgen_hw_type)0, (enum cgen_mode)0, (enum cgen_operand_type)0, 0, 0 }
};

static const CGEN_OPINST sfmt_cmpz_ops[] ATTRIBUTE_UNUSED = {
  { INPUT, "src2", HW_H_GR, CGEN_MODE_SI, OP_ENT (SRC2), 0, 0 },
  { OUTPUT, "condbit", HW_H_COND, CGEN_MODE_BI, 0, 0, 0 },
  { END, (const char *)0, (enum cgen_hw_type)0, (enum cgen_mode)0, (enum cgen_operand_type)0, 0, 0 }
};

static const CGEN_OPINST sfmt_div_ops[] ATTRIBUTE_UNUSED = {
  { INPUT, "dr", HW_H_GR, CGEN_MODE_SI, OP_ENT (DR), 0, COND_REF },
  { INPUT, "sr", HW_H_GR, CGEN_MODE_SI, OP_ENT (SR), 0, 0 },
  { OUTPUT, "dr", HW_H_GR, CGEN_MODE_SI, OP_ENT (DR), 0, COND_REF },
  { END, (const char *)0, (enum cgen_hw_type)0, (enum cgen_mode)0, (enum cgen_operand_type)0, 0, 0 }
};

static const CGEN_OPINST sfmt_jc_ops[] ATTRIBUTE_UNUSED = {
  { INPUT, "condbit", HW_H_COND, CGEN_MODE_BI, 0, 0, 0 },
  { INPUT, "sr", HW_H_GR, CGEN_MODE_SI, OP_ENT (SR), 0, COND_REF },
  { OUTPUT, "pc", HW_H_PC, CGEN_MODE_USI, 0, 0, COND_REF },
  { END, (const char *)0, (enum cgen_hw_type)0, (enum cgen_mode)0, (enum cgen_operand_type)0, 0, 0 }
};

static const CGEN_OPINST sfmt_jl_ops[] ATTRIBUTE_UNUSED = {
  { INPUT, "pc", HW_H_PC, CGEN_MODE_USI, 0, 0, 0 },
  { INPUT, "sr", HW_H_GR, CGEN_MODE_SI, OP_ENT (SR), 0, 0 },
  { OUTPUT, "h_gr_SI_14", HW_H_GR, CGEN_MODE_SI, 0, 14, 0 },
  { OUTPUT, "pc", HW_H_PC, CGEN_MODE_USI, 0, 0, 0 },
  { END, (const char *)0, (enum cgen_hw_type)0, (enum cgen_mode)0, (enum cgen_operand_type)0, 0, 0 }
};

static const CGEN_OPINST sfmt_jmp_ops[] ATTRIBUTE_UNUSED = {
  { INPUT, "sr", HW_H_GR, CGEN_MODE_SI, OP_ENT (SR), 0, 0 },
  { OUTPUT, "pc", HW_H_PC, CGEN_MODE_USI, 0, 0, 0 },
  { END, (const char *)0, (enum cgen_hw_type)0, (enum cgen_mode)0, (enum cgen_operand_type)0, 0, 0 }
};

static const CGEN_OPINST sfmt_ld_ops[] ATTRIBUTE_UNUSED = {
  { INPUT, "h_memory_SI_sr", HW_H_MEMORY, CGEN_MODE_SI, 0, 0, 0 },
  { INPUT, "sr", HW_H_GR, CGEN_MODE_USI, OP_ENT (SR), 0, 0 },
  { OUTPUT, "dr", HW_H_GR, CGEN_MODE_SI, OP_ENT (DR), 0, 0 },
  { END, (const char *)0, (enum cgen_hw_type)0, (enum cgen_mode)0, (enum cgen_operand_type)0, 0, 0 }
};

static const CGEN_OPINST sfmt_ld_d_ops[] ATTRIBUTE_UNUSED = {
  { INPUT, "h_memory_SI_add__DFLT_sr_slo16", HW_H_MEMORY, CGEN_MODE_SI, 0, 0, 0 },
  { INPUT, "slo16", HW_H_SLO16, CGEN_MODE_INT, OP_ENT (SLO16), 0, 0 },
  { INPUT, "sr", HW_H_GR, CGEN_MODE_SI, OP_ENT (SR), 0, 0 },
  { OUTPUT, "dr", HW_H_GR, CGEN_MODE_SI, OP_ENT (DR), 0, 0 },
  { END, (const char *)0, (enum cgen_hw_type)0, (enum cgen_mode)0, (enum cgen_operand_type)0, 0, 0 }
};

static const CGEN_OPINST sfmt_ldb_ops[] ATTRIBUTE_UNUSED = {
  { INPUT, "h_memory_QI_sr", HW_H_MEMORY, CGEN_MODE_QI, 0, 0, 0 },
  { INPUT, "sr", HW_H_GR, CGEN_MODE_USI, OP_ENT (SR), 0, 0 },
  { OUTPUT, "dr", HW_H_GR, CGEN_MODE_SI, OP_ENT (DR), 0, 0 },
  { END, (const char *)0, (enum cgen_hw_type)0, (enum cgen_mode)0, (enum cgen_operand_type)0, 0, 0 }
};

static const CGEN_OPINST sfmt_ldb_d_ops[] ATTRIBUTE_UNUSED = {
  { INPUT, "h_memory_QI_add__DFLT_sr_slo16", HW_H_MEMORY, CGEN_MODE_QI, 0, 0, 0 },
  { INPUT, "slo16", HW_H_SLO16, CGEN_MODE_INT, OP_ENT (SLO16), 0, 0 },
  { INPUT, "sr", HW_H_GR, CGEN_MODE_SI, OP_ENT (SR), 0, 0 },
  { OUTPUT, "dr", HW_H_GR, CGEN_MODE_SI, OP_ENT (DR), 0, 0 },
  { END, (const char *)0, (enum cgen_hw_type)0, (enum cgen_mode)0, (enum cgen_operand_type)0, 0, 0 }
};

static const CGEN_OPINST sfmt_ldh_ops[] ATTRIBUTE_UNUSED = {
  { INPUT, "h_memory_HI_sr", HW_H_MEMORY, CGEN_MODE_HI, 0, 0, 0 },
  { INPUT, "sr", HW_H_GR, CGEN_MODE_USI, OP_ENT (SR), 0, 0 },
  { OUTPUT, "dr", HW_H_GR, CGEN_MODE_SI, OP_ENT (DR), 0, 0 },
  { END, (const char *)0, (enum cgen_hw_type)0, (enum cgen_mode)0, (enum cgen_operand_type)0, 0, 0 }
};

static const CGEN_OPINST sfmt_ldh_d_ops[] ATTRIBUTE_UNUSED = {
  { INPUT, "h_memory_HI_add__DFLT_sr_slo16", HW_H_MEMORY, CGEN_MODE_HI, 0, 0, 0 },
  { INPUT, "slo16", HW_H_SLO16, CGEN_MODE_INT, OP_ENT (SLO16), 0, 0 },
  { INPUT, "sr", HW_H_GR, CGEN_MODE_SI, OP_ENT (SR), 0, 0 },
  { OUTPUT, "dr", HW_H_GR, CGEN_MODE_SI, OP_ENT (DR), 0, 0 },
  { END, (const char *)0, (enum cgen_hw_type)0, (enum cgen_mode)0, (enum cgen_operand_type)0, 0, 0 }
};

static const CGEN_OPINST sfmt_ld_plus_ops[] ATTRIBUTE_UNUSED = {
  { INPUT, "h_memory_SI_sr", HW_H_MEMORY, CGEN_MODE_SI, 0, 0, 0 },
  { INPUT, "sr", HW_H_GR, CGEN_MODE_USI, OP_ENT (SR), 0, 0 },
  { OUTPUT, "dr", HW_H_GR, CGEN_MODE_SI, OP_ENT (DR), 0, 0 },
  { OUTPUT, "sr", HW_H_GR, CGEN_MODE_SI, OP_ENT (SR), 0, 0 },
  { END, (const char *)0, (enum cgen_hw_type)0, (enum cgen_mode)0, (enum cgen_operand_type)0, 0, 0 }
};

static const CGEN_OPINST sfmt_ld24_ops[] ATTRIBUTE_UNUSED = {
  { INPUT, "uimm24", HW_H_ADDR, CGEN_MODE_USI, OP_ENT (UIMM24), 0, 0 },
  { OUTPUT, "dr", HW_H_GR, CGEN_MODE_SI, OP_ENT (DR), 0, 0 },
  { END, (const char *)0, (enum cgen_hw_type)0, (enum cgen_mode)0, (enum cgen_operand_type)0, 0, 0 }
};

static const CGEN_OPINST sfmt_ldi8_ops[] ATTRIBUTE_UNUSED = {
  { INPUT, "simm8", HW_H_SINT, CGEN_MODE_INT, OP_ENT (SIMM8), 0, 0 },
  { OUTPUT, "dr", HW_H_GR, CGEN_MODE_SI, OP_ENT (DR), 0, 0 },
  { END, (const char *)0, (enum cgen_hw_type)0, (enum cgen_mode)0, (enum cgen_operand_type)0, 0, 0 }
};

static const CGEN_OPINST sfmt_ldi16_ops[] ATTRIBUTE_UNUSED = {
  { INPUT, "slo16", HW_H_SLO16, CGEN_MODE_INT, OP_ENT (SLO16), 0, 0 },
  { OUTPUT, "dr", HW_H_GR, CGEN_MODE_SI, OP_ENT (DR), 0, 0 },
  { END, (const char *)0, (enum cgen_hw_type)0, (enum cgen_mode)0, (enum cgen_operand_type)0, 0, 0 }
};

static const CGEN_OPINST sfmt_lock_ops[] ATTRIBUTE_UNUSED = {
  { INPUT, "h_memory_SI_sr", HW_H_MEMORY, CGEN_MODE_SI, 0, 0, 0 },
  { INPUT, "sr", HW_H_GR, CGEN_MODE_USI, OP_ENT (SR), 0, 0 },
  { OUTPUT, "dr", HW_H_GR, CGEN_MODE_SI, OP_ENT (DR), 0, 0 },
  { OUTPUT, "h_lock_BI", HW_H_LOCK, CGEN_MODE_BI, 0, 0, 0 },
  { END, (const char *)0, (enum cgen_hw_type)0, (enum cgen_mode)0, (enum cgen_operand_type)0, 0, 0 }
};

static const CGEN_OPINST sfmt_machi_ops[] ATTRIBUTE_UNUSED = {
  { INPUT, "accum", HW_H_ACCUM, CGEN_MODE_DI, 0, 0, 0 },
  { INPUT, "src1", HW_H_GR, CGEN_MODE_SI, OP_ENT (SRC1), 0, 0 },
  { INPUT, "src2", HW_H_GR, CGEN_MODE_SI, OP_ENT (SRC2), 0, 0 },
  { OUTPUT, "accum", HW_H_ACCUM, CGEN_MODE_DI, 0, 0, 0 },
  { END, (const char *)0, (enum cgen_hw_type)0, (enum cgen_mode)0, (enum cgen_operand_type)0, 0, 0 }
};

static const CGEN_OPINST sfmt_machi_a_ops[] ATTRIBUTE_UNUSED = {
  { INPUT, "acc", HW_H_ACCUMS, CGEN_MODE_DI, OP_ENT (ACC), 0, 0 },
  { INPUT, "src1", HW_H_GR, CGEN_MODE_SI, OP_ENT (SRC1), 0, 0 },
  { INPUT, "src2", HW_H_GR, CGEN_MODE_SI, OP_ENT (SRC2), 0, 0 },
  { OUTPUT, "acc", HW_H_ACCUMS, CGEN_MODE_DI, OP_ENT (ACC), 0, 0 },
  { END, (const char *)0, (enum cgen_hw_type)0, (enum cgen_mode)0, (enum cgen_operand_type)0, 0, 0 }
};

static const CGEN_OPINST sfmt_mulhi_ops[] ATTRIBUTE_UNUSED = {
  { INPUT, "src1", HW_H_GR, CGEN_MODE_SI, OP_ENT (SRC1), 0, 0 },
  { INPUT, "src2", HW_H_GR, CGEN_MODE_SI, OP_ENT (SRC2), 0, 0 },
  { OUTPUT, "accum", HW_H_ACCUM, CGEN_MODE_DI, 0, 0, 0 },
  { END, (const char *)0, (enum cgen_hw_type)0, (enum cgen_mode)0, (enum cgen_operand_type)0, 0, 0 }
};

static const CGEN_OPINST sfmt_mulhi_a_ops[] ATTRIBUTE_UNUSED = {
  { INPUT, "src1", HW_H_GR, CGEN_MODE_SI, OP_ENT (SRC1), 0, 0 },
  { INPUT, "src2", HW_H_GR, CGEN_MODE_SI, OP_ENT (SRC2), 0, 0 },
  { OUTPUT, "acc", HW_H_ACCUMS, CGEN_MODE_DI, OP_ENT (ACC), 0, 0 },
  { END, (const char *)0, (enum cgen_hw_type)0, (enum cgen_mode)0, (enum cgen_operand_type)0, 0, 0 }
};

static const CGEN_OPINST sfmt_mv_ops[] ATTRIBUTE_UNUSED = {
  { INPUT, "sr", HW_H_GR, CGEN_MODE_SI, OP_ENT (SR), 0, 0 },
  { OUTPUT, "dr", HW_H_GR, CGEN_MODE_SI, OP_ENT (DR), 0, 0 },
  { END, (const char *)0, (enum cgen_hw_type)0, (enum cgen_mode)0, (enum cgen_operand_type)0, 0, 0 }
};

static const CGEN_OPINST sfmt_mvfachi_ops[] ATTRIBUTE_UNUSED = {
  { INPUT, "accum", HW_H_ACCUM, CGEN_MODE_DI, 0, 0, 0 },
  { OUTPUT, "dr", HW_H_GR, CGEN_MODE_SI, OP_ENT (DR), 0, 0 },
  { END, (const char *)0, (enum cgen_hw_type)0, (enum cgen_mode)0, (enum cgen_operand_type)0, 0, 0 }
};

static const CGEN_OPINST sfmt_mvfachi_a_ops[] ATTRIBUTE_UNUSED = {
  { INPUT, "accs", HW_H_ACCUMS, CGEN_MODE_DI, OP_ENT (ACCS), 0, 0 },
  { OUTPUT, "dr", HW_H_GR, CGEN_MODE_SI, OP_ENT (DR), 0, 0 },
  { END, (const char *)0, (enum cgen_hw_type)0, (enum cgen_mode)0, (enum cgen_operand_type)0, 0, 0 }
};

static const CGEN_OPINST sfmt_mvfc_ops[] ATTRIBUTE_UNUSED = {
  { INPUT, "scr", HW_H_CR, CGEN_MODE_USI, OP_ENT (SCR), 0, 0 },
  { OUTPUT, "dr", HW_H_GR, CGEN_MODE_SI, OP_ENT (DR), 0, 0 },
  { END, (const char *)0, (enum cgen_hw_type)0, (enum cgen_mode)0, (enum cgen_operand_type)0, 0, 0 }
};

static const CGEN_OPINST sfmt_mvtachi_ops[] ATTRIBUTE_UNUSED = {
  { INPUT, "accum", HW_H_ACCUM, CGEN_MODE_DI, 0, 0, 0 },
  { INPUT, "src1", HW_H_GR, CGEN_MODE_SI, OP_ENT (SRC1), 0, 0 },
  { OUTPUT, "accum", HW_H_ACCUM, CGEN_MODE_DI, 0, 0, 0 },
  { END, (const char *)0, (enum cgen_hw_type)0, (enum cgen_mode)0, (enum cgen_operand_type)0, 0, 0 }
};

static const CGEN_OPINST sfmt_mvtachi_a_ops[] ATTRIBUTE_UNUSED = {
  { INPUT, "accs", HW_H_ACCUMS, CGEN_MODE_DI, OP_ENT (ACCS), 0, 0 },
  { INPUT, "src1", HW_H_GR, CGEN_MODE_SI, OP_ENT (SRC1), 0, 0 },
  { OUTPUT, "accs", HW_H_ACCUMS, CGEN_MODE_DI, OP_ENT (ACCS), 0, 0 },
  { END, (const char *)0, (enum cgen_hw_type)0, (enum cgen_mode)0, (enum cgen_operand_type)0, 0, 0 }
};

static const CGEN_OPINST sfmt_mvtc_ops[] ATTRIBUTE_UNUSED = {
  { INPUT, "sr", HW_H_GR, CGEN_MODE_SI, OP_ENT (SR), 0, 0 },
  { OUTPUT, "dcr", HW_H_CR, CGEN_MODE_USI, OP_ENT (DCR), 0, 0 },
  { END, (const char *)0, (enum cgen_hw_type)0, (enum cgen_mode)0, (enum cgen_operand_type)0, 0, 0 }
};

static const CGEN_OPINST sfmt_nop_ops[] ATTRIBUTE_UNUSED = {
  { END, (const char *)0, (enum cgen_hw_type)0, (enum cgen_mode)0, (enum cgen_operand_type)0, 0, 0 }
};

static const CGEN_OPINST sfmt_rac_ops[] ATTRIBUTE_UNUSED = {
  { INPUT, "accum", HW_H_ACCUM, CGEN_MODE_DI, 0, 0, 0 },
  { OUTPUT, "accum", HW_H_ACCUM, CGEN_MODE_DI, 0, 0, 0 },
  { END, (const char *)0, (enum cgen_hw_type)0, (enum cgen_mode)0, (enum cgen_operand_type)0, 0, 0 }
};

static const CGEN_OPINST sfmt_rac_dsi_ops[] ATTRIBUTE_UNUSED = {
  { INPUT, "accs", HW_H_ACCUMS, CGEN_MODE_DI, OP_ENT (ACCS), 0, 0 },
  { INPUT, "imm1", HW_H_UINT, CGEN_MODE_INT, OP_ENT (IMM1), 0, 0 },
  { OUTPUT, "accd", HW_H_ACCUMS, CGEN_MODE_DI, OP_ENT (ACCD), 0, 0 },
  { END, (const char *)0, (enum cgen_hw_type)0, (enum cgen_mode)0, (enum cgen_operand_type)0, 0, 0 }
};

static const CGEN_OPINST sfmt_rte_ops[] ATTRIBUTE_UNUSED = {
  { INPUT, "h_bbpsw_UQI", HW_H_BBPSW, CGEN_MODE_UQI, 0, 0, 0 },
  { INPUT, "h_bpsw_UQI", HW_H_BPSW, CGEN_MODE_UQI, 0, 0, 0 },
  { INPUT, "h_cr_USI_14", HW_H_CR, CGEN_MODE_USI, 0, 14, 0 },
  { INPUT, "h_cr_USI_6", HW_H_CR, CGEN_MODE_USI, 0, 6, 0 },
  { OUTPUT, "h_bpsw_UQI", HW_H_BPSW, CGEN_MODE_UQI, 0, 0, 0 },
  { OUTPUT, "h_cr_USI_6", HW_H_CR, CGEN_MODE_USI, 0, 6, 0 },
  { OUTPUT, "h_psw_UQI", HW_H_PSW, CGEN_MODE_UQI, 0, 0, 0 },
  { OUTPUT, "pc", HW_H_PC, CGEN_MODE_USI, 0, 0, 0 },
  { END, (const char *)0, (enum cgen_hw_type)0, (enum cgen_mode)0, (enum cgen_operand_type)0, 0, 0 }
};

static const CGEN_OPINST sfmt_seth_ops[] ATTRIBUTE_UNUSED = {
  { INPUT, "hi16", HW_H_HI16, CGEN_MODE_SI, OP_ENT (HI16), 0, 0 },
  { OUTPUT, "dr", HW_H_GR, CGEN_MODE_SI, OP_ENT (DR), 0, 0 },
  { END, (const char *)0, (enum cgen_hw_type)0, (enum cgen_mode)0, (enum cgen_operand_type)0, 0, 0 }
};

static const CGEN_OPINST sfmt_sll3_ops[] ATTRIBUTE_UNUSED = {
  { INPUT, "simm16", HW_H_SINT, CGEN_MODE_SI, OP_ENT (SIMM16), 0, 0 },
  { INPUT, "sr", HW_H_GR, CGEN_MODE_SI, OP_ENT (SR), 0, 0 },
  { OUTPUT, "dr", HW_H_GR, CGEN_MODE_SI, OP_ENT (DR), 0, 0 },
  { END, (const char *)0, (enum cgen_hw_type)0, (enum cgen_mode)0, (enum cgen_operand_type)0, 0, 0 }
};

static const CGEN_OPINST sfmt_slli_ops[] ATTRIBUTE_UNUSED = {
  { INPUT, "dr", HW_H_GR, CGEN_MODE_SI, OP_ENT (DR), 0, 0 },
  { INPUT, "uimm5", HW_H_UINT, CGEN_MODE_INT, OP_ENT (UIMM5), 0, 0 },
  { OUTPUT, "dr", HW_H_GR, CGEN_MODE_SI, OP_ENT (DR), 0, 0 },
  { END, (const char *)0, (enum cgen_hw_type)0, (enum cgen_mode)0, (enum cgen_operand_type)0, 0, 0 }
};

static const CGEN_OPINST sfmt_st_ops[] ATTRIBUTE_UNUSED = {
  { INPUT, "src1", HW_H_GR, CGEN_MODE_SI, OP_ENT (SRC1), 0, 0 },
  { INPUT, "src2", HW_H_GR, CGEN_MODE_USI, OP_ENT (SRC2), 0, 0 },
  { OUTPUT, "h_memory_SI_src2", HW_H_MEMORY, CGEN_MODE_SI, 0, 0, 0 },
  { END, (const char *)0, (enum cgen_hw_type)0, (enum cgen_mode)0, (enum cgen_operand_type)0, 0, 0 }
};

static const CGEN_OPINST sfmt_st_d_ops[] ATTRIBUTE_UNUSED = {
  { INPUT, "slo16", HW_H_SLO16, CGEN_MODE_INT, OP_ENT (SLO16), 0, 0 },
  { INPUT, "src1", HW_H_GR, CGEN_MODE_SI, OP_ENT (SRC1), 0, 0 },
  { INPUT, "src2", HW_H_GR, CGEN_MODE_SI, OP_ENT (SRC2), 0, 0 },
  { OUTPUT, "h_memory_SI_add__DFLT_src2_slo16", HW_H_MEMORY, CGEN_MODE_SI, 0, 0, 0 },
  { END, (const char *)0, (enum cgen_hw_type)0, (enum cgen_mode)0, (enum cgen_operand_type)0, 0, 0 }
};

static const CGEN_OPINST sfmt_stb_ops[] ATTRIBUTE_UNUSED = {
  { INPUT, "src1", HW_H_GR, CGEN_MODE_QI, OP_ENT (SRC1), 0, 0 },
  { INPUT, "src2", HW_H_GR, CGEN_MODE_USI, OP_ENT (SRC2), 0, 0 },
  { OUTPUT, "h_memory_QI_src2", HW_H_MEMORY, CGEN_MODE_QI, 0, 0, 0 },
  { END, (const char *)0, (enum cgen_hw_type)0, (enum cgen_mode)0, (enum cgen_operand_type)0, 0, 0 }
};

static const CGEN_OPINST sfmt_stb_d_ops[] ATTRIBUTE_UNUSED = {
  { INPUT, "slo16", HW_H_SLO16, CGEN_MODE_INT, OP_ENT (SLO16), 0, 0 },
  { INPUT, "src1", HW_H_GR, CGEN_MODE_QI, OP_ENT (SRC1), 0, 0 },
  { INPUT, "src2", HW_H_GR, CGEN_MODE_SI, OP_ENT (SRC2), 0, 0 },
  { OUTPUT, "h_memory_QI_add__DFLT_src2_slo16", HW_H_MEMORY, CGEN_MODE_QI, 0, 0, 0 },
  { END, (const char *)0, (enum cgen_hw_type)0, (enum cgen_mode)0, (enum cgen_operand_type)0, 0, 0 }
};

static const CGEN_OPINST sfmt_sth_ops[] ATTRIBUTE_UNUSED = {
  { INPUT, "src1", HW_H_GR, CGEN_MODE_HI, OP_ENT (SRC1), 0, 0 },
  { INPUT, "src2", HW_H_GR, CGEN_MODE_USI, OP_ENT (SRC2), 0, 0 },
  { OUTPUT, "h_memory_HI_src2", HW_H_MEMORY, CGEN_MODE_HI, 0, 0, 0 },
  { END, (const char *)0, (enum cgen_hw_type)0, (enum cgen_mode)0, (enum cgen_operand_type)0, 0, 0 }
};

static const CGEN_OPINST sfmt_sth_d_ops[] ATTRIBUTE_UNUSED = {
  { INPUT, "slo16", HW_H_SLO16, CGEN_MODE_INT, OP_ENT (SLO16), 0, 0 },
  { INPUT, "src1", HW_H_GR, CGEN_MODE_HI, OP_ENT (SRC1), 0, 0 },
  { INPUT, "src2", HW_H_GR, CGEN_MODE_SI, OP_ENT (SRC2), 0, 0 },
  { OUTPUT, "h_memory_HI_add__DFLT_src2_slo16", HW_H_MEMORY, CGEN_MODE_HI, 0, 0, 0 },
  { END, (const char *)0, (enum cgen_hw_type)0, (enum cgen_mode)0, (enum cgen_operand_type)0, 0, 0 }
};

static const CGEN_OPINST sfmt_st_plus_ops[] ATTRIBUTE_UNUSED = {
  { INPUT, "src1", HW_H_GR, CGEN_MODE_SI, OP_ENT (SRC1), 0, 0 },
  { INPUT, "src2", HW_H_GR, CGEN_MODE_SI, OP_ENT (SRC2), 0, 0 },
  { OUTPUT, "h_memory_SI_new_src2", HW_H_MEMORY, CGEN_MODE_SI, 0, 0, 0 },
  { OUTPUT, "src2", HW_H_GR, CGEN_MODE_SI, OP_ENT (SRC2), 0, 0 },
  { END, (const char *)0, (enum cgen_hw_type)0, (enum cgen_mode)0, (enum cgen_operand_type)0, 0, 0 }
};

static const CGEN_OPINST sfmt_sth_plus_ops[] ATTRIBUTE_UNUSED = {
  { INPUT, "src1", HW_H_GR, CGEN_MODE_SI, OP_ENT (SRC1), 0, 0 },
  { INPUT, "src2", HW_H_GR, CGEN_MODE_SI, OP_ENT (SRC2), 0, 0 },
  { OUTPUT, "h_memory_HI_new_src2", HW_H_MEMORY, CGEN_MODE_HI, 0, 0, 0 },
  { OUTPUT, "src2", HW_H_GR, CGEN_MODE_SI, OP_ENT (SRC2), 0, 0 },
  { END, (const char *)0, (enum cgen_hw_type)0, (enum cgen_mode)0, (enum cgen_operand_type)0, 0, 0 }
};

static const CGEN_OPINST sfmt_stb_plus_ops[] ATTRIBUTE_UNUSED = {
  { INPUT, "src1", HW_H_GR, CGEN_MODE_SI, OP_ENT (SRC1), 0, 0 },
  { INPUT, "src2", HW_H_GR, CGEN_MODE_SI, OP_ENT (SRC2), 0, 0 },
  { OUTPUT, "h_memory_QI_new_src2", HW_H_MEMORY, CGEN_MODE_QI, 0, 0, 0 },
  { OUTPUT, "src2", HW_H_GR, CGEN_MODE_SI, OP_ENT (SRC2), 0, 0 },
  { END, (const char *)0, (enum cgen_hw_type)0, (enum cgen_mode)0, (enum cgen_operand_type)0, 0, 0 }
};

static const CGEN_OPINST sfmt_trap_ops[] ATTRIBUTE_UNUSED = {
  { INPUT, "h_bpsw_UQI", HW_H_BPSW, CGEN_MODE_UQI, 0, 0, 0 },
  { INPUT, "h_cr_USI_6", HW_H_CR, CGEN_MODE_USI, 0, 6, 0 },
  { INPUT, "h_psw_UQI", HW_H_PSW, CGEN_MODE_UQI, 0, 0, 0 },
  { INPUT, "pc", HW_H_PC, CGEN_MODE_USI, 0, 0, 0 },
  { INPUT, "uimm4", HW_H_UINT, CGEN_MODE_UINT, OP_ENT (UIMM4), 0, 0 },
  { OUTPUT, "h_bbpsw_UQI", HW_H_BBPSW, CGEN_MODE_UQI, 0, 0, 0 },
  { OUTPUT, "h_bpsw_UQI", HW_H_BPSW, CGEN_MODE_UQI, 0, 0, 0 },
  { OUTPUT, "h_cr_USI_14", HW_H_CR, CGEN_MODE_USI, 0, 14, 0 },
  { OUTPUT, "h_cr_USI_6", HW_H_CR, CGEN_MODE_USI, 0, 6, 0 },
  { OUTPUT, "h_psw_UQI", HW_H_PSW, CGEN_MODE_UQI, 0, 0, 0 },
  { OUTPUT, "pc", HW_H_PC, CGEN_MODE_SI, 0, 0, 0 },
  { END, (const char *)0, (enum cgen_hw_type)0, (enum cgen_mode)0, (enum cgen_operand_type)0, 0, 0 }
};

static const CGEN_OPINST sfmt_unlock_ops[] ATTRIBUTE_UNUSED = {
  { INPUT, "h_lock_BI", HW_H_LOCK, CGEN_MODE_BI, 0, 0, 0 },
  { INPUT, "src1", HW_H_GR, CGEN_MODE_SI, OP_ENT (SRC1), 0, COND_REF },
  { INPUT, "src2", HW_H_GR, CGEN_MODE_USI, OP_ENT (SRC2), 0, COND_REF },
  { OUTPUT, "h_lock_BI", HW_H_LOCK, CGEN_MODE_BI, 0, 0, 0 },
  { OUTPUT, "h_memory_SI_src2", HW_H_MEMORY, CGEN_MODE_SI, 0, 0, COND_REF },
  { END, (const char *)0, (enum cgen_hw_type)0, (enum cgen_mode)0, (enum cgen_operand_type)0, 0, 0 }
};

static const CGEN_OPINST sfmt_satb_ops[] ATTRIBUTE_UNUSED = {
  { INPUT, "sr", HW_H_GR, CGEN_MODE_SI, OP_ENT (SR), 0, 0 },
  { OUTPUT, "dr", HW_H_GR, CGEN_MODE_SI, OP_ENT (DR), 0, 0 },
  { END, (const char *)0, (enum cgen_hw_type)0, (enum cgen_mode)0, (enum cgen_operand_type)0, 0, 0 }
};

static const CGEN_OPINST sfmt_sat_ops[] ATTRIBUTE_UNUSED = {
  { INPUT, "condbit", HW_H_COND, CGEN_MODE_BI, 0, 0, 0 },
  { INPUT, "sr", HW_H_GR, CGEN_MODE_SI, OP_ENT (SR), 0, COND_REF },
  { OUTPUT, "dr", HW_H_GR, CGEN_MODE_SI, OP_ENT (DR), 0, 0 },
  { END, (const char *)0, (enum cgen_hw_type)0, (enum cgen_mode)0, (enum cgen_operand_type)0, 0, 0 }
};

static const CGEN_OPINST sfmt_sadd_ops[] ATTRIBUTE_UNUSED = {
  { INPUT, "h_accums_DI_0", HW_H_ACCUMS, CGEN_MODE_DI, 0, 0, 0 },
  { INPUT, "h_accums_DI_1", HW_H_ACCUMS, CGEN_MODE_DI, 0, 1, 0 },
  { OUTPUT, "h_accums_DI_0", HW_H_ACCUMS, CGEN_MODE_DI, 0, 0, 0 },
  { END, (const char *)0, (enum cgen_hw_type)0, (enum cgen_mode)0, (enum cgen_operand_type)0, 0, 0 }
};

static const CGEN_OPINST sfmt_macwu1_ops[] ATTRIBUTE_UNUSED = {
  { INPUT, "h_accums_DI_1", HW_H_ACCUMS, CGEN_MODE_DI, 0, 1, 0 },
  { INPUT, "src1", HW_H_GR, CGEN_MODE_SI, OP_ENT (SRC1), 0, 0 },
  { INPUT, "src2", HW_H_GR, CGEN_MODE_SI, OP_ENT (SRC2), 0, 0 },
  { OUTPUT, "h_accums_DI_1", HW_H_ACCUMS, CGEN_MODE_DI, 0, 1, 0 },
  { END, (const char *)0, (enum cgen_hw_type)0, (enum cgen_mode)0, (enum cgen_operand_type)0, 0, 0 }
};

static const CGEN_OPINST sfmt_mulwu1_ops[] ATTRIBUTE_UNUSED = {
  { INPUT, "src1", HW_H_GR, CGEN_MODE_SI, OP_ENT (SRC1), 0, 0 },
  { INPUT, "src2", HW_H_GR, CGEN_MODE_SI, OP_ENT (SRC2), 0, 0 },
  { OUTPUT, "h_accums_DI_1", HW_H_ACCUMS, CGEN_MODE_DI, 0, 1, 0 },
  { END, (const char *)0, (enum cgen_hw_type)0, (enum cgen_mode)0, (enum cgen_operand_type)0, 0, 0 }
};

static const CGEN_OPINST sfmt_sc_ops[] ATTRIBUTE_UNUSED = {
  { INPUT, "condbit", HW_H_COND, CGEN_MODE_BI, 0, 0, 0 },
  { END, (const char *)0, (enum cgen_hw_type)0, (enum cgen_mode)0, (enum cgen_operand_type)0, 0, 0 }
};

static const CGEN_OPINST sfmt_clrpsw_ops[] ATTRIBUTE_UNUSED = {
  { INPUT, "h_cr_USI_0", HW_H_CR, CGEN_MODE_USI, 0, 0, 0 },
  { INPUT, "uimm8", HW_H_UINT, CGEN_MODE_BI, OP_ENT (UIMM8), 0, 0 },
  { OUTPUT, "h_cr_USI_0", HW_H_CR, CGEN_MODE_USI, 0, 0, 0 },
  { END, (const char *)0, (enum cgen_hw_type)0, (enum cgen_mode)0, (enum cgen_operand_type)0, 0, 0 }
};

static const CGEN_OPINST sfmt_setpsw_ops[] ATTRIBUTE_UNUSED = {
  { INPUT, "uimm8", HW_H_UINT, CGEN_MODE_USI, OP_ENT (UIMM8), 0, 0 },
  { OUTPUT, "h_cr_USI_0", HW_H_CR, CGEN_MODE_USI, 0, 0, 0 },
  { END, (const char *)0, (enum cgen_hw_type)0, (enum cgen_mode)0, (enum cgen_operand_type)0, 0, 0 }
};

static const CGEN_OPINST sfmt_bset_ops[] ATTRIBUTE_UNUSED = {
  { INPUT, "h_memory_QI_add__DFLT_sr_slo16", HW_H_MEMORY, CGEN_MODE_QI, 0, 0, 0 },
  { INPUT, "slo16", HW_H_SLO16, CGEN_MODE_INT, OP_ENT (SLO16), 0, 0 },
  { INPUT, "sr", HW_H_GR, CGEN_MODE_SI, OP_ENT (SR), 0, 0 },
  { INPUT, "uimm3", HW_H_UINT, CGEN_MODE_UINT, OP_ENT (UIMM3), 0, 0 },
  { OUTPUT, "h_memory_QI_add__DFLT_sr_slo16", HW_H_MEMORY, CGEN_MODE_QI, 0, 0, 0 },
  { END, (const char *)0, (enum cgen_hw_type)0, (enum cgen_mode)0, (enum cgen_operand_type)0, 0, 0 }
};

static const CGEN_OPINST sfmt_btst_ops[] ATTRIBUTE_UNUSED = {
  { INPUT, "sr", HW_H_GR, CGEN_MODE_USI, OP_ENT (SR), 0, 0 },
  { INPUT, "uimm3", HW_H_UINT, CGEN_MODE_UINT, OP_ENT (UIMM3), 0, 0 },
  { OUTPUT, "condbit", HW_H_COND, CGEN_MODE_BI, 0, 0, 0 },
  { END, (const char *)0, (enum cgen_hw_type)0, (enum cgen_mode)0, (enum cgen_operand_type)0, 0, 0 }
};

#undef OP_ENT
#undef INPUT
#undef OUTPUT
#undef END
#undef COND_REF

/* Operand instance lookup table.  */

static const CGEN_OPINST *m32r_cgen_opinst_table[MAX_INSNS] = {
  0,
  & sfmt_add_ops[0],
  & sfmt_add3_ops[0],
  & sfmt_add_ops[0],
  & sfmt_and3_ops[0],
  & sfmt_add_ops[0],
  & sfmt_or3_ops[0],
  & sfmt_add_ops[0],
  & sfmt_and3_ops[0],
  & sfmt_addi_ops[0],
  & sfmt_addv_ops[0],
  & sfmt_addv3_ops[0],
  & sfmt_addx_ops[0],
  & sfmt_bc8_ops[0],
  & sfmt_bc24_ops[0],
  & sfmt_beq_ops[0],
  & sfmt_beqz_ops[0],
  & sfmt_beqz_ops[0],
  & sfmt_beqz_ops[0],
  & sfmt_beqz_ops[0],
  & sfmt_beqz_ops[0],
  & sfmt_beqz_ops[0],
  & sfmt_bl8_ops[0],
  & sfmt_bl24_ops[0],
  & sfmt_bcl8_ops[0],
  & sfmt_bcl24_ops[0],
  & sfmt_bc8_ops[0],
  & sfmt_bc24_ops[0],
  & sfmt_beq_ops[0],
  & sfmt_bra8_ops[0],
  & sfmt_bra24_ops[0],
  & sfmt_bcl8_ops[0],
  & sfmt_bcl24_ops[0],
  & sfmt_cmp_ops[0],
  & sfmt_cmpi_ops[0],
  & sfmt_cmp_ops[0],
  & sfmt_cmpi_ops[0],
  & sfmt_cmp_ops[0],
  & sfmt_cmpz_ops[0],
  & sfmt_div_ops[0],
  & sfmt_div_ops[0],
  & sfmt_div_ops[0],
  & sfmt_div_ops[0],
  & sfmt_div_ops[0],
  & sfmt_div_ops[0],
  & sfmt_div_ops[0],
  & sfmt_div_ops[0],
  & sfmt_div_ops[0],
  & sfmt_div_ops[0],
  & sfmt_div_ops[0],
  & sfmt_div_ops[0],
  & sfmt_jc_ops[0],
  & sfmt_jc_ops[0],
  & sfmt_jl_ops[0],
  & sfmt_jmp_ops[0],
  & sfmt_ld_ops[0],
  & sfmt_ld_d_ops[0],
  & sfmt_ldb_ops[0],
  & sfmt_ldb_d_ops[0],
  & sfmt_ldh_ops[0],
  & sfmt_ldh_d_ops[0],
  & sfmt_ldb_ops[0],
  & sfmt_ldb_d_ops[0],
  & sfmt_ldh_ops[0],
  & sfmt_ldh_d_ops[0],
  & sfmt_ld_plus_ops[0],
  & sfmt_ld24_ops[0],
  & sfmt_ldi8_ops[0],
  & sfmt_ldi16_ops[0],
  & sfmt_lock_ops[0],
  & sfmt_machi_ops[0],
  & sfmt_machi_a_ops[0],
  & sfmt_machi_ops[0],
  & sfmt_machi_a_ops[0],
  & sfmt_machi_ops[0],
  & sfmt_machi_a_ops[0],
  & sfmt_machi_ops[0],
  & sfmt_machi_a_ops[0],
  & sfmt_add_ops[0],
  & sfmt_mulhi_ops[0],
  & sfmt_mulhi_a_ops[0],
  & sfmt_mulhi_ops[0],
  & sfmt_mulhi_a_ops[0],
  & sfmt_mulhi_ops[0],
  & sfmt_mulhi_a_ops[0],
  & sfmt_mulhi_ops[0],
  & sfmt_mulhi_a_ops[0],
  & sfmt_mv_ops[0],
  & sfmt_mvfachi_ops[0],
  & sfmt_mvfachi_a_ops[0],
  & sfmt_mvfachi_ops[0],
  & sfmt_mvfachi_a_ops[0],
  & sfmt_mvfachi_ops[0],
  & sfmt_mvfachi_a_ops[0],
  & sfmt_mvfc_ops[0],
  & sfmt_mvtachi_ops[0],
  & sfmt_mvtachi_a_ops[0],
  & sfmt_mvtachi_ops[0],
  & sfmt_mvtachi_a_ops[0],
  & sfmt_mvtc_ops[0],
  & sfmt_mv_ops[0],
  & sfmt_nop_ops[0],
  & sfmt_mv_ops[0],
  & sfmt_rac_ops[0],
  & sfmt_rac_dsi_ops[0],
  & sfmt_rac_ops[0],
  & sfmt_rac_dsi_ops[0],
  & sfmt_rte_ops[0],
  & sfmt_seth_ops[0],
  & sfmt_add_ops[0],
  & sfmt_sll3_ops[0],
  & sfmt_slli_ops[0],
  & sfmt_add_ops[0],
  & sfmt_sll3_ops[0],
  & sfmt_slli_ops[0],
  & sfmt_add_ops[0],
  & sfmt_sll3_ops[0],
  & sfmt_slli_ops[0],
  & sfmt_st_ops[0],
  & sfmt_st_d_ops[0],
  & sfmt_stb_ops[0],
  & sfmt_stb_d_ops[0],
  & sfmt_sth_ops[0],
  & sfmt_sth_d_ops[0],
  & sfmt_st_plus_ops[0],
  & sfmt_sth_plus_ops[0],
  & sfmt_stb_plus_ops[0],
  & sfmt_st_plus_ops[0],
  & sfmt_add_ops[0],
  & sfmt_addv_ops[0],
  & sfmt_addx_ops[0],
  & sfmt_trap_ops[0],
  & sfmt_unlock_ops[0],
  & sfmt_satb_ops[0],
  & sfmt_satb_ops[0],
  & sfmt_sat_ops[0],
  & sfmt_cmpz_ops[0],
  & sfmt_sadd_ops[0],
  & sfmt_macwu1_ops[0],
  & sfmt_machi_ops[0],
  & sfmt_mulwu1_ops[0],
  & sfmt_macwu1_ops[0],
  & sfmt_sc_ops[0],
  & sfmt_sc_ops[0],
  & sfmt_clrpsw_ops[0],
  & sfmt_setpsw_ops[0],
  & sfmt_bset_ops[0],
  & sfmt_bset_ops[0],
  & sfmt_btst_ops[0],
};

/* Function to call before using the operand instance table.  */

void
m32r_cgen_init_opinst_table (cd)
     CGEN_CPU_DESC cd;
{
  int i;
  const CGEN_OPINST **oi = & m32r_cgen_opinst_table[0];
  CGEN_INSN *insns = (CGEN_INSN *) cd->insn_table.init_entries;
  for (i = 0; i < MAX_INSNS; ++i)
    insns[i].opinst = oi[i];
}
