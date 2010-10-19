/* bfin.h -- Header file for ADI Blackfin opcode table
   Copyright 2005 Free Software Foundation, Inc.

This file is part of GDB, GAS, and the GNU binutils.

GDB, GAS, and the GNU binutils are free software; you can redistribute
them and/or modify them under the terms of the GNU General Public
License as published by the Free Software Foundation; either version
1, or (at your option) any later version.

GDB, GAS, and the GNU binutils are distributed in the hope that they
will be useful, but WITHOUT ANY WARRANTY; without even the implied
warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See
the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this file; see the file COPYING.  If not, write to the Free
Software Foundation, 51 Franklin Street - Fifth Floor, Boston, MA 02110-1301, USA.  */

/* Common to all DSP32 instructions.  */
#define BIT_MULTI_INS 0x0800

/* This just sets the multi instruction bit of a DSP32 instruction.  */
#define SET_MULTI_INSTRUCTION_BIT(x) x->value |=  BIT_MULTI_INS;


/* DSP instructions (32 bit) */

/*   dsp32mac
+----+----+---+---|---+----+----+---|---+---+---+---|---+---+---+---+
| 1  | 1  | 0 | 0 |.M.| 0  | 0  |.mmod..........|.MM|.P.|.w1|.op1...|
|.h01|.h11|.w0|.op0...|.h00|.h10|.dst.......|.src0......|.src1......|
+----+----+---+---|---+----+----+---|---+---+---+---|---+---+---+---+
*/

typedef struct
{
  unsigned long opcode;
  int bits_src1;
  int mask_src1;
  int bits_src0;      
  int mask_src0;
  int bits_dst;      
  int mask_dst;
  int bits_h10;      
  int mask_h10;
  int bits_h00;      
  int mask_h00;
  int bits_op0;      
  int mask_op0;
  int bits_w0;      
  int mask_w0;
  int bits_h11;      
  int mask_h11;
  int bits_h01;      
  int mask_h01;
  int bits_op1;      
  int mask_op1;
  int bits_w1;      
  int mask_w1;
  int bits_P;      
  int mask_P;
  int bits_MM;      
  int mask_MM;
  int bits_mmod;      
  int mask_mmod;
  int bits_code2;      
  int mask_code2;
  int bits_M;      
  int mask_M;
  int bits_code;      
  int mask_code;
} DSP32Mac;

#define DSP32Mac_opcode			0xc0000000
#define DSP32Mac_src1_bits		0
#define DSP32Mac_src1_mask		0x7
#define DSP32Mac_src0_bits		3
#define DSP32Mac_src0_mask		0x7
#define DSP32Mac_dst_bits		6
#define DSP32Mac_dst_mask		0x7
#define DSP32Mac_h10_bits		9
#define DSP32Mac_h10_mask		0x1
#define DSP32Mac_h00_bits		10
#define DSP32Mac_h00_mask		0x1
#define DSP32Mac_op0_bits		11
#define DSP32Mac_op0_mask		0x3
#define DSP32Mac_w0_bits		13
#define DSP32Mac_w0_mask		0x1
#define DSP32Mac_h11_bits		14
#define DSP32Mac_h11_mask		0x1
#define DSP32Mac_h01_bits		15
#define DSP32Mac_h01_mask		0x1
#define DSP32Mac_op1_bits		16
#define DSP32Mac_op1_mask		0x3
#define DSP32Mac_w1_bits		18
#define DSP32Mac_w1_mask		0x1
#define DSP32Mac_p_bits			19
#define DSP32Mac_p_mask			0x1
#define DSP32Mac_MM_bits		20	
#define DSP32Mac_MM_mask		0x1
#define DSP32Mac_mmod_bits		21
#define DSP32Mac_mmod_mask		0xf
#define DSP32Mac_code2_bits		25
#define DSP32Mac_code2_mask		0x3
#define DSP32Mac_M_bits			27
#define DSP32Mac_M_mask			0x1
#define DSP32Mac_code_bits		28
#define DSP32Mac_code_mask		0xf

#define init_DSP32Mac				\
{						\
  DSP32Mac_opcode,				\
  DSP32Mac_src1_bits,	DSP32Mac_src1_mask,	\
  DSP32Mac_src0_bits,	DSP32Mac_src0_mask,	\
  DSP32Mac_dst_bits,	DSP32Mac_dst_mask,	\
  DSP32Mac_h10_bits,	DSP32Mac_h10_mask,	\
  DSP32Mac_h00_bits,	DSP32Mac_h00_mask,	\
  DSP32Mac_op0_bits,	DSP32Mac_op0_mask,	\
  DSP32Mac_w0_bits,	DSP32Mac_w0_mask,	\
  DSP32Mac_h11_bits,	DSP32Mac_h11_mask,	\
  DSP32Mac_h01_bits,	DSP32Mac_h01_mask,	\
  DSP32Mac_op1_bits,	DSP32Mac_op1_mask,	\
  DSP32Mac_w1_bits,	DSP32Mac_w1_mask,	\
  DSP32Mac_p_bits,	DSP32Mac_p_mask,	\
  DSP32Mac_MM_bits,	DSP32Mac_MM_mask,	\
  DSP32Mac_mmod_bits,	DSP32Mac_mmod_mask,	\
  DSP32Mac_code2_bits,	DSP32Mac_code2_mask,	\
  DSP32Mac_M_bits,	DSP32Mac_M_mask,	\
  DSP32Mac_code_bits,	DSP32Mac_code_mask	\
};

/*  dsp32mult
+----+----+---+---|---+----+----+---|---+---+---+---|---+---+---+---+
| 1  | 1  | 0 | 0 |.M.| 0  | 1  |.mmod..........|.MM|.P.|.w1|.op1...|
|.h01|.h11|.w0|.op0...|.h00|.h10|.dst.......|.src0......|.src1......|
+----+----+---+---|---+----+----+---|---+---+---+---|---+---+---+---+
*/

typedef DSP32Mac DSP32Mult;
#define DSP32Mult_opcode 	0xc2000000

#define init_DSP32Mult				\
{						\
  DSP32Mult_opcode,				\
  DSP32Mac_src1_bits,	DSP32Mac_src1_mask,	\
  DSP32Mac_src0_bits,	DSP32Mac_src0_mask,	\
  DSP32Mac_dst_bits,	DSP32Mac_dst_mask,	\
  DSP32Mac_h10_bits,	DSP32Mac_h10_mask,	\
  DSP32Mac_h00_bits,	DSP32Mac_h00_mask,	\
  DSP32Mac_op0_bits,	DSP32Mac_op0_mask,	\
  DSP32Mac_w0_bits,	DSP32Mac_w0_mask,	\
  DSP32Mac_h11_bits,	DSP32Mac_h11_mask,	\
  DSP32Mac_h01_bits,	DSP32Mac_h01_mask,	\
  DSP32Mac_op1_bits,	DSP32Mac_op1_mask,	\
  DSP32Mac_w1_bits,	DSP32Mac_w1_mask,	\
  DSP32Mac_p_bits,	DSP32Mac_p_mask,	\
  DSP32Mac_MM_bits,	DSP32Mac_MM_mask,	\
  DSP32Mac_mmod_bits,	DSP32Mac_mmod_mask,	\
  DSP32Mac_code2_bits,	DSP32Mac_code2_mask,	\
  DSP32Mac_M_bits,	DSP32Mac_M_mask,	\
  DSP32Mac_code_bits,	DSP32Mac_code_mask	\
};

/*  dsp32alu
+---+---+---+---|---+---+---+---|---+---+---+---|---+---+---+---+
| 1 | 1 | 0 | 0 |.M.| 1 | 0 | - | - | - |.HL|.aopcde............|
|.aop...|.s.|.x.|.dst0......|.dst1......|.src0......|.src1......|
+---+---+---+---|---+---+---+---|---+---+---+---|---+---+---+---+
*/

typedef struct
{
  unsigned long opcode;
  int bits_src1;
  int mask_src1;
  int bits_src0;
  int mask_src0;
  int bits_dst1;
  int mask_dst1;
  int bits_dst0;
  int mask_dst0;
  int bits_x;
  int mask_x;
  int bits_s;
  int mask_s;
  int bits_aop;
  int mask_aop;
  int bits_aopcde;
  int mask_aopcde;
  int bits_HL;
  int mask_HL;
  int bits_dontcare;
  int mask_dontcare;
  int bits_code2;
  int mask_code2;
  int bits_M;
  int mask_M;
  int bits_code;
  int mask_code;
} DSP32Alu;

#define DSP32Alu_opcode		0xc4000000
#define DSP32Alu_src1_bits	0
#define DSP32Alu_src1_mask	0x7
#define DSP32Alu_src0_bits	3	
#define DSP32Alu_src0_mask	0x7
#define DSP32Alu_dst1_bits	6
#define DSP32Alu_dst1_mask	0x7
#define DSP32Alu_dst0_bits	9	
#define DSP32Alu_dst0_mask	0x7
#define DSP32Alu_x_bits		12
#define DSP32Alu_x_mask		0x1
#define DSP32Alu_s_bits		13
#define DSP32Alu_s_mask		0x1
#define DSP32Alu_aop_bits	14
#define DSP32Alu_aop_mask	0x3
#define DSP32Alu_aopcde_bits	16
#define DSP32Alu_aopcde_mask	0x1f
#define DSP32Alu_HL_bits	21
#define DSP32Alu_HL_mask	0x1
#define DSP32Alu_dontcare_bits	22
#define DSP32Alu_dontcare_mask	0x7
#define DSP32Alu_code2_bits	25
#define DSP32Alu_code2_mask	0x3
#define DSP32Alu_M_bits		27
#define DSP32Alu_M_mask		0x1
#define DSP32Alu_code_bits	28
#define DSP32Alu_code_mask	0xf

#define init_DSP32Alu 					\
{							\
  DSP32Alu_opcode,					\
  DSP32Alu_src1_bits,		DSP32Alu_src1_mask,	\
  DSP32Alu_src0_bits,		DSP32Alu_src0_mask,	\
  DSP32Alu_dst1_bits,		DSP32Alu_dst1_mask,	\
  DSP32Alu_dst0_bits,		DSP32Alu_dst0_mask,	\
  DSP32Alu_x_bits,		DSP32Alu_x_mask,	\
  DSP32Alu_s_bits,		DSP32Alu_s_mask,	\
  DSP32Alu_aop_bits,		DSP32Alu_aop_mask,	\
  DSP32Alu_aopcde_bits,		DSP32Alu_aopcde_mask,	\
  DSP32Alu_HL_bits,		DSP32Alu_HL_mask,	\
  DSP32Alu_dontcare_bits,	DSP32Alu_dontcare_mask,	\
  DSP32Alu_code2_bits,		DSP32Alu_code2_mask,	\
  DSP32Alu_M_bits,		DSP32Alu_M_mask,	\
  DSP32Alu_code_bits,		DSP32Alu_code_mask 	\
};

/*  dsp32shift
+---+---+---+---|---+---+---+---|---+---+---+---|---+---+---+---+
| 1 | 1 | 0 | 0 |.M.| 1 | 1 | 0 | 0 | - | - |.sopcde............|
|.sop...|.HLs...|.dst0......| - | - | - |.src0......|.src1......|
+---+---+---+---|---+---+---+---|---+---+---+---|---+---+---+---+
*/

typedef struct
{
  unsigned long opcode;
  int bits_src1;
  int mask_src1;
  int bits_src0;
  int mask_src0;
  int bits_dst1;
  int mask_dst1;
  int bits_dst0;
  int mask_dst0;
  int bits_HLs;
  int mask_HLs;
  int bits_sop;
  int mask_sop;
  int bits_sopcde;
  int mask_sopcde;
  int bits_dontcare;
  int mask_dontcare;
  int bits_code2;
  int mask_code2;
  int bits_M;
  int mask_M;
  int bits_code;
  int mask_code;
} DSP32Shift;

#define DSP32Shift_opcode		0xc6000000
#define DSP32Shift_src1_bits		0
#define DSP32Shift_src1_mask		0x7
#define DSP32Shift_src0_bits		3
#define DSP32Shift_src0_mask		0x7
#define DSP32Shift_dst1_bits		6
#define DSP32Shift_dst1_mask		0x7
#define DSP32Shift_dst0_bits		9
#define DSP32Shift_dst0_mask		0x7
#define DSP32Shift_HLs_bits		12
#define DSP32Shift_HLs_mask		0x3
#define DSP32Shift_sop_bits		14
#define DSP32Shift_sop_mask		0x3
#define DSP32Shift_sopcde_bits		16
#define DSP32Shift_sopcde_mask		0x1f
#define DSP32Shift_dontcare_bits	21
#define DSP32Shift_dontcare_mask	0x3
#define DSP32Shift_code2_bits		23
#define DSP32Shift_code2_mask		0xf
#define DSP32Shift_M_bits		27
#define DSP32Shift_M_mask		0x1
#define DSP32Shift_code_bits		28
#define DSP32Shift_code_mask		0xf

#define init_DSP32Shift						\
{								\
  DSP32Shift_opcode,						\
  DSP32Shift_src1_bits,		DSP32Shift_src1_mask,		\
  DSP32Shift_src0_bits,		DSP32Shift_src0_mask,		\
  DSP32Shift_dst1_bits,		DSP32Shift_dst1_mask,		\
  DSP32Shift_dst0_bits,		DSP32Shift_dst0_mask,		\
  DSP32Shift_HLs_bits,		DSP32Shift_HLs_mask,		\
  DSP32Shift_sop_bits,		DSP32Shift_sop_mask,		\
  DSP32Shift_sopcde_bits,	DSP32Shift_sopcde_mask,		\
  DSP32Shift_dontcare_bits,	DSP32Shift_dontcare_mask,	\
  DSP32Shift_code2_bits,	DSP32Shift_code2_mask,		\
  DSP32Shift_M_bits,		DSP32Shift_M_mask,		\
  DSP32Shift_code_bits,		DSP32Shift_code_mask		\
};

/*  dsp32shiftimm
+---+---+---+---|---+---+---+---|---+---+---+---|---+---+---+---+
| 1 | 1 | 0 | 0 |.M.| 1 | 1 | 0 | 1 | - | - |.sopcde............|
|.sop...|.HLs...|.dst0......|.immag.................|.src1......|
+---+---+---+---|---+---+---+---|---+---+---+---|---+---+---+---+
*/

typedef struct
{
  unsigned long opcode;
  int bits_src1;
  int mask_src1;
  int bits_immag;
  int mask_immag;
  int bits_dst0;
  int mask_dst0;
  int bits_HLs;
  int mask_HLs;
  int bits_sop;
  int mask_sop;
  int bits_sopcde;
  int mask_sopcde;
  int bits_dontcare;
  int mask_dontcare;
  int bits_code2;
  int mask_code2;
  int bits_M;
  int mask_M;
  int bits_code;
  int mask_code;
} DSP32ShiftImm;

#define DSP32ShiftImm_opcode		0xc6800000
#define DSP32ShiftImm_src1_bits		0
#define DSP32ShiftImm_src1_mask		0x7
#define DSP32ShiftImm_immag_bits	3
#define DSP32ShiftImm_immag_mask	0x3f
#define DSP32ShiftImm_dst0_bits		9
#define DSP32ShiftImm_dst0_mask		0x7
#define DSP32ShiftImm_HLs_bits		12
#define DSP32ShiftImm_HLs_mask		0x3
#define DSP32ShiftImm_sop_bits		14
#define DSP32ShiftImm_sop_mask		0x3
#define DSP32ShiftImm_sopcde_bits	16
#define DSP32ShiftImm_sopcde_mask	0x1f
#define DSP32ShiftImm_dontcare_bits	21
#define DSP32ShiftImm_dontcare_mask	0x3
#define DSP32ShiftImm_code2_bits	23
#define DSP32ShiftImm_code2_mask	0xf
#define DSP32ShiftImm_M_bits		27
#define DSP32ShiftImm_M_mask		0x1
#define DSP32ShiftImm_code_bits		28	
#define DSP32ShiftImm_code_mask		0xf

#define init_DSP32ShiftImm					\
{								\
  DSP32ShiftImm_opcode,						\
  DSP32ShiftImm_src1_bits,	DSP32ShiftImm_src1_mask,	\
  DSP32ShiftImm_immag_bits,	DSP32ShiftImm_immag_mask,	\
  DSP32ShiftImm_dst0_bits,	DSP32ShiftImm_dst0_mask,	\
  DSP32ShiftImm_HLs_bits,	DSP32ShiftImm_HLs_mask,		\
  DSP32ShiftImm_sop_bits,	DSP32ShiftImm_sop_mask,		\
  DSP32ShiftImm_sopcde_bits,	DSP32ShiftImm_sopcde_mask,	\
  DSP32ShiftImm_dontcare_bits,	DSP32ShiftImm_dontcare_mask,	\
  DSP32ShiftImm_code2_bits,	DSP32ShiftImm_code2_mask,	\
  DSP32ShiftImm_M_bits,		DSP32ShiftImm_M_mask,		\
  DSP32ShiftImm_code_bits,	DSP32ShiftImm_code_mask		\
};

/* LOAD / STORE  */

/*  LDSTidxI
+---+---+---+---|---+---+---+---|---+---+---+---|---+---+---+---+
| 1 | 1 | 1 | 0 | 0 | 1 |.W.|.Z.|.sz....|.ptr.......|.reg.......|
|.offset........................................................|
+---+---+---+---|---+---+---+---|---+---+---+---|---+---+---+---+
*/

typedef struct
{
  unsigned long opcode;
  int bits_offset;
  int mask_offset;
  int bits_reg;
  int mask_reg;
  int bits_ptr;
  int mask_ptr;
  int bits_sz;
  int mask_sz;
  int bits_Z;
  int mask_Z;
  int bits_W;
  int mask_W;
  int bits_code;
  int mask_code;
} LDSTidxI;

#define LDSTidxI_opcode		0xe4000000
#define LDSTidxI_offset_bits	0
#define LDSTidxI_offset_mask	0xffff
#define LDSTidxI_reg_bits	16
#define LDSTidxI_reg_mask	0x7
#define LDSTidxI_ptr_bits	19
#define LDSTidxI_ptr_mask	0x7
#define LDSTidxI_sz_bits	22
#define LDSTidxI_sz_mask	0x3
#define LDSTidxI_Z_bits		24
#define LDSTidxI_Z_mask		0x1
#define LDSTidxI_W_bits		25
#define LDSTidxI_W_mask		0x1
#define LDSTidxI_code_bits	26
#define LDSTidxI_code_mask	0x3f

#define init_LDSTidxI				\
{						\
  LDSTidxI_opcode,				\
  LDSTidxI_offset_bits, LDSTidxI_offset_mask,	\
  LDSTidxI_reg_bits, LDSTidxI_reg_mask,		\
  LDSTidxI_ptr_bits, LDSTidxI_ptr_mask,		\
  LDSTidxI_sz_bits, LDSTidxI_sz_mask,		\
  LDSTidxI_Z_bits, LDSTidxI_Z_mask,		\
  LDSTidxI_W_bits, LDSTidxI_W_mask,		\
  LDSTidxI_code_bits, LDSTidxI_code_mask	\
};


/*  LDST
+---+---+---+---|---+---+---+---|---+---+---+---|---+---+---+---+
| 1 | 0 | 0 | 1 |.sz....|.W.|.aop...|.Z.|.ptr.......|.reg.......|
+---+---+---+---|---+---+---+---|---+---+---+---|---+---+---+---+
*/

typedef struct
{
  unsigned short opcode;
  int bits_reg;
  int mask_reg;
  int bits_ptr;
  int mask_ptr;
  int bits_Z;
  int mask_Z;
  int bits_aop;
  int mask_aop;
  int bits_W;
  int mask_W;
  int bits_sz;
  int mask_sz;
  int bits_code;
  int mask_code;
} LDST;

#define LDST_opcode		0x9000
#define LDST_reg_bits		0
#define LDST_reg_mask		0x7
#define LDST_ptr_bits		3
#define LDST_ptr_mask		0x7
#define LDST_Z_bits		6
#define LDST_Z_mask		0x1
#define LDST_aop_bits		7
#define LDST_aop_mask		0x3
#define LDST_W_bits		9
#define LDST_W_mask		0x1
#define LDST_sz_bits		10
#define LDST_sz_mask		0x3
#define LDST_code_bits		12
#define LDST_code_mask		0xf

#define init_LDST			\
{					\
  LDST_opcode,				\
  LDST_reg_bits,	LDST_reg_mask,	\
  LDST_ptr_bits,	LDST_ptr_mask,	\
  LDST_Z_bits,		LDST_Z_mask,	\
  LDST_aop_bits,	LDST_aop_mask,	\
  LDST_W_bits,		LDST_W_mask,	\
  LDST_sz_bits,		LDST_sz_mask,	\
  LDST_code_bits,	LDST_code_mask	\
};

/*  LDSTii
+---+---+---+---|---+---+---+---|---+---+---+---|---+---+---+---+
| 1 | 0 | 1 |.W.|.op....|.offset........|.ptr.......|.reg.......|
+---+---+---+---|---+---+---+---|---+---+---+---|---+---+---+---+
*/

typedef struct
{
  unsigned short opcode;
  int bits_reg;
  int mask_reg;
  int bits_ptr;
  int mask_ptr;
  int bits_offset;
  int mask_offset;
  int bits_op;
  int mask_op;
  int bits_W;
  int mask_W;
  int bits_code;
  int mask_code;
} LDSTii;

#define LDSTii_opcode		0xa000
#define LDSTii_reg_bit		0
#define LDSTii_reg_mask		0x7
#define LDSTii_ptr_bit		3
#define LDSTii_ptr_mask		0x7
#define LDSTii_offset_bit	6
#define LDSTii_offset_mask	0xf
#define LDSTii_op_bit		10
#define LDSTii_op_mask		0x3
#define LDSTii_W_bit		12
#define LDSTii_W_mask		0x1
#define LDSTii_code_bit		13
#define LDSTii_code_mask	0x7

#define init_LDSTii 				\
{						\
  LDSTii_opcode,				\
  LDSTii_reg_bit,	LDSTii_reg_mask,	\
  LDSTii_ptr_bit,	LDSTii_ptr_mask,	\
  LDSTii_offset_bit,    LDSTii_offset_mask, 	\
  LDSTii_op_bit,        LDSTii_op_mask,		\
  LDSTii_W_bit,		LDSTii_W_mask,		\
  LDSTii_code_bit,	LDSTii_code_mask	\
};


/*  LDSTiiFP
+---+---+---+---|---+---+---+---|---+---+---+---|---+---+---+---+
| 1 | 0 | 1 | 1 | 1 | 0 |.W.|.offset............|.reg...........|
+---+---+---+---|---+---+---+---|---+---+---+---|---+---+---+---+
*/

typedef struct
{
  unsigned short opcode;
  int bits_reg;
  int mask_reg;
  int bits_offset;
  int mask_offset;
  int bits_W;
  int mask_W;
  int bits_code;
  int mask_code;
} LDSTiiFP;

#define LDSTiiFP_opcode		0xb800
#define LDSTiiFP_reg_bits	0
#define LDSTiiFP_reg_mask	0xf
#define LDSTiiFP_offset_bits	4
#define LDSTiiFP_offset_mask	0x1f
#define LDSTiiFP_W_bits		9
#define LDSTiiFP_W_mask		0x1
#define LDSTiiFP_code_bits	10
#define LDSTiiFP_code_mask	0x3f

#define init_LDSTiiFP				\
{						\
  LDSTiiFP_opcode,				\
  LDSTiiFP_reg_bits,	LDSTiiFP_reg_mask,	\
  LDSTiiFP_offset_bits, LDSTiiFP_offset_mask,	\
  LDSTiiFP_W_bits,	LDSTiiFP_W_mask,	\
  LDSTiiFP_code_bits,	LDSTiiFP_code_mask	\
};

/*  dspLDST
+---+---+---+---|---+---+---+---|---+---+---+---|---+---+---+---+
| 1 | 0 | 0 | 1 | 1 | 1 |.W.|.aop...|.m.....|.i.....|.reg.......|
+---+---+---+---|---+---+---+---|---+---+---+---|---+---+---+---+
*/

typedef struct
{
  unsigned short opcode;
  int bits_reg;
  int mask_reg;
  int bits_i;
  int mask_i;
  int bits_m;
  int mask_m;
  int bits_aop;
  int mask_aop;
  int bits_W;
  int mask_W;
  int bits_code;
  int mask_code;
} DspLDST;

#define DspLDST_opcode		0x9c00
#define DspLDST_reg_bits	0
#define DspLDST_reg_mask	0x7
#define DspLDST_i_bits		3
#define DspLDST_i_mask		0x3
#define DspLDST_m_bits		5
#define DspLDST_m_mask		0x3
#define DspLDST_aop_bits	7
#define DspLDST_aop_mask	0x3
#define DspLDST_W_bits		9
#define DspLDST_W_mask		0x1
#define DspLDST_code_bits	10
#define DspLDST_code_mask	0x3f

#define init_DspLDST				\
{						\
  DspLDST_opcode,				\
  DspLDST_reg_bits,	DspLDST_reg_mask,	\
  DspLDST_i_bits,	DspLDST_i_mask,		\
  DspLDST_m_bits,	DspLDST_m_mask,		\
  DspLDST_aop_bits,	DspLDST_aop_mask,	\
  DspLDST_W_bits,	DspLDST_W_mask,		\
  DspLDST_code_bits,	DspLDST_code_mask	\
};


/*  LDSTpmod
+---+---+---+---|---+---+---+---|---+---+---+---|---+---+---+---+
| 1 | 0 | 0 | 0 |.W.|.aop...|.reg.......|.idx.......|.ptr.......|
+---+---+---+---|---+---+---+---|---+---+---+---|---+---+---+---+
*/

typedef struct
{
  unsigned short opcode;
  int bits_ptr;
  int mask_ptr;
  int bits_idx;
  int mask_idx;
  int bits_reg;
  int mask_reg;
  int bits_aop;
  int mask_aop;
  int bits_W;
  int mask_W;
  int bits_code;
  int mask_code;
} LDSTpmod;

#define LDSTpmod_opcode		0x8000
#define LDSTpmod_ptr_bits	0
#define LDSTpmod_ptr_mask	0x7
#define LDSTpmod_idx_bits	3
#define LDSTpmod_idx_mask	0x7
#define LDSTpmod_reg_bits	6
#define LDSTpmod_reg_mask	0x7
#define LDSTpmod_aop_bits	9
#define LDSTpmod_aop_mask	0x3
#define LDSTpmod_W_bits		11
#define LDSTpmod_W_mask		0x1
#define LDSTpmod_code_bits	12
#define LDSTpmod_code_mask	0xf

#define init_LDSTpmod				\
{						\
  LDSTpmod_opcode,				\
  LDSTpmod_ptr_bits, 	LDSTpmod_ptr_mask,	\
  LDSTpmod_idx_bits,	LDSTpmod_idx_mask,	\
  LDSTpmod_reg_bits,	LDSTpmod_reg_mask,	\
  LDSTpmod_aop_bits,	LDSTpmod_aop_mask,	\
  LDSTpmod_W_bits,	LDSTpmod_W_mask,	\
  LDSTpmod_code_bits,	LDSTpmod_code_mask	\
};


/*  LOGI2op
+---+---+---+---|---+---+---+---|---+---+---+---|---+---+---+---+
| 0 | 1 | 0 | 0 | 1 |.opc.......|.src...............|.dst.......|
+---+---+---+---|---+---+---+---|---+---+---+---|---+---+---+---+
*/

typedef struct
{
  unsigned short opcode;
  int bits_dst;
  int mask_dst;
  int bits_src;
  int mask_src;
  int bits_opc;
  int mask_opc;
  int bits_code;
  int mask_code;
} LOGI2op;

#define LOGI2op_opcode		0x4800
#define LOGI2op_dst_bits	0
#define LOGI2op_dst_mask	0x7
#define LOGI2op_src_bits	3
#define LOGI2op_src_mask	0x1f
#define LOGI2op_opc_bits	8
#define LOGI2op_opc_mask	0x7
#define LOGI2op_code_bits	11
#define LOGI2op_code_mask	0x1f

#define init_LOGI2op				\
{						\
  LOGI2op_opcode,				\
  LOGI2op_dst_bits, 	LOGI2op_dst_mask,	\
  LOGI2op_src_bits,	LOGI2op_src_mask,	\
  LOGI2op_opc_bits,	LOGI2op_opc_mask,	\
  LOGI2op_code_bits,	LOGI2op_code_mask	\
};


/*  ALU2op
+---+---+---+---|---+---+---+---|---+---+---+---|---+---+---+---+
| 0 | 1 | 0 | 0 | 0 | 0 |.opc...........|.src.......|.dst.......|
+---+---+---+---|---+---+---+---|---+---+---+---|---+---+---+---+
*/

typedef struct
{
  unsigned short opcode;
  int bits_dst;
  int mask_dst;
  int bits_src;
  int mask_src;
  int bits_opc;
  int mask_opc;
  int bits_code;
  int mask_code;
} ALU2op;

#define ALU2op_opcode 		0x4000
#define ALU2op_dst_bits		0
#define ALU2op_dst_mask		0x7
#define ALU2op_src_bits		3
#define ALU2op_src_mask		0x7
#define ALU2op_opc_bits		6
#define ALU2op_opc_mask		0xf
#define ALU2op_code_bits	10
#define ALU2op_code_mask	0x3f

#define init_ALU2op				\
{						\
  ALU2op_opcode,				\
  ALU2op_dst_bits,	ALU2op_dst_mask,	\
  ALU2op_src_bits,	ALU2op_src_mask,	\
  ALU2op_opc_bits,	ALU2op_opc_mask,	\
  ALU2op_code_bits,	ALU2op_code_mask	\
};


/*  BRCC
+---+---+---+---|---+---+---+---|---+---+---+---|---+---+---+---+
| 0 | 0 | 0 | 1 |.T.|.B.|.offset................................|
+---+---+---+---|---+---+---+---|---+---+---+---|---+---+---+---+
*/

typedef struct
{
  unsigned short opcode;
  int bits_offset;
  int mask_offset;
  int bits_B;
  int mask_B;
  int bits_T;
  int mask_T;
  int bits_code;
  int mask_code;
} BRCC;

#define BRCC_opcode		0x1000
#define BRCC_offset_bits	0
#define BRCC_offset_mask	0x3ff
#define BRCC_B_bits		10
#define BRCC_B_mask		0x1
#define BRCC_T_bits		11
#define BRCC_T_mask		0x1
#define BRCC_code_bits		12
#define BRCC_code_mask		0xf

#define init_BRCC				\
{						\
  BRCC_opcode,					\
  BRCC_offset_bits,	BRCC_offset_mask,	\
  BRCC_B_bits,		BRCC_B_mask,		\
  BRCC_T_bits,		BRCC_T_mask,		\
  BRCC_code_bits,	BRCC_code_mask		\
};


/*  UJUMP
+---+---+---+---|---+---+---+---|---+---+---+---|---+---+---+---+
| 0 | 0 | 1 | 0 |.offset........................................|
+---+---+---+---|---+---+---+---|---+---+---+---|---+---+---+---+
*/

typedef struct
{
  unsigned short opcode;
  int bits_offset;
  int mask_offset;
  int bits_code;
  int mask_code;
} UJump;

#define UJump_opcode		0x2000
#define UJump_offset_bits	0
#define UJump_offset_mask	0xfff
#define UJump_code_bits		12
#define UJump_code_mask		0xf

#define init_UJump				\
{						\
  UJump_opcode,					\
  UJump_offset_bits,	UJump_offset_mask,	\
  UJump_code_bits,	UJump_code_mask		\
};


/*  ProgCtrl
+---+---+---+---|---+---+---+---|---+---+---+---|---+---+---+---+
| 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 |.prgfunc.......|.poprnd........|
+---+---+---+---|---+---+---+---|---+---+---+---|---+---+---+---+
*/

typedef struct
{
  unsigned short opcode;
  int bits_poprnd;
  int mask_poprnd;
  int bits_prgfunc;
  int mask_prgfunc;
  int bits_code;
  int mask_code;
} ProgCtrl;

#define ProgCtrl_opcode		0x0000
#define ProgCtrl_poprnd_bits	0
#define ProgCtrl_poprnd_mask	0xf
#define ProgCtrl_prgfunc_bits	4
#define ProgCtrl_prgfunc_mask	0xf
#define ProgCtrl_code_bits	8
#define ProgCtrl_code_mask	0xff

#define init_ProgCtrl					\
{							\
  ProgCtrl_opcode,					\
  ProgCtrl_poprnd_bits,		ProgCtrl_poprnd_mask,	\
  ProgCtrl_prgfunc_bits,	ProgCtrl_prgfunc_mask,	\
  ProgCtrl_code_bits,		ProgCtrl_code_mask	\
};

/*  CALLa
+---+---+---+---|---+---+---+---|---+---+---+---|---+---+---+---+
| 1 | 1 | 1 | 0 | 0 | 0 | 1 |.S.|.msw...........................|
|.lsw...........................................................|
+---+---+---+---|---+---+---+---|---+---+---+---|---+---+---+---+
*/


typedef struct
{
  unsigned long opcode;
  int bits_addr;
  int mask_addr;
  int bits_S;
  int mask_S;
  int bits_code;
  int mask_code;
} CALLa;

#define CALLa_opcode	0xe2000000
#define CALLa_addr_bits	0
#define CALLa_addr_mask	0xffffff
#define CALLa_S_bits	24
#define CALLa_S_mask	0x1
#define CALLa_code_bits	25
#define CALLa_code_mask	0x7f

#define init_CALLa				\
{						\
  CALLa_opcode,					\
  CALLa_addr_bits,	CALLa_addr_mask,	\
  CALLa_S_bits,		CALLa_S_mask,		\
  CALLa_code_bits,	CALLa_code_mask		\
};


/*  pseudoDEBUG
+---+---+---+---|---+---+---+---|---+---+---+---|---+---+---+---+
| 1 | 1 | 1 | 1 | 1 | 0 | 0 | 0 |.fn....|.grp.......|.reg.......|
+---+---+---+---|---+---+---+---|---+---+---+---|---+---+---+---+
*/

typedef struct
{
  unsigned short opcode;
  int bits_reg;
  int mask_reg;
  int bits_grp;
  int mask_grp;
  int bits_fn;
  int mask_fn;
  int bits_code;
  int mask_code;
} PseudoDbg;

#define PseudoDbg_opcode	0xf800
#define PseudoDbg_reg_bits	0
#define PseudoDbg_reg_mask	0x7
#define PseudoDbg_grp_bits	3
#define PseudoDbg_grp_mask	0x7
#define PseudoDbg_fn_bits	6
#define PseudoDbg_fn_mask	0x3
#define PseudoDbg_code_bits	8
#define PseudoDbg_code_mask	0xff

#define init_PseudoDbg				\
{						\
  PseudoDbg_opcode,				\
  PseudoDbg_reg_bits,	PseudoDbg_reg_mask,	\
  PseudoDbg_grp_bits,	PseudoDbg_grp_mask,	\
  PseudoDbg_fn_bits,	PseudoDbg_fn_mask,	\
  PseudoDbg_code_bits,	PseudoDbg_code_mask	\
};

/*  PseudoDbg_assert
+---+---+---+---|---+---+---+---|---+---+---+---|---+---+---+---+
| 1 | 1 | 1 | 1 | 0 | - | - | - | - | - |.dbgop.....|.regtest...|
|.expected......................................................|
+---+---+---+---|---+---+---+---|---+---+---+---|---+---+---+---+
*/

typedef struct
{
  unsigned long opcode;
  int bits_expected;
  int mask_expected;
  int bits_regtest;
  int mask_regtest;
  int bits_dbgop;
  int mask_dbgop;
  int bits_dontcare;
  int mask_dontcare;
  int bits_code;
  int mask_code;
} PseudoDbg_Assert;

#define PseudoDbg_Assert_opcode		0xf0000000
#define PseudoDbg_Assert_expected_bits	0
#define PseudoDbg_Assert_expected_mask	0xffff
#define PseudoDbg_Assert_regtest_bits	16
#define PseudoDbg_Assert_regtest_mask	0x7
#define PseudoDbg_Assert_dbgop_bits	19
#define PseudoDbg_Assert_dbgop_mask	0x7
#define PseudoDbg_Assert_dontcare_bits	22
#define PseudoDbg_Assert_dontcare_mask	0x1f
#define PseudoDbg_Assert_code_bits	27
#define PseudoDbg_Assert_code_mask	0x1f

#define init_PseudoDbg_Assert						\
{									\
  PseudoDbg_Assert_opcode,						\
  PseudoDbg_Assert_expected_bits, 	PseudoDbg_Assert_expected_mask,	\
  PseudoDbg_Assert_regtest_bits, 	PseudoDbg_Assert_regtest_mask,	\
  PseudoDbg_Assert_dbgop_bits, 		PseudoDbg_Assert_dbgop_mask,	\
  PseudoDbg_Assert_dontcare_bits, 	PseudoDbg_Assert_dontcare_mask,	\
  PseudoDbg_Assert_code_bits,	 	PseudoDbg_Assert_code_mask	\
};

/*  CaCTRL
+---+---+---+---|---+---+---+---|---+---+---+---|---+---+---+---+
| 0 | 0 | 0 | 0 | 0 | 0 | 1 | 0 | 0 | 1 |.a.|.op....|.reg.......|
+---+---+---+---|---+---+---+---|---+---+---+---|---+---+---+---+
*/

typedef struct
{
  unsigned short opcode;
  int bits_reg;
  int mask_reg;
  int bits_op;
  int mask_op;
  int bits_a;
  int mask_a;
  int bits_code;
  int mask_code;
} CaCTRL;

#define CaCTRL_opcode		0x0240
#define CaCTRL_reg_bits		0
#define CaCTRL_reg_mask		0x7
#define CaCTRL_op_bits		3
#define CaCTRL_op_mask		0x3
#define CaCTRL_a_bits		5
#define CaCTRL_a_mask		0x1
#define CaCTRL_code_bits	6
#define CaCTRL_code_mask	0x3fff

#define init_CaCTRL				\
{						\
  CaCTRL_opcode,				\
  CaCTRL_reg_bits,	CaCTRL_reg_mask,	\
  CaCTRL_op_bits,	CaCTRL_op_mask,		\
  CaCTRL_a_bits,	CaCTRL_a_mask,		\
  CaCTRL_code_bits,	CaCTRL_code_mask	\
};

/*  PushPopMultiple
+---+---+---+---|---+---+---+---|---+---+---+---|---+---+---+---+
| 0 | 0 | 0 | 0 | 0 | 1 | 0 |.d.|.p.|.W.|.dr........|.pr........|
+---+---+---+---|---+---+---+---|---+---+---+---|---+---+---+---+
*/

typedef struct
{
  unsigned short opcode;
  int bits_pr;
  int mask_pr;
  int bits_dr;
  int mask_dr;
  int bits_W;
  int mask_W;
  int bits_p;
  int mask_p;
  int bits_d;
  int mask_d;
  int bits_code;
  int mask_code;
} PushPopMultiple;

#define PushPopMultiple_opcode		0x0400
#define PushPopMultiple_pr_bits		0
#define PushPopMultiple_pr_mask		0x7
#define PushPopMultiple_dr_bits		3
#define PushPopMultiple_dr_mask		0x7
#define PushPopMultiple_W_bits		6
#define PushPopMultiple_W_mask		0x1
#define PushPopMultiple_p_bits		7
#define PushPopMultiple_p_mask		0x1
#define PushPopMultiple_d_bits		8
#define PushPopMultiple_d_mask		0x1
#define PushPopMultiple_code_bits	8
#define PushPopMultiple_code_mask	0x1

#define init_PushPopMultiple					\
{								\
  PushPopMultiple_opcode,					\
  PushPopMultiple_pr_bits,	PushPopMultiple_pr_mask,	\
  PushPopMultiple_dr_bits,	PushPopMultiple_dr_mask,	\
  PushPopMultiple_W_bits,	PushPopMultiple_W_mask,		\
  PushPopMultiple_p_bits,	PushPopMultiple_p_mask,		\
  PushPopMultiple_d_bits,	PushPopMultiple_d_mask,		\
  PushPopMultiple_code_bits,	PushPopMultiple_code_mask	\
};

/*  PushPopReg
+---+---+---+---|---+---+---+---|---+---+---+---|---+---+---+---+
| 0 | 0 | 0 | 0 | 0 | 0 | 0 | 1 | 0 |.W.|.grp.......|.reg.......|
+---+---+---+---|---+---+---+---|---+---+---+---|---+---+---+---+
*/

typedef struct
{
  unsigned short opcode;
  int bits_reg;
  int mask_reg;
  int bits_grp;
  int mask_grp;
  int bits_W;
  int mask_W;
  int bits_code;
  int mask_code;
} PushPopReg;

#define PushPopReg_opcode	0x0100
#define PushPopReg_reg_bits	0
#define PushPopReg_reg_mask	0x7
#define PushPopReg_grp_bits	3
#define PushPopReg_grp_mask	0x7
#define PushPopReg_W_bits	6
#define PushPopReg_W_mask	0x1
#define PushPopReg_code_bits	7
#define PushPopReg_code_mask	0x1ff

#define init_PushPopReg				\
{						\
  PushPopReg_opcode,				\
  PushPopReg_reg_bits,	PushPopReg_reg_mask,	\
  PushPopReg_grp_bits,	PushPopReg_grp_mask,	\
  PushPopReg_W_bits,	PushPopReg_W_mask,	\
  PushPopReg_code_bits,	PushPopReg_code_mask,	\
};

/*  linkage
+---+---+---+---|---+---+---+---|---+---+---+---|---+---+---+---+
| 1 | 1 | 1 | 0 | 1 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 |.R.|
|.framesize.....................................................|
+---+---+---+---|---+---+---+---|---+---+---+---|---+---+---+---+
*/

typedef struct
{
  unsigned long opcode;
  int bits_framesize;      
  int mask_framesize;
  int bits_R;      
  int mask_R;
  int bits_code;
  int mask_code;
} Linkage;

#define Linkage_opcode		0xe8000000
#define Linkage_framesize_bits	0
#define Linkage_framesize_mask	0xffff
#define Linkage_R_bits		16
#define Linkage_R_mask		0x1
#define Linkage_code_bits	17
#define Linkage_code_mask	0x7fff

#define init_Linkage					\
{							\
  Linkage_opcode,					\
  Linkage_framesize_bits,	Linkage_framesize_mask,	\
  Linkage_R_bits,		Linkage_R_mask,		\
  Linkage_code_bits,		Linkage_code_mask	\
};

/*  LoopSetup
+---+---+---+---|---+---+---+---|---+---+---+---|---+---+---+---+
| 1 | 1 | 1 | 0 | 0 | 0 | 0 | 0 | 1 |.rop...|.c.|.soffset.......|
|.reg...........| - | - |.eoffset...............................|
+---+---+---+---|---+---+---+---|---+---+---+---|---+---+---+---+
*/

typedef struct
{
  unsigned long opcode;
  int bits_eoffset;
  int mask_eoffset; 
  int bits_dontcare;      
  int mask_dontcare;
  int bits_reg;      
  int mask_reg;
  int bits_soffset;      
  int mask_soffset;
  int bits_c;      
  int mask_c;
  int bits_rop;      
  int mask_rop;
  int bits_code;      
  int mask_code;
} LoopSetup;

#define LoopSetup_opcode		0xe0800000
#define LoopSetup_eoffset_bits		0
#define LoopSetup_eoffset_mask		0x3ff
#define LoopSetup_dontcare_bits		10
#define LoopSetup_dontcare_mask		0x3
#define LoopSetup_reg_bits		12
#define LoopSetup_reg_mask		0xf
#define LoopSetup_soffset_bits		16
#define LoopSetup_soffset_mask		0xf
#define LoopSetup_c_bits		20
#define LoopSetup_c_mask		0x1
#define LoopSetup_rop_bits		21
#define LoopSetup_rop_mask		0x3
#define LoopSetup_code_bits		23
#define LoopSetup_code_mask		0x1ff

#define init_LoopSetup						\
{								\
  LoopSetup_opcode,						\
  LoopSetup_eoffset_bits,	LoopSetup_eoffset_mask,		\
  LoopSetup_dontcare_bits,	LoopSetup_dontcare_mask,	\
  LoopSetup_reg_bits,		LoopSetup_reg_mask,		\
  LoopSetup_soffset_bits,	LoopSetup_soffset_mask,		\
  LoopSetup_c_bits,		LoopSetup_c_mask,		\
  LoopSetup_rop_bits,		LoopSetup_rop_mask,		\
  LoopSetup_code_bits,		LoopSetup_code_mask		\
};

/*  LDIMMhalf
+---+---+---+---|---+---+---+---|---+---+---+---|---+---+---+---+
| 1 | 1 | 1 | 0 | 0 | 0 | 0 | 1 |.Z.|.H.|.S.|.grp...|.reg.......|
|.hword.........................................................|
+---+---+---+---|---+---+---+---|---+---+---+---|---+---+---+---+
*/

typedef struct
{
  unsigned long opcode;
  int bits_hword;
  int mask_hword;
  int bits_reg;      
  int mask_reg;
  int bits_grp;      
  int mask_grp;
  int bits_S;      
  int mask_S;
  int bits_H;      
  int mask_H;
  int bits_Z;      
  int mask_Z;
  int bits_code;      
  int mask_code;
} LDIMMhalf;

#define LDIMMhalf_opcode	0xe1000000
#define LDIMMhalf_hword_bits	0
#define LDIMMhalf_hword_mask	0xffff
#define LDIMMhalf_reg_bits	16
#define LDIMMhalf_reg_mask	0x7
#define LDIMMhalf_grp_bits	19
#define LDIMMhalf_grp_mask	0x3
#define LDIMMhalf_S_bits	21
#define LDIMMhalf_S_mask	0x1
#define LDIMMhalf_H_bits	22
#define LDIMMhalf_H_mask	0x1
#define LDIMMhalf_Z_bits	23
#define LDIMMhalf_Z_mask	0x1
#define LDIMMhalf_code_bits	24
#define LDIMMhalf_code_mask	0xff

#define init_LDIMMhalf				\
{						\
  LDIMMhalf_opcode,				\
  LDIMMhalf_hword_bits,	LDIMMhalf_hword_mask,	\
  LDIMMhalf_reg_bits,	LDIMMhalf_reg_mask,	\
  LDIMMhalf_grp_bits,	LDIMMhalf_grp_mask,	\
  LDIMMhalf_S_bits,	LDIMMhalf_S_mask,	\
  LDIMMhalf_H_bits,	LDIMMhalf_H_mask,	\
  LDIMMhalf_Z_bits,	LDIMMhalf_Z_mask,	\
  LDIMMhalf_code_bits,	LDIMMhalf_code_mask	\
};


/*  CC2dreg
+---+---+---+---|---+---+---+---|---+---+---+---|---+---+---+---+
| 0 | 0 | 0 | 0 | 0 | 0 | 1 | 0 | 0 | 0 | 0 |.op....|.reg.......|
+---+---+---+---|---+---+---+---|---+---+---+---|---+---+---+---+
*/

typedef struct
{
  unsigned short opcode;
  int bits_reg;
  int mask_reg;
  int bits_op;      
  int mask_op;
  int bits_code;      
  int mask_code;
} CC2dreg;

#define CC2dreg_opcode		0x0200
#define CC2dreg_reg_bits	0
#define CC2dreg_reg_mask	0x7
#define CC2dreg_op_bits		3
#define CC2dreg_op_mask		0x3
#define CC2dreg_code_bits	5
#define CC2dreg_code_mask	0x7fff

#define init_CC2dreg				\
{						\
  CC2dreg_opcode,				\
  CC2dreg_reg_bits,	CC2dreg_reg_mask,	\
  CC2dreg_op_bits,	CC2dreg_op_mask,	\
  CC2dreg_code_bits,	CC2dreg_code_mask	\
};


/*  PTR2op
+---+---+---+---|---+---+---+---|---+---+---+---|---+---+---+---+
| 0 | 1 | 0 | 0 | 0 | 1 | 0 |.opc.......|.src.......|.dst.......|
+---+---+---+---|---+---+---+---|---+---+---+---|---+---+---+---+
*/

typedef struct
{
  unsigned short opcode;
  int bits_dst;
  int mask_dst;
  int bits_src;      
  int mask_src;
  int bits_opc;      
  int mask_opc;
  int bits_code;      
  int mask_code;
} PTR2op;

#define PTR2op_opcode		0x4400
#define PTR2op_dst_bits		0
#define PTR2op_dst_mask		0x7
#define PTR2op_src_bits		3
#define PTR2op_src_mask		0x7
#define PTR2op_opc_bits		6
#define PTR2op_opc_mask		0x7
#define PTR2op_code_bits	9	
#define PTR2op_code_mask	0x7f

#define init_PTR2op				\
{						\
  PTR2op_opcode,				\
  PTR2op_dst_bits,	PTR2op_dst_mask,	\
  PTR2op_src_bits,	PTR2op_src_mask,	\
  PTR2op_opc_bits,	PTR2op_opc_mask,	\
  PTR2op_code_bits,	PTR2op_code_mask	\
};


/*  COMP3op
+---+---+---+---|---+---+---+---|---+---+---+---|---+---+---+---+
| 0 | 1 | 0 | 1 |.opc.......|.dst.......|.src1......|.src0......|
+---+---+---+---|---+---+---+---|---+---+---+---|---+---+---+---+
*/

typedef struct
{
  unsigned short opcode;
  int bits_src0;
  int mask_src0;
  int bits_src1;      
  int mask_src1;
  int bits_dst;      
  int mask_dst;
  int bits_opc;      
  int mask_opc;
  int bits_code;      
  int mask_code;
} COMP3op;

#define COMP3op_opcode		0x5000
#define COMP3op_src0_bits	0
#define COMP3op_src0_mask	0x7
#define COMP3op_src1_bits	3
#define COMP3op_src1_mask	0x7
#define COMP3op_dst_bits	6
#define COMP3op_dst_mask	0x7
#define COMP3op_opc_bits	9
#define COMP3op_opc_mask	0x7
#define COMP3op_code_bits	12
#define COMP3op_code_mask	0xf

#define init_COMP3op				\
{						\
  COMP3op_opcode,				\
  COMP3op_src0_bits,	COMP3op_src0_mask,	\
  COMP3op_src1_bits,	COMP3op_src1_mask,	\
  COMP3op_dst_bits,	COMP3op_dst_mask,	\
  COMP3op_opc_bits,	COMP3op_opc_mask,	\
  COMP3op_code_bits,	COMP3op_code_mask	\
};

/*  ccMV
+---+---+---+---|---+---+---+---|---+---+---+---|---+---+---+---+
| 0 | 0 | 0 | 0 | 0 | 1 | 1 |.T.|.d.|.s.|.dst.......|.src.......|
+---+---+---+---|---+---+---+---|---+---+---+---|---+---+---+---+
*/

typedef struct
{
  unsigned short opcode;
  int bits_src;
  int mask_src;
  int bits_dst;      
  int mask_dst;
  int bits_s;      
  int mask_s;
  int bits_d;      
  int mask_d;
  int bits_T;      
  int mask_T;
  int bits_code;      
  int mask_code;
} CCmv;

#define CCmv_opcode	0x0600
#define CCmv_src_bits	0
#define CCmv_src_mask	0x7
#define CCmv_dst_bits	3
#define CCmv_dst_mask	0x7
#define CCmv_s_bits	6
#define CCmv_s_mask	0x1
#define CCmv_d_bits	7	
#define CCmv_d_mask	0x1
#define CCmv_T_bits	8
#define CCmv_T_mask	0x1
#define CCmv_code_bits	9
#define CCmv_code_mask	0x7f

#define init_CCmv			\
{					\
  CCmv_opcode,				\
  CCmv_src_bits,	CCmv_src_mask,	\
  CCmv_dst_bits,	CCmv_dst_mask,	\
  CCmv_s_bits,		CCmv_s_mask,	\
  CCmv_d_bits,		CCmv_d_mask,	\
  CCmv_T_bits,		CCmv_T_mask,	\
  CCmv_code_bits,	CCmv_code_mask	\
};


/*  CCflag
+---+---+---+---|---+---+---+---|---+---+---+---|---+---+---+---+
| 0 | 0 | 0 | 0 | 1 |.I.|.opc.......|.G.|.y.........|.x.........|
+---+---+---+---|---+---+---+---|---+---+---+---|---+---+---+---+
*/

typedef struct
{
  unsigned short opcode;
  int bits_x;
  int mask_x;
  int bits_y;      
  int mask_y;
  int bits_G;      
  int mask_G;
  int bits_opc;      
  int mask_opc;
  int bits_I;      
  int mask_I;
  int bits_code;      
  int mask_code;
} CCflag;

#define CCflag_opcode		0x0800
#define CCflag_x_bits		0
#define CCflag_x_mask		0x7
#define CCflag_y_bits		3
#define CCflag_y_mask		0x7
#define CCflag_G_bits		6
#define CCflag_G_mask		0x1
#define CCflag_opc_bits		7
#define CCflag_opc_mask		0x7
#define CCflag_I_bits		10
#define CCflag_I_mask		0x1
#define CCflag_code_bits	11
#define CCflag_code_mask	0x1f

#define init_CCflag				\
{						\
  CCflag_opcode,				\
  CCflag_x_bits,	CCflag_x_mask,		\
  CCflag_y_bits,	CCflag_y_mask,		\
  CCflag_G_bits,	CCflag_G_mask,		\
  CCflag_opc_bits,	CCflag_opc_mask,	\
  CCflag_I_bits,	CCflag_I_mask,		\
  CCflag_code_bits,	CCflag_code_mask,	\
};


/*  CC2stat
+---+---+---+---|---+---+---+---|---+---+---+---|---+---+---+---+
| 0 | 0 | 0 | 0 | 0 | 0 | 1 | 1 |.D.|.op....|.cbit..............|
+---+---+---+---|---+---+---+---|---+---+---+---|---+---+---+---+
*/

typedef struct
{
  unsigned short opcode;
  int bits_cbit;
  int mask_cbit;
  int bits_op;      
  int mask_op;
  int bits_D;      
  int mask_D;
  int bits_code;      
  int mask_code;
} CC2stat;

#define CC2stat_opcode		0x0300
#define CC2stat_cbit_bits	0
#define CC2stat_cbit_mask	0x1f
#define CC2stat_op_bits		5
#define CC2stat_op_mask		0x3
#define CC2stat_D_bits		7
#define CC2stat_D_mask		0x1
#define CC2stat_code_bits	8
#define CC2stat_code_mask	0xff

#define init_CC2stat				\
{						\
  CC2stat_opcode,				\
  CC2stat_cbit_bits,	CC2stat_cbit_mask,	\
  CC2stat_op_bits,	CC2stat_op_mask,	\
  CC2stat_D_bits,	CC2stat_D_mask,		\
  CC2stat_code_bits,	CC2stat_code_mask	\
};


/*  REGMV
+---+---+---+---|---+---+---+---|---+---+---+---|---+---+---+---+
| 0 | 0 | 1 | 1 |.gd........|.gs........|.dst.......|.src.......|
+---+---+---+---|---+---+---+---|---+---+---+---|---+---+---+---+
*/

typedef struct
{
  unsigned short opcode;
  int bits_src;
  int mask_src;
  int bits_dst;      
  int mask_dst;
  int bits_gs;      
  int mask_gs;
  int bits_gd;      
  int mask_gd;
  int bits_code;      
  int mask_code;
} RegMv;

#define RegMv_opcode		0x3000
#define RegMv_src_bits		0
#define RegMv_src_mask		0x7
#define RegMv_dst_bits		3
#define RegMv_dst_mask		0x7
#define RegMv_gs_bits		6
#define RegMv_gs_mask		0x7
#define RegMv_gd_bits		9
#define RegMv_gd_mask		0x7
#define RegMv_code_bits		12
#define RegMv_code_mask		0xf

#define init_RegMv			\
{					\
  RegMv_opcode,				\
  RegMv_src_bits,	RegMv_src_mask,	\
  RegMv_dst_bits,	RegMv_dst_mask,	\
  RegMv_gs_bits,	RegMv_gs_mask,	\
  RegMv_gd_bits,	RegMv_gd_mask,	\
  RegMv_code_bits,	RegMv_code_mask	\
};


/*  COMPI2opD
+---+---+---+---|---+---+---+---|---+---+---+---|---+---+---+---+
| 0 | 1 | 1 | 0 | 0 |.op|.isrc......................|.dst.......|
+---+---+---+---|---+---+---+---|---+---+---+---|---+---+---+---+
*/

typedef struct
{
  unsigned short opcode;
  int bits_dst;
  int mask_dst;
  int bits_src;      
  int mask_src;
  int bits_op;      
  int mask_op;
  int bits_code;      
  int mask_code;
} COMPI2opD;

#define COMPI2opD_opcode	0x6000
#define COMPI2opD_dst_bits	0
#define COMPI2opD_dst_mask	0x7
#define COMPI2opD_src_bits	3
#define COMPI2opD_src_mask	0x7f
#define COMPI2opD_op_bits	10
#define COMPI2opD_op_mask	0x1
#define COMPI2opD_code_bits	11
#define COMPI2opD_code_mask	0x1f

#define init_COMPI2opD				\
{						\
  COMPI2opD_opcode,				\
  COMPI2opD_dst_bits,	COMPI2opD_dst_mask,	\
  COMPI2opD_src_bits,	COMPI2opD_src_mask,	\
  COMPI2opD_op_bits,	COMPI2opD_op_mask,	\
  COMPI2opD_code_bits,	COMPI2opD_code_mask	\
};

/*  COMPI2opP
+---+---+---+---|---+---+---+---|---+---+---+---|---+---+---+---+
| 0 | 1 | 1 | 0 | 1 |.op|.src.......................|.dst.......|
+---+---+---+---|---+---+---+---|---+---+---+---|---+---+---+---+
*/

typedef COMPI2opD COMPI2opP;

#define COMPI2opP_opcode 	0x6800
#define COMPI2opP_dst_bits	0
#define COMPI2opP_dst_mask	0x7
#define COMPI2opP_src_bits	3
#define COMPI2opP_src_mask	0x7f
#define COMPI2opP_op_bits	10
#define COMPI2opP_op_mask	0x1
#define COMPI2opP_code_bits	11
#define COMPI2opP_code_mask	0x1f

#define init_COMPI2opP				\
{						\
  COMPI2opP_opcode,				\
  COMPI2opP_dst_bits,	COMPI2opP_dst_mask,	\
  COMPI2opP_src_bits,	COMPI2opP_src_mask,	\
  COMPI2opP_op_bits,	COMPI2opP_op_mask,	\
  COMPI2opP_code_bits,	COMPI2opP_code_mask	\
};


/*  dagMODim
+---+---+---+---|---+---+---+---|---+---+---+---|---+---+---+---+
| 1 | 0 | 0 | 1 | 1 | 1 | 1 | 0 |.br| 1 | 1 |.op|.m.....|.i.....|
+---+---+---+---|---+---+---+---|---+---+---+---|---+---+---+---+
*/

typedef struct
{
  unsigned short opcode;
  int bits_i;
  int mask_i;
  int bits_m;      
  int mask_m;
  int bits_op;      
  int mask_op;
  int bits_code2;      
  int mask_code2;
  int bits_br;      
  int mask_br;
  int bits_code;      
  int mask_code;
} DagMODim;

#define DagMODim_opcode		0x9e60
#define DagMODim_i_bits		0
#define DagMODim_i_mask		0x3
#define DagMODim_m_bits		2
#define DagMODim_m_mask		0x3
#define DagMODim_op_bits	4
#define DagMODim_op_mask	0x1
#define DagMODim_code2_bits	5
#define DagMODim_code2_mask	0x3
#define DagMODim_br_bits	7
#define DagMODim_br_mask	0x1
#define DagMODim_code_bits	8
#define DagMODim_code_mask	0xff

#define init_DagMODim				\
{						\
  DagMODim_opcode,				\
  DagMODim_i_bits,	DagMODim_i_mask,	\
  DagMODim_m_bits,	DagMODim_m_mask,	\
  DagMODim_op_bits,	DagMODim_op_mask,	\
  DagMODim_code2_bits,	DagMODim_code2_mask,	\
  DagMODim_br_bits,	DagMODim_br_mask,	\
  DagMODim_code_bits,	DagMODim_code_mask	\
};

/*  dagMODik
+---+---+---+---|---+---+---+---|---+---+---+---|---+---+---+---+
| 1 | 0 | 0 | 1 | 1 | 1 | 1 | 1 | 0 | 1 | 1 | 0 |.op....|.i.....|
+---+---+---+---|---+---+---+---|---+---+---+---|---+---+---+---+
*/

typedef struct
{
  unsigned short opcode;
  int bits_i;
  int mask_i;
  int bits_op;
  int mask_op;
  int bits_code;
  int mask_code;
} DagMODik;

#define DagMODik_opcode		0x9f60
#define DagMODik_i_bits		0
#define DagMODik_i_mask		0x3
#define DagMODik_op_bits	2
#define DagMODik_op_mask	0x3
#define DagMODik_code_bits	3
#define DagMODik_code_mask	0xfff

#define init_DagMODik				\
{						\
  DagMODik_opcode,				\
  DagMODik_i_bits,	DagMODik_i_mask,	\
  DagMODik_op_bits,	DagMODik_op_mask,	\
  DagMODik_code_bits,	DagMODik_code_mask	\
};
