/* Definitions for SH opcodes.
   Copyright (C) 1993, 94, 95, 96, 1997 Free Software Foundation, Inc.

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

typedef enum {
	HEX_0,
	HEX_1,
	HEX_2,
	HEX_3,
	HEX_4,
	HEX_5,
	HEX_6,
	HEX_7,
	HEX_8,
	HEX_9,
	HEX_A,
	HEX_B,
	HEX_C,
	HEX_D,
	HEX_E,
	HEX_F,
	REG_N,
	REG_M,
	SDT_REG_N,
	REG_NM,
        REG_B,
	BRANCH_12,
	BRANCH_8,
	DISP_8,
	DISP_4,
	IMM_4,
	IMM_4BY2,
	IMM_4BY4,
	PCRELIMM_8BY2,
	PCRELIMM_8BY4,
	IMM_8,
	IMM_8BY2,
	IMM_8BY4,
	PPI,
	NOPX,
	NOPY,
	MOVX,
	MOVY,
	PSH,
	PMUL,
	PPI3,
	PDC,
	PPIC
} sh_nibble_type;

typedef enum {
	A_END,
	A_BDISP12,
	A_BDISP8,
	A_DEC_M,
	A_DEC_N,
	A_DISP_GBR,
	A_DISP_PC,
	A_DISP_REG_M,
	A_DISP_REG_N,
	A_GBR,
	A_IMM,
	A_INC_M,
	A_INC_N,
	A_IND_M,
	A_IND_N,
	A_PMOD_N,
	A_PMODY_N,
	A_IND_R0_REG_M,
	A_IND_R0_REG_N,
	A_MACH,
	A_MACL,
	A_PR,
	A_R0,
	A_R0_GBR,
	A_REG_M,
	A_REG_N,
	A_REG_B,
	A_SR,
	A_VBR,
	A_MOD,
	A_RE,
	A_RS,
	A_DSR,
	DSP_REG_M,
	DSP_REG_N,
	DSP_REG_X,
	DSP_REG_Y,
	DSP_REG_E,
	DSP_REG_F,
	DSP_REG_G,
	A_A0,
	A_X0,
	A_X1,
	A_Y0,
	A_Y1,
	A_SSR,
	A_SPC,
	A_SGR,
	A_DBR,
	F_REG_N,
	F_REG_M,
	D_REG_N,
	D_REG_M,
	X_REG_N, /* Only used for argument parsing */
	X_REG_M, /* Only used for argument parsing */
	DX_REG_N,
	DX_REG_M,
	V_REG_N,
	V_REG_M,
	XMTRX_M4,
	F_FR0,
	FPUL_N,
	FPUL_M,
	FPSCR_N,
	FPSCR_M
} sh_arg_type;

typedef enum {
  A_A1_NUM =   5,
  A_A0_NUM =   7,
  A_X0_NUM, A_X1_NUM, A_Y0_NUM, A_Y1_NUM,
  A_M0_NUM, A_A1G_NUM, A_M1_NUM, A_A0G_NUM
} sh_dsp_reg_nums;

#define arch_sh1     0x0001
#define arch_sh2     0x0002
#define arch_sh3     0x0004
#define arch_sh3e    0x0008
#define arch_sh4     0x0010
#define arch_sh_dsp  0x0100
#define arch_sh3_dsp 0x0200

#define arch_sh1_up (arch_sh1 | arch_sh2_up)
#define arch_sh2_up (arch_sh2 | arch_sh3_up | arch_sh_dsp)
#define arch_sh3_up (arch_sh3 | arch_sh3e_up | arch_sh3_dsp)
#define arch_sh3e_up (arch_sh3e | arch_sh4_up)
#define arch_sh4_up arch_sh4

#define arch_sh_dsp_up (arch_sh_dsp | arch_sh3_dsp_up)
#define arch_sh3_dsp_up arch_sh3_dsp

typedef struct {
  char *name;
  sh_arg_type arg[4];
  sh_nibble_type nibbles[4];
  int arch;
} sh_opcode_info;

#ifdef DEFINE_TABLE

sh_opcode_info sh_table[] = {

/* 0111nnnni8*1.... add #<imm>,<REG_N>  */{"add",{A_IMM,A_REG_N},{HEX_7,REG_N,IMM_8}, arch_sh1_up},

/* 0011nnnnmmmm1100 add <REG_M>,<REG_N> */{"add",{ A_REG_M,A_REG_N},{HEX_3,REG_N,REG_M,HEX_C}, arch_sh1_up},

/* 0011nnnnmmmm1110 addc <REG_M>,<REG_N>*/{"addc",{ A_REG_M,A_REG_N},{HEX_3,REG_N,REG_M,HEX_E}, arch_sh1_up},

/* 0011nnnnmmmm1111 addv <REG_M>,<REG_N>*/{"addv",{ A_REG_M,A_REG_N},{HEX_3,REG_N,REG_M,HEX_F}, arch_sh1_up},

/* 11001001i8*1.... and #<imm>,R0       */{"and",{A_IMM,A_R0},{HEX_C,HEX_9,IMM_8}, arch_sh1_up},

/* 0010nnnnmmmm1001 and <REG_M>,<REG_N> */{"and",{ A_REG_M,A_REG_N},{HEX_2,REG_N,REG_M,HEX_9}, arch_sh1_up},

/* 11001101i8*1.... and.b #<imm>,@(R0,GBR)*/{"and.b",{A_IMM,A_R0_GBR},{HEX_C,HEX_D,IMM_8}, arch_sh1_up},

/* 1010i12......... bra <bdisp12>       */{"bra",{A_BDISP12},{HEX_A,BRANCH_12}, arch_sh1_up},

/* 1011i12......... bsr <bdisp12>       */{"bsr",{A_BDISP12},{HEX_B,BRANCH_12}, arch_sh1_up},

/* 10001001i8p1.... bt <bdisp8>         */{"bt",{A_BDISP8},{HEX_8,HEX_9,BRANCH_8}, arch_sh1_up},

/* 10001011i8p1.... bf <bdisp8>         */{"bf",{A_BDISP8},{HEX_8,HEX_B,BRANCH_8}, arch_sh1_up},

/* 10001101i8p1.... bt.s <bdisp8>       */{"bt.s",{A_BDISP8},{HEX_8,HEX_D,BRANCH_8}, arch_sh2_up},

/* 10001101i8p1.... bt/s <bdisp8>       */{"bt/s",{A_BDISP8},{HEX_8,HEX_D,BRANCH_8}, arch_sh2_up},

/* 10001111i8p1.... bf.s <bdisp8>       */{"bf.s",{A_BDISP8},{HEX_8,HEX_F,BRANCH_8}, arch_sh2_up},

/* 10001111i8p1.... bf/s <bdisp8>       */{"bf/s",{A_BDISP8},{HEX_8,HEX_F,BRANCH_8}, arch_sh2_up},

/* 0000000000101000 clrmac              */{"clrmac",{0},{HEX_0,HEX_0,HEX_2,HEX_8}, arch_sh1_up},

/* 0000000001001000 clrs                */{"clrs",{0},{HEX_0,HEX_0,HEX_4,HEX_8}, arch_sh1_up},

/* 0000000000001000 clrt                */{"clrt",{0},{HEX_0,HEX_0,HEX_0,HEX_8}, arch_sh1_up},

/* 10001000i8*1.... cmp/eq #<imm>,R0    */{"cmp/eq",{A_IMM,A_R0},{HEX_8,HEX_8,IMM_8}, arch_sh1_up},

/* 0011nnnnmmmm0000 cmp/eq <REG_M>,<REG_N>*/{"cmp/eq",{ A_REG_M,A_REG_N},{HEX_3,REG_N,REG_M,HEX_0}, arch_sh1_up},

/* 0011nnnnmmmm0011 cmp/ge <REG_M>,<REG_N>*/{"cmp/ge",{ A_REG_M,A_REG_N},{HEX_3,REG_N,REG_M,HEX_3}, arch_sh1_up},

/* 0011nnnnmmmm0111 cmp/gt <REG_M>,<REG_N>*/{"cmp/gt",{ A_REG_M,A_REG_N},{HEX_3,REG_N,REG_M,HEX_7}, arch_sh1_up},

/* 0011nnnnmmmm0110 cmp/hi <REG_M>,<REG_N>*/{"cmp/hi",{ A_REG_M,A_REG_N},{HEX_3,REG_N,REG_M,HEX_6}, arch_sh1_up},

/* 0011nnnnmmmm0010 cmp/hs <REG_M>,<REG_N>*/{"cmp/hs",{ A_REG_M,A_REG_N},{HEX_3,REG_N,REG_M,HEX_2}, arch_sh1_up},

/* 0100nnnn00010101 cmp/pl <REG_N>      */{"cmp/pl",{A_REG_N},{HEX_4,REG_N,HEX_1,HEX_5}, arch_sh1_up},

/* 0100nnnn00010001 cmp/pz <REG_N>      */{"cmp/pz",{A_REG_N},{HEX_4,REG_N,HEX_1,HEX_1}, arch_sh1_up},

/* 0010nnnnmmmm1100 cmp/str <REG_M>,<REG_N>*/{"cmp/str",{ A_REG_M,A_REG_N},{HEX_2,REG_N,REG_M,HEX_C}, arch_sh1_up},

/* 0010nnnnmmmm0111 div0s <REG_M>,<REG_N>*/{"div0s",{ A_REG_M,A_REG_N},{HEX_2,REG_N,REG_M,HEX_7}, arch_sh1_up},

/* 0000000000011001 div0u               */{"div0u",{0},{HEX_0,HEX_0,HEX_1,HEX_9}, arch_sh1_up},

/* 0011nnnnmmmm0100 div1 <REG_M>,<REG_N>*/{"div1",{ A_REG_M,A_REG_N},{HEX_3,REG_N,REG_M,HEX_4}, arch_sh1_up},

/* 0110nnnnmmmm1110 exts.b <REG_M>,<REG_N>*/{"exts.b",{ A_REG_M,A_REG_N},{HEX_6,REG_N,REG_M,HEX_E}, arch_sh1_up},

/* 0110nnnnmmmm1111 exts.w <REG_M>,<REG_N>*/{"exts.w",{ A_REG_M,A_REG_N},{HEX_6,REG_N,REG_M,HEX_F}, arch_sh1_up},

/* 0110nnnnmmmm1100 extu.b <REG_M>,<REG_N>*/{"extu.b",{ A_REG_M,A_REG_N},{HEX_6,REG_N,REG_M,HEX_C}, arch_sh1_up},

/* 0110nnnnmmmm1101 extu.w <REG_M>,<REG_N>*/{"extu.w",{ A_REG_M,A_REG_N},{HEX_6,REG_N,REG_M,HEX_D}, arch_sh1_up},

/* 0100nnnn00101011 jmp @<REG_N>        */{"jmp",{A_IND_N},{HEX_4,REG_N,HEX_2,HEX_B}, arch_sh1_up},

/* 0100nnnn00001011 jsr @<REG_N>        */{"jsr",{A_IND_N},{HEX_4,REG_N,HEX_0,HEX_B}, arch_sh1_up},

/* 0100nnnn00001110 ldc <REG_N>,SR      */{"ldc",{A_REG_N,A_SR},{HEX_4,REG_N,HEX_0,HEX_E}, arch_sh1_up},

/* 0100nnnn00011110 ldc <REG_N>,GBR     */{"ldc",{A_REG_N,A_GBR},{HEX_4,REG_N,HEX_1,HEX_E}, arch_sh1_up},

/* 0100nnnn00101110 ldc <REG_N>,VBR     */{"ldc",{A_REG_N,A_VBR},{HEX_4,REG_N,HEX_2,HEX_E}, arch_sh1_up},

/* 0100nnnn01011110 ldc <REG_N>,MOD     */{"ldc",{A_REG_N,A_MOD},{HEX_4,REG_N,HEX_5,HEX_E}, arch_sh_dsp_up},

/* 0100nnnn01111110 ldc <REG_N>,RE     */{"ldc",{A_REG_N,A_RE},{HEX_4,REG_N,HEX_7,HEX_E}, arch_sh_dsp_up},

/* 0100nnnn01101110 ldc <REG_N>,RS     */{"ldc",{A_REG_N,A_RS},{HEX_4,REG_N,HEX_6,HEX_E}, arch_sh_dsp_up},

/* 0100nnnn00111110 ldc <REG_N>,SSR     */{"ldc",{A_REG_N,A_SSR},{HEX_4,REG_N,HEX_3,HEX_E}, arch_sh3_up},

/* 0100nnnn01001110 ldc <REG_N>,SPC     */{"ldc",{A_REG_N,A_SPC},{HEX_4,REG_N,HEX_4,HEX_E}, arch_sh3_up},

/* 0100nnnn11111010 ldc <REG_N>,DBR     */{"ldc",{A_REG_N,A_DBR},{HEX_4,REG_N,HEX_F,HEX_A}, arch_sh4_up},

/* 0100nnnn1xxx1110 ldc <REG_N>,Rn_BANK */{"ldc",{A_REG_N,A_REG_B},{HEX_4,REG_N,REG_B,HEX_E}, arch_sh3_up},

/* 0100nnnn00000111 ldc.l @<REG_N>+,SR  */{"ldc.l",{A_INC_N,A_SR},{HEX_4,REG_N,HEX_0,HEX_7}, arch_sh1_up},

/* 0100nnnn00010111 ldc.l @<REG_N>+,GBR */{"ldc.l",{A_INC_N,A_GBR},{HEX_4,REG_N,HEX_1,HEX_7}, arch_sh1_up},

/* 0100nnnn00100111 ldc.l @<REG_N>+,VBR */{"ldc.l",{A_INC_N,A_VBR},{HEX_4,REG_N,HEX_2,HEX_7}, arch_sh1_up},

/* 0100nnnn01010111 ldc.l @<REG_N>+,MOD */{"ldc.l",{A_INC_N,A_MOD},{HEX_4,REG_N,HEX_5,HEX_7}, arch_sh_dsp_up},

/* 0100nnnn01110111 ldc.l @<REG_N>+,RE */{"ldc.l",{A_INC_N,A_RE},{HEX_4,REG_N,HEX_7,HEX_7}, arch_sh_dsp_up},

/* 0100nnnn01100111 ldc.l @<REG_N>+,RS */{"ldc.l",{A_INC_N,A_RS},{HEX_4,REG_N,HEX_6,HEX_7}, arch_sh_dsp_up},

/* 0100nnnn00110111 ldc.l @<REG_N>+,SSR */{"ldc.l",{A_INC_N,A_SSR},{HEX_4,REG_N,HEX_3,HEX_7}, arch_sh3_up},

/* 0100nnnn01000111 ldc.l @<REG_N>+,SPC */{"ldc.l",{A_INC_N,A_SPC},{HEX_4,REG_N,HEX_4,HEX_7}, arch_sh3_up},

/* 0100nnnn11110110 ldc.l @<REG_N>+,DBR */{"ldc.l",{A_INC_N,A_DBR},{HEX_4,REG_N,HEX_F,HEX_6}, arch_sh4_up},

/* 0100nnnn1xxx0111 ldc.l <REG_N>,Rn_BANK */{"ldc.l",{A_INC_N,A_REG_B},{HEX_4,REG_N,REG_B,HEX_7}, arch_sh3_up},

/* 10001110i8p2.... ldre @(<disp>,PC)	*/{"ldre",{A_BDISP8},{HEX_8,HEX_E,BRANCH_8}, arch_sh_dsp_up},

/* 10001100i8p2.... ldrs @(<disp>,PC)	*/{"ldrs",{A_BDISP8},{HEX_8,HEX_C,BRANCH_8}, arch_sh_dsp_up},

/* 0100nnnn00001010 lds <REG_N>,MACH    */{"lds",{A_REG_N,A_MACH},{HEX_4,REG_N,HEX_0,HEX_A}, arch_sh1_up},

/* 0100nnnn00011010 lds <REG_N>,MACL    */{"lds",{A_REG_N,A_MACL},{HEX_4,REG_N,HEX_1,HEX_A}, arch_sh1_up},

/* 0100nnnn00101010 lds <REG_N>,PR      */{"lds",{A_REG_N,A_PR},{HEX_4,REG_N,HEX_2,HEX_A}, arch_sh1_up},

/* 0100nnnn01101010 lds <REG_N>,DSR	*/{"lds",{A_REG_N,A_DSR},{HEX_4,REG_N,HEX_6,HEX_A}, arch_sh_dsp_up},

/* 0100nnnn01111010 lds <REG_N>,A0	*/{"lds",{A_REG_N,A_A0},{HEX_4,REG_N,HEX_7,HEX_A}, arch_sh_dsp_up},

/* 0100nnnn10001010 lds <REG_N>,X0	*/{"lds",{A_REG_N,A_X0},{HEX_4,REG_N,HEX_8,HEX_A}, arch_sh_dsp_up},

/* 0100nnnn10011010 lds <REG_N>,X1	*/{"lds",{A_REG_N,A_X1},{HEX_4,REG_N,HEX_9,HEX_A}, arch_sh_dsp_up},

/* 0100nnnn10101010 lds <REG_N>,Y0	*/{"lds",{A_REG_N,A_Y0},{HEX_4,REG_N,HEX_A,HEX_A}, arch_sh_dsp_up},

/* 0100nnnn10111010 lds <REG_N>,Y1	*/{"lds",{A_REG_N,A_Y1},{HEX_4,REG_N,HEX_B,HEX_A}, arch_sh_dsp_up},

/* 0100nnnn01011010 lds <REG_N>,FPUL    */{"lds",{A_REG_M,FPUL_N},{HEX_4,REG_M,HEX_5,HEX_A}, arch_sh3e_up},

/* 0100nnnn01101010 lds <REG_M>,FPSCR   */{"lds",{A_REG_M,FPSCR_N},{HEX_4,REG_M,HEX_6,HEX_A}, arch_sh3e_up},

/* 0100nnnn00000110 lds.l @<REG_N>+,MACH*/{"lds.l",{A_INC_N,A_MACH},{HEX_4,REG_N,HEX_0,HEX_6}, arch_sh1_up},

/* 0100nnnn00010110 lds.l @<REG_N>+,MACL*/{"lds.l",{A_INC_N,A_MACL},{HEX_4,REG_N,HEX_1,HEX_6}, arch_sh1_up},

/* 0100nnnn00100110 lds.l @<REG_N>+,PR  */{"lds.l",{A_INC_N,A_PR},{HEX_4,REG_N,HEX_2,HEX_6}, arch_sh1_up},

/* 0100nnnn01100110 lds.l @<REG_N>+,DSR	*/{"lds.l",{A_INC_N,A_DSR},{HEX_4,REG_N,HEX_6,HEX_6}, arch_sh_dsp_up},

/* 0100nnnn01110110 lds.l @<REG_N>+,A0	*/{"lds.l",{A_INC_N,A_A0},{HEX_4,REG_N,HEX_7,HEX_6}, arch_sh_dsp_up},

/* 0100nnnn10000110 lds.l @<REG_N>+,X0	*/{"lds.l",{A_INC_N,A_X0},{HEX_4,REG_N,HEX_8,HEX_6}, arch_sh_dsp_up},

/* 0100nnnn10010110 lds.l @<REG_N>+,X1	*/{"lds.l",{A_INC_N,A_X1},{HEX_4,REG_N,HEX_9,HEX_6}, arch_sh_dsp_up},

/* 0100nnnn10100110 lds.l @<REG_N>+,Y0	*/{"lds.l",{A_INC_N,A_Y0},{HEX_4,REG_N,HEX_A,HEX_6}, arch_sh_dsp_up},

/* 0100nnnn10110110 lds.l @<REG_N>+,Y1	*/{"lds.l",{A_INC_N,A_Y1},{HEX_4,REG_N,HEX_B,HEX_6}, arch_sh_dsp_up},

/* 0100nnnn01010110 lds.l @<REG_M>+,FPUL*/{"lds.l",{A_INC_M,FPUL_N},{HEX_4,REG_M,HEX_5,HEX_6}, arch_sh3e_up},

/* 0100nnnn01100110 lds.l @<REG_M>+,FPSCR*/{"lds.l",{A_INC_M,FPSCR_N},{HEX_4,REG_M,HEX_6,HEX_6}, arch_sh3e_up},

/* 0000000000111000 ldtlb               */{"ldtlb",{0},{HEX_0,HEX_0,HEX_3,HEX_8}, arch_sh3_up},

/* 0100nnnnmmmm1111 mac.w @<REG_M>+,@<REG_N>+*/{"mac.w",{A_INC_M,A_INC_N},{HEX_4,REG_N,REG_M,HEX_F}, arch_sh1_up},

/* 1110nnnni8*1.... mov #<imm>,<REG_N>  */{"mov",{A_IMM,A_REG_N},{HEX_E,REG_N,IMM_8}, arch_sh1_up},

/* 0110nnnnmmmm0011 mov <REG_M>,<REG_N> */{"mov",{ A_REG_M,A_REG_N},{HEX_6,REG_N,REG_M,HEX_3}, arch_sh1_up},

/* 0000nnnnmmmm0100 mov.b <REG_M>,@(R0,<REG_N>)*/{"mov.b",{ A_REG_M,A_IND_R0_REG_N},{HEX_0,REG_N,REG_M,HEX_4}, arch_sh1_up},

/* 0010nnnnmmmm0100 mov.b <REG_M>,@-<REG_N>*/{"mov.b",{ A_REG_M,A_DEC_N},{HEX_2,REG_N,REG_M,HEX_4}, arch_sh1_up},

/* 0010nnnnmmmm0000 mov.b <REG_M>,@<REG_N>*/{"mov.b",{ A_REG_M,A_IND_N},{HEX_2,REG_N,REG_M,HEX_0}, arch_sh1_up},

/* 10000100mmmmi4*1 mov.b @(<disp>,<REG_M>),R0*/{"mov.b",{A_DISP_REG_M,A_R0},{HEX_8,HEX_4,REG_M,IMM_4}, arch_sh1_up},

/* 11000100i8*1.... mov.b @(<disp>,GBR),R0*/{"mov.b",{A_DISP_GBR,A_R0},{HEX_C,HEX_4,IMM_8}, arch_sh1_up},

/* 0000nnnnmmmm1100 mov.b @(R0,<REG_M>),<REG_N>*/{"mov.b",{A_IND_R0_REG_M,A_REG_N},{HEX_0,REG_N,REG_M,HEX_C}, arch_sh1_up},

/* 0110nnnnmmmm0100 mov.b @<REG_M>+,<REG_N>*/{"mov.b",{A_INC_M,A_REG_N},{HEX_6,REG_N,REG_M,HEX_4}, arch_sh1_up},

/* 0110nnnnmmmm0000 mov.b @<REG_M>,<REG_N>*/{"mov.b",{A_IND_M,A_REG_N},{HEX_6,REG_N,REG_M,HEX_0}, arch_sh1_up},

/* 10000000mmmmi4*1 mov.b R0,@(<disp>,<REG_M>)*/{"mov.b",{A_R0,A_DISP_REG_M},{HEX_8,HEX_0,REG_M,IMM_4}, arch_sh1_up},

/* 11000000i8*1.... mov.b R0,@(<disp>,GBR)*/{"mov.b",{A_R0,A_DISP_GBR},{HEX_C,HEX_0,IMM_8}, arch_sh1_up},

/* 0001nnnnmmmmi4*4 mov.l <REG_M>,@(<disp>,<REG_N>)*/{"mov.l",{ A_REG_M,A_DISP_REG_N},{HEX_1,REG_N,REG_M,IMM_4BY4}, arch_sh1_up},

/* 0000nnnnmmmm0110 mov.l <REG_M>,@(R0,<REG_N>)*/{"mov.l",{ A_REG_M,A_IND_R0_REG_N},{HEX_0,REG_N,REG_M,HEX_6}, arch_sh1_up},

/* 0010nnnnmmmm0110 mov.l <REG_M>,@-<REG_N>*/{"mov.l",{ A_REG_M,A_DEC_N},{HEX_2,REG_N,REG_M,HEX_6}, arch_sh1_up},

/* 0010nnnnmmmm0010 mov.l <REG_M>,@<REG_N>*/{"mov.l",{ A_REG_M,A_IND_N},{HEX_2,REG_N,REG_M,HEX_2}, arch_sh1_up},

/* 0101nnnnmmmmi4*4 mov.l @(<disp>,<REG_M>),<REG_N>*/{"mov.l",{A_DISP_REG_M,A_REG_N},{HEX_5,REG_N,REG_M,IMM_4BY4}, arch_sh1_up},

/* 11000110i8*4.... mov.l @(<disp>,GBR),R0*/{"mov.l",{A_DISP_GBR,A_R0},{HEX_C,HEX_6,IMM_8BY4}, arch_sh1_up},

/* 1101nnnni8p4.... mov.l @(<disp>,PC),<REG_N>*/{"mov.l",{A_DISP_PC,A_REG_N},{HEX_D,REG_N,PCRELIMM_8BY4}, arch_sh1_up},

/* 0000nnnnmmmm1110 mov.l @(R0,<REG_M>),<REG_N>*/{"mov.l",{A_IND_R0_REG_M,A_REG_N},{HEX_0,REG_N,REG_M,HEX_E}, arch_sh1_up},

/* 0110nnnnmmmm0110 mov.l @<REG_M>+,<REG_N>*/{"mov.l",{A_INC_M,A_REG_N},{HEX_6,REG_N,REG_M,HEX_6}, arch_sh1_up},

/* 0110nnnnmmmm0010 mov.l @<REG_M>,<REG_N>*/{"mov.l",{A_IND_M,A_REG_N},{HEX_6,REG_N,REG_M,HEX_2}, arch_sh1_up},

/* 11000010i8*4.... mov.l R0,@(<disp>,GBR)*/{"mov.l",{A_R0,A_DISP_GBR},{HEX_C,HEX_2,IMM_8BY4}, arch_sh1_up},

/* 0000nnnnmmmm0101 mov.w <REG_M>,@(R0,<REG_N>)*/{"mov.w",{ A_REG_M,A_IND_R0_REG_N},{HEX_0,REG_N,REG_M,HEX_5}, arch_sh1_up},

/* 0010nnnnmmmm0101 mov.w <REG_M>,@-<REG_N>*/{"mov.w",{ A_REG_M,A_DEC_N},{HEX_2,REG_N,REG_M,HEX_5}, arch_sh1_up},

/* 0010nnnnmmmm0001 mov.w <REG_M>,@<REG_N>*/{"mov.w",{ A_REG_M,A_IND_N},{HEX_2,REG_N,REG_M,HEX_1}, arch_sh1_up},

/* 10000101mmmmi4*2 mov.w @(<disp>,<REG_M>),R0*/{"mov.w",{A_DISP_REG_M,A_R0},{HEX_8,HEX_5,REG_M,IMM_4BY2}, arch_sh1_up},

/* 11000101i8*2.... mov.w @(<disp>,GBR),R0*/{"mov.w",{A_DISP_GBR,A_R0},{HEX_C,HEX_5,IMM_8BY2}, arch_sh1_up},

/* 1001nnnni8p2.... mov.w @(<disp>,PC),<REG_N>*/{"mov.w",{A_DISP_PC,A_REG_N},{HEX_9,REG_N,PCRELIMM_8BY2}, arch_sh1_up},

/* 0000nnnnmmmm1101 mov.w @(R0,<REG_M>),<REG_N>*/{"mov.w",{A_IND_R0_REG_M,A_REG_N},{HEX_0,REG_N,REG_M,HEX_D}, arch_sh1_up},

/* 0110nnnnmmmm0101 mov.w @<REG_M>+,<REG_N>*/{"mov.w",{A_INC_M,A_REG_N},{HEX_6,REG_N,REG_M,HEX_5}, arch_sh1_up},

/* 0110nnnnmmmm0001 mov.w @<REG_M>,<REG_N>*/{"mov.w",{A_IND_M,A_REG_N},{HEX_6,REG_N,REG_M,HEX_1}, arch_sh1_up},

/* 10000001mmmmi4*2 mov.w R0,@(<disp>,<REG_M>)*/{"mov.w",{A_R0,A_DISP_REG_M},{HEX_8,HEX_1,REG_M,IMM_4BY2}, arch_sh1_up},

/* 11000001i8*2.... mov.w R0,@(<disp>,GBR)*/{"mov.w",{A_R0,A_DISP_GBR},{HEX_C,HEX_1,IMM_8BY2}, arch_sh1_up},

/* 11000111i8p4.... mova @(<disp>,PC),R0*/{"mova",{A_DISP_PC,A_R0},{HEX_C,HEX_7,PCRELIMM_8BY4}, arch_sh1_up},
/* 0000nnnn11000011 movca.l R0,@<REG_N> */{"movca.l",{A_R0,A_IND_N},{HEX_0,REG_N,HEX_C,HEX_3}, arch_sh4_up},


/* 0000nnnn00101001 movt <REG_N>        */{"movt",{A_REG_N},{HEX_0,REG_N,HEX_2,HEX_9}, arch_sh1_up},

/* 0010nnnnmmmm1111 muls.w <REG_M>,<REG_N>*/{"muls.w",{ A_REG_M,A_REG_N},{HEX_2,REG_N,REG_M,HEX_F}, arch_sh1_up},
/* 0010nnnnmmmm1111 muls <REG_M>,<REG_N>*/{"muls",{ A_REG_M,A_REG_N},{HEX_2,REG_N,REG_M,HEX_F}, arch_sh1_up},

/* 0000nnnnmmmm0111 mul.l <REG_M>,<REG_N>*/{"mul.l",{ A_REG_M,A_REG_N},{HEX_0,REG_N,REG_M,HEX_7}, arch_sh2_up},

/* 0010nnnnmmmm1110 mulu.w <REG_M>,<REG_N>*/{"mulu.w",{ A_REG_M,A_REG_N},{HEX_2,REG_N,REG_M,HEX_E}, arch_sh1_up},
/* 0010nnnnmmmm1110 mulu <REG_M>,<REG_N>*/{"mulu",{ A_REG_M,A_REG_N},{HEX_2,REG_N,REG_M,HEX_E}, arch_sh1_up},

/* 0110nnnnmmmm1011 neg <REG_M>,<REG_N> */{"neg",{ A_REG_M,A_REG_N},{HEX_6,REG_N,REG_M,HEX_B}, arch_sh1_up},

/* 0110nnnnmmmm1010 negc <REG_M>,<REG_N>*/{"negc",{ A_REG_M,A_REG_N},{HEX_6,REG_N,REG_M,HEX_A}, arch_sh1_up},

/* 0000000000001001 nop                 */{"nop",{0},{HEX_0,HEX_0,HEX_0,HEX_9}, arch_sh1_up},

/* 0110nnnnmmmm0111 not <REG_M>,<REG_N> */{"not",{ A_REG_M,A_REG_N},{HEX_6,REG_N,REG_M,HEX_7}, arch_sh1_up},
/* 0000nnnn10010011 ocbi @<REG_N>       */{"ocbi",{A_IND_N},{HEX_0,REG_N,HEX_9,HEX_3}, arch_sh4_up},

/* 0000nnnn10100011 ocbp @<REG_N>       */{"ocbp",{A_IND_N},{HEX_0,REG_N,HEX_A,HEX_3}, arch_sh4_up},

/* 0000nnnn10110011 ocbwb @<REG_N>      */{"ocbwb",{A_IND_N},{HEX_0,REG_N,HEX_B,HEX_3}, arch_sh4_up},


/* 11001011i8*1.... or #<imm>,R0        */{"or",{A_IMM,A_R0},{HEX_C,HEX_B,IMM_8}, arch_sh1_up},

/* 0010nnnnmmmm1011 or <REG_M>,<REG_N>  */{"or",{ A_REG_M,A_REG_N},{HEX_2,REG_N,REG_M,HEX_B}, arch_sh1_up},

/* 11001111i8*1.... or.b #<imm>,@(R0,GBR)*/{"or.b",{A_IMM,A_R0_GBR},{HEX_C,HEX_F,IMM_8}, arch_sh1_up},

/* 0000nnnn10000011 pref @<REG_N>       */{"pref",{A_IND_N},{HEX_0,REG_N,HEX_8,HEX_3}, arch_sh4_up},

/* 0100nnnn00100100 rotcl <REG_N>       */{"rotcl",{A_REG_N},{HEX_4,REG_N,HEX_2,HEX_4}, arch_sh1_up},

/* 0100nnnn00100101 rotcr <REG_N>       */{"rotcr",{A_REG_N},{HEX_4,REG_N,HEX_2,HEX_5}, arch_sh1_up},

/* 0100nnnn00000100 rotl <REG_N>        */{"rotl",{A_REG_N},{HEX_4,REG_N,HEX_0,HEX_4}, arch_sh1_up},

/* 0100nnnn00000101 rotr <REG_N>        */{"rotr",{A_REG_N},{HEX_4,REG_N,HEX_0,HEX_5}, arch_sh1_up},

/* 0000000000101011 rte                 */{"rte",{0},{HEX_0,HEX_0,HEX_2,HEX_B}, arch_sh1_up},

/* 0000000000001011 rts                 */{"rts",{0},{HEX_0,HEX_0,HEX_0,HEX_B}, arch_sh1_up},

/* 0000000001011000 sets                */{"sets",{0},{HEX_0,HEX_0,HEX_5,HEX_8}, arch_sh1_up},
/* 0000000000011000 sett                */{"sett",{0},{HEX_0,HEX_0,HEX_1,HEX_8}, arch_sh1_up},

/* 0100nnnn00010100 setrc <REG_N>       */{"setrc",{A_REG_N},{HEX_4,REG_N,HEX_1,HEX_4}, arch_sh_dsp_up},

/* 10000010i8*1.... setrc #<imm>        */{"setrc",{A_IMM},{HEX_8,HEX_2,IMM_8}, arch_sh_dsp_up},

/* 0100nnnnmmmm1100 shad <REG_M>,<REG_N>*/{"shad",{ A_REG_M,A_REG_N},{HEX_4,REG_N,REG_M,HEX_C}, arch_sh3_up},

/* 0100nnnnmmmm1101 shld <REG_M>,<REG_N>*/{"shld",{ A_REG_M,A_REG_N},{HEX_4,REG_N,REG_M,HEX_D}, arch_sh3_up},

/* 0100nnnn00100000 shal <REG_N>        */{"shal",{A_REG_N},{HEX_4,REG_N,HEX_2,HEX_0}, arch_sh1_up},

/* 0100nnnn00100001 shar <REG_N>        */{"shar",{A_REG_N},{HEX_4,REG_N,HEX_2,HEX_1}, arch_sh1_up},

/* 0100nnnn00000000 shll <REG_N>        */{"shll",{A_REG_N},{HEX_4,REG_N,HEX_0,HEX_0}, arch_sh1_up},

/* 0100nnnn00101000 shll16 <REG_N>      */{"shll16",{A_REG_N},{HEX_4,REG_N,HEX_2,HEX_8}, arch_sh1_up},

/* 0100nnnn00001000 shll2 <REG_N>       */{"shll2",{A_REG_N},{HEX_4,REG_N,HEX_0,HEX_8}, arch_sh1_up},

/* 0100nnnn00011000 shll8 <REG_N>       */{"shll8",{A_REG_N},{HEX_4,REG_N,HEX_1,HEX_8}, arch_sh1_up},

/* 0100nnnn00000001 shlr <REG_N>        */{"shlr",{A_REG_N},{HEX_4,REG_N,HEX_0,HEX_1}, arch_sh1_up},

/* 0100nnnn00101001 shlr16 <REG_N>      */{"shlr16",{A_REG_N},{HEX_4,REG_N,HEX_2,HEX_9}, arch_sh1_up},

/* 0100nnnn00001001 shlr2 <REG_N>       */{"shlr2",{A_REG_N},{HEX_4,REG_N,HEX_0,HEX_9}, arch_sh1_up},

/* 0100nnnn00011001 shlr8 <REG_N>       */{"shlr8",{A_REG_N},{HEX_4,REG_N,HEX_1,HEX_9}, arch_sh1_up},

/* 0000000000011011 sleep               */{"sleep",{0},{HEX_0,HEX_0,HEX_1,HEX_B}, arch_sh1_up},

/* 0000nnnn00000010 stc SR,<REG_N>      */{"stc",{A_SR,A_REG_N},{HEX_0,REG_N,HEX_0,HEX_2}, arch_sh1_up},

/* 0000nnnn00010010 stc GBR,<REG_N>     */{"stc",{A_GBR,A_REG_N},{HEX_0,REG_N,HEX_1,HEX_2}, arch_sh1_up},

/* 0000nnnn00100010 stc VBR,<REG_N>     */{"stc",{A_VBR,A_REG_N},{HEX_0,REG_N,HEX_2,HEX_2}, arch_sh1_up},

/* 0000nnnn01010010 stc MOD,<REG_N>     */{"stc",{A_MOD,A_REG_N},{HEX_0,REG_N,HEX_5,HEX_2}, arch_sh_dsp_up},

/* 0000nnnn01110010 stc RE,<REG_N>     */{"stc",{A_RE,A_REG_N},{HEX_0,REG_N,HEX_7,HEX_2}, arch_sh_dsp_up},

/* 0000nnnn01100010 stc RS,<REG_N>     */{"stc",{A_RS,A_REG_N},{HEX_0,REG_N,HEX_6,HEX_2}, arch_sh_dsp_up},

/* 0000nnnn00110010 stc SSR,<REG_N>     */{"stc",{A_SSR,A_REG_N},{HEX_0,REG_N,HEX_3,HEX_2}, arch_sh3_up},

/* 0000nnnn01000010 stc SPC,<REG_N>     */{"stc",{A_SPC,A_REG_N},{HEX_0,REG_N,HEX_4,HEX_2}, arch_sh3_up},

/* 0000nnnn00111010 stc SGR,<REG_N>     */{"stc",{A_SGR,A_REG_N},{HEX_0,REG_N,HEX_3,HEX_A}, arch_sh4_up},

/* 0000nnnn11111010 stc DBR,<REG_N>     */{"stc",{A_DBR,A_REG_N},{HEX_0,REG_N,HEX_F,HEX_A}, arch_sh4_up},

/* 0000nnnn1xxx0010 stc Rn_BANK,<REG_N> */{"stc",{A_REG_B,A_REG_N},{HEX_0,REG_N,REG_B,HEX_2}, arch_sh3_up},

/* 0100nnnn00000011 stc.l SR,@-<REG_N>  */{"stc.l",{A_SR,A_DEC_N},{HEX_4,REG_N,HEX_0,HEX_3}, arch_sh1_up},

/* 0100nnnn00100011 stc.l VBR,@-<REG_N> */{"stc.l",{A_VBR,A_DEC_N},{HEX_4,REG_N,HEX_2,HEX_3}, arch_sh1_up},

/* 0100nnnn01010011 stc.l MOD,@-<REG_N> */{"stc.l",{A_MOD,A_DEC_N},{HEX_4,REG_N,HEX_5,HEX_3}, arch_sh_dsp_up},

/* 0100nnnn01110011 stc.l RE,@-<REG_N>  */{"stc.l",{A_RE,A_DEC_N},{HEX_4,REG_N,HEX_7,HEX_3}, arch_sh_dsp_up},

/* 0100nnnn01100011 stc.l RS,@-<REG_N>  */{"stc.l",{A_RS,A_DEC_N},{HEX_4,REG_N,HEX_6,HEX_3}, arch_sh_dsp_up},

/* 0100nnnn00110011 stc.l SSR,@-<REG_N> */{"stc.l",{A_SSR,A_DEC_N},{HEX_4,REG_N,HEX_3,HEX_3}, arch_sh3_up},

/* 0100nnnn01000011 stc.l SPC,@-<REG_N> */{"stc.l",{A_SPC,A_DEC_N},{HEX_4,REG_N,HEX_4,HEX_3}, arch_sh3_up},

/* 0100nnnn00010011 stc.l GBR,@-<REG_N> */{"stc.l",{A_GBR,A_DEC_N},{HEX_4,REG_N,HEX_1,HEX_3}, arch_sh4_up},

/* 0100nnnn00110010 stc.l SGR,@-<REG_N> */{"stc.l",{A_SGR,A_DEC_N},{HEX_4,REG_N,HEX_3,HEX_2}, arch_sh4_up},

/* 0100nnnn11110010 stc.l DBR,@-<REG_N> */{"stc.l",{A_DBR,A_DEC_N},{HEX_4,REG_N,HEX_F,HEX_2}, arch_sh4_up},

/* 0100nnnn1xxx0011 stc.l Rn_BANK,@-<REG_N> */{"stc.l",{A_REG_B,A_DEC_N},{HEX_4,REG_N,REG_B,HEX_3}, arch_sh3_up},

/* 0000nnnn00001010 sts MACH,<REG_N>    */{"sts",{A_MACH,A_REG_N},{HEX_0,REG_N,HEX_0,HEX_A}, arch_sh1_up},

/* 0000nnnn00011010 sts MACL,<REG_N>    */{"sts",{A_MACL,A_REG_N},{HEX_0,REG_N,HEX_1,HEX_A}, arch_sh1_up},

/* 0000nnnn00101010 sts PR,<REG_N>      */{"sts",{A_PR,A_REG_N},{HEX_0,REG_N,HEX_2,HEX_A}, arch_sh1_up},

/* 0000nnnn01101010 sts DSR,<REG_N>	*/{"sts",{A_DSR,A_REG_N},{HEX_0,REG_N,HEX_6,HEX_A}, arch_sh_dsp_up},

/* 0000nnnn01111010 sts A0,<REG_N>	*/{"sts",{A_A0,A_REG_N},{HEX_0,REG_N,HEX_7,HEX_A}, arch_sh_dsp_up},

/* 0000nnnn10001010 sts X0,<REG_N>	*/{"sts",{A_X0,A_REG_N},{HEX_0,REG_N,HEX_8,HEX_A}, arch_sh_dsp_up},

/* 0000nnnn10011010 sts X1,<REG_N>	*/{"sts",{A_X1,A_REG_N},{HEX_0,REG_N,HEX_9,HEX_A}, arch_sh_dsp_up},

/* 0000nnnn10101010 sts Y0,<REG_N>	*/{"sts",{A_Y0,A_REG_N},{HEX_0,REG_N,HEX_A,HEX_A}, arch_sh_dsp_up},

/* 0000nnnn10111010 sts Y1,<REG_N>	*/{"sts",{A_Y1,A_REG_N},{HEX_0,REG_N,HEX_B,HEX_A}, arch_sh_dsp_up},

/* 0000nnnn01011010 sts FPUL,<REG_N>    */{"sts",{FPUL_M,A_REG_N},{HEX_0,REG_N,HEX_5,HEX_A}, arch_sh3e_up},

/* 0000nnnn01101010 sts FPSCR,<REG_N>   */{"sts",{FPSCR_M,A_REG_N},{HEX_0,REG_N,HEX_6,HEX_A}, arch_sh3e_up},

/* 0100nnnn00000010 sts.l MACH,@-<REG_N>*/{"sts.l",{A_MACH,A_DEC_N},{HEX_4,REG_N,HEX_0,HEX_2}, arch_sh1_up},

/* 0100nnnn00010010 sts.l MACL,@-<REG_N>*/{"sts.l",{A_MACL,A_DEC_N},{HEX_4,REG_N,HEX_1,HEX_2}, arch_sh1_up},

/* 0100nnnn00100010 sts.l PR,@-<REG_N>  */{"sts.l",{A_PR,A_DEC_N},{HEX_4,REG_N,HEX_2,HEX_2}, arch_sh1_up},

/* 0100nnnn01100110 sts.l DSR,@-<REG_N>	*/{"sts.l",{A_DSR,A_DEC_N},{HEX_4,REG_N,HEX_6,HEX_2}, arch_sh_dsp_up},

/* 0100nnnn01110110 sts.l A0,@-<REG_N>	*/{"sts.l",{A_A0,A_DEC_N},{HEX_4,REG_N,HEX_7,HEX_2}, arch_sh_dsp_up},

/* 0100nnnn10000110 sts.l X0,@-<REG_N>	*/{"sts.l",{A_X0,A_DEC_N},{HEX_4,REG_N,HEX_8,HEX_2}, arch_sh_dsp_up},

/* 0100nnnn10010110 sts.l X1,@-<REG_N>	*/{"sts.l",{A_X1,A_DEC_N},{HEX_4,REG_N,HEX_9,HEX_2}, arch_sh_dsp_up},

/* 0100nnnn10100110 sts.l Y0,@-<REG_N>	*/{"sts.l",{A_Y0,A_DEC_N},{HEX_4,REG_N,HEX_A,HEX_2}, arch_sh_dsp_up},

/* 0100nnnn10110110 sts.l Y1,@-<REG_N>	*/{"sts.l",{A_Y1,A_DEC_N},{HEX_4,REG_N,HEX_B,HEX_2}, arch_sh_dsp_up},

/* 0100nnnn01010010 sts.l FPUL,@-<REG_N>*/{"sts.l",{FPUL_M,A_DEC_N},{HEX_4,REG_N,HEX_5,HEX_2}, arch_sh3e_up},

/* 0100nnnn01100010 sts.l FPSCR,@-<REG_N>*/{"sts.l",{FPSCR_M,A_DEC_N},{HEX_4,REG_N,HEX_6,HEX_2}, arch_sh3e_up},

/* 0011nnnnmmmm1000 sub <REG_M>,<REG_N> */{"sub",{ A_REG_M,A_REG_N},{HEX_3,REG_N,REG_M,HEX_8}, arch_sh1_up},

/* 0011nnnnmmmm1010 subc <REG_M>,<REG_N>*/{"subc",{ A_REG_M,A_REG_N},{HEX_3,REG_N,REG_M,HEX_A}, arch_sh1_up},

/* 0011nnnnmmmm1011 subv <REG_M>,<REG_N>*/{"subv",{ A_REG_M,A_REG_N},{HEX_3,REG_N,REG_M,HEX_B}, arch_sh1_up},

/* 0110nnnnmmmm1000 swap.b <REG_M>,<REG_N>*/{"swap.b",{ A_REG_M,A_REG_N},{HEX_6,REG_N,REG_M,HEX_8}, arch_sh1_up},

/* 0110nnnnmmmm1001 swap.w <REG_M>,<REG_N>*/{"swap.w",{ A_REG_M,A_REG_N},{HEX_6,REG_N,REG_M,HEX_9}, arch_sh1_up},

/* 0100nnnn00011011 tas.b @<REG_N>      */{"tas.b",{A_IND_N},{HEX_4,REG_N,HEX_1,HEX_B}, arch_sh1_up},

/* 11000011i8*1.... trapa #<imm>        */{"trapa",{A_IMM},{HEX_C,HEX_3,IMM_8}, arch_sh1_up},

/* 11001000i8*1.... tst #<imm>,R0       */{"tst",{A_IMM,A_R0},{HEX_C,HEX_8,IMM_8}, arch_sh1_up},

/* 0010nnnnmmmm1000 tst <REG_M>,<REG_N> */{"tst",{ A_REG_M,A_REG_N},{HEX_2,REG_N,REG_M,HEX_8}, arch_sh1_up},

/* 11001100i8*1.... tst.b #<imm>,@(R0,GBR)*/{"tst.b",{A_IMM,A_R0_GBR},{HEX_C,HEX_C,IMM_8}, arch_sh1_up},

/* 11001010i8*1.... xor #<imm>,R0       */{"xor",{A_IMM,A_R0},{HEX_C,HEX_A,IMM_8}, arch_sh1_up},

/* 0010nnnnmmmm1010 xor <REG_M>,<REG_N> */{"xor",{ A_REG_M,A_REG_N},{HEX_2,REG_N,REG_M,HEX_A}, arch_sh1_up},

/* 11001110i8*1.... xor.b #<imm>,@(R0,GBR)*/{"xor.b",{A_IMM,A_R0_GBR},{HEX_C,HEX_E,IMM_8}, arch_sh1_up},

/* 0010nnnnmmmm1101 xtrct <REG_M>,<REG_N>*/{"xtrct",{ A_REG_M,A_REG_N},{HEX_2,REG_N,REG_M,HEX_D}, arch_sh1_up},

/* 0000nnnnmmmm0111 mul.l <REG_M>,<REG_N>*/{"mul.l",{ A_REG_M,A_REG_N},{HEX_0,REG_N,REG_M,HEX_7}, arch_sh1_up},

/* 0100nnnn00010000 dt <REG_N>          */{"dt",{A_REG_N},{HEX_4,REG_N,HEX_1,HEX_0}, arch_sh2_up},

/* 0011nnnnmmmm1101 dmuls.l <REG_M>,<REG_N>*/{"dmuls.l",{ A_REG_M,A_REG_N},{HEX_3,REG_N,REG_M,HEX_D}, arch_sh2_up},

/* 0011nnnnmmmm0101 dmulu.l <REG_M>,<REG_N>*/{"dmulu.l",{ A_REG_M,A_REG_N},{HEX_3,REG_N,REG_M,HEX_5}, arch_sh2_up},

/* 0000nnnnmmmm1111 mac.l @<REG_M>+,@<REG_N>+*/{"mac.l",{A_INC_M,A_INC_N},{HEX_0,REG_N,REG_M,HEX_F}, arch_sh2_up},

/* 0000nnnn00100011 braf <REG_N>       */{"braf",{A_REG_N},{HEX_0,REG_N,HEX_2,HEX_3}, arch_sh2_up},

/* 0000nnnn00000011 bsrf <REG_N>       */{"bsrf",{A_REG_N},{HEX_0,REG_N,HEX_0,HEX_3}, arch_sh2_up},

/* 111101nnmmmm0000 movs.w @-<REG_N>,<DSP_REG_M> */   {"movs.w",{A_DEC_N,DSP_REG_M},{HEX_F,SDT_REG_N,REG_M,HEX_0}, arch_sh_dsp_up},

/* 111101nnmmmm0001 movs.w @<REG_N>,<DSP_REG_M> */    {"movs.w",{A_IND_N,DSP_REG_M},{HEX_F,SDT_REG_N,REG_M,HEX_1}, arch_sh_dsp_up},

/* 111101nnmmmm0010 movs.w @<REG_N>+,<DSP_REG_M> */   {"movs.w",{A_INC_N,DSP_REG_M},{HEX_F,SDT_REG_N,REG_M,HEX_2}, arch_sh_dsp_up},

/* 111101nnmmmm0011 movs.w @<REG_N>+r8,<DSP_REG_M> */ {"movs.w",{A_PMOD_N,DSP_REG_M},{HEX_F,SDT_REG_N,REG_M,HEX_3}, arch_sh_dsp_up},

/* 111101nnmmmm0100 movs.w <DSP_REG_M>,@-<REG_N> */   {"movs.w",{DSP_REG_M,A_DEC_N},{HEX_F,SDT_REG_N,REG_M,HEX_4}, arch_sh_dsp_up},

/* 111101nnmmmm0101 movs.w <DSP_REG_M>,@<REG_N> */    {"movs.w",{DSP_REG_M,A_IND_N},{HEX_F,SDT_REG_N,REG_M,HEX_5}, arch_sh_dsp_up},

/* 111101nnmmmm0110 movs.w <DSP_REG_M>,@<REG_N>+ */   {"movs.w",{DSP_REG_M,A_INC_N},{HEX_F,SDT_REG_N,REG_M,HEX_6}, arch_sh_dsp_up},

/* 111101nnmmmm0111 movs.w <DSP_REG_M>,@<REG_N>+r8 */ {"movs.w",{DSP_REG_M,A_PMOD_N},{HEX_F,SDT_REG_N,REG_M,HEX_7}, arch_sh_dsp_up},

/* 111101nnmmmm1000 movs.l @-<REG_N>,<DSP_REG_M> */   {"movs.l",{A_DEC_N,DSP_REG_M},{HEX_F,SDT_REG_N,REG_M,HEX_8}, arch_sh_dsp_up},

/* 111101nnmmmm1001 movs.l @<REG_N>,<DSP_REG_M> */    {"movs.l",{A_IND_N,DSP_REG_M},{HEX_F,SDT_REG_N,REG_M,HEX_9}, arch_sh_dsp_up},

/* 111101nnmmmm1010 movs.l @<REG_N>+,<DSP_REG_M> */   {"movs.l",{A_INC_N,DSP_REG_M},{HEX_F,SDT_REG_N,REG_M,HEX_A}, arch_sh_dsp_up},

/* 111101nnmmmm1011 movs.l @<REG_N>+r8,<DSP_REG_M> */ {"movs.l",{A_PMOD_N,DSP_REG_M},{HEX_F,SDT_REG_N,REG_M,HEX_B}, arch_sh_dsp_up},

/* 111101nnmmmm1100 movs.l <DSP_REG_M>,@-<REG_N> */   {"movs.l",{DSP_REG_M,A_DEC_N},{HEX_F,SDT_REG_N,REG_M,HEX_C}, arch_sh_dsp_up},

/* 111101nnmmmm1101 movs.l <DSP_REG_M>,@<REG_N> */    {"movs.l",{DSP_REG_M,A_IND_N},{HEX_F,SDT_REG_N,REG_M,HEX_D}, arch_sh_dsp_up},

/* 111101nnmmmm1110 movs.l <DSP_REG_M>,@<REG_N>+ */   {"movs.l",{DSP_REG_M,A_INC_N},{HEX_F,SDT_REG_N,REG_M,HEX_E}, arch_sh_dsp_up},

/* 111101nnmmmm1111 movs.l <DSP_REG_M>,@<REG_N>+r8 */ {"movs.l",{DSP_REG_M,A_PMOD_N},{HEX_F,SDT_REG_N,REG_M,HEX_F}, arch_sh_dsp_up},

/* 0*0*0*00** nopx */ {"nopx",{0},{PPI,NOPX}, arch_sh_dsp_up},
/* *0*0*0**00 nopy */ {"nopy",{0},{PPI,NOPY}, arch_sh_dsp_up},
/* n*m*0*01** movx.w @<REG_N>,<DSP_REG_X> */    {"movx.w",{A_IND_N,DSP_REG_X},{PPI,MOVX,HEX_1}, arch_sh_dsp_up},
/* n*m*0*10** movx.w @<REG_N>+,<DSP_REG_X> */   {"movx.w",{A_INC_N,DSP_REG_X},{PPI,MOVX,HEX_2}, arch_sh_dsp_up},
/* n*m*0*11** movx.w @<REG_N>+r8,<DSP_REG_X> */ {"movx.w",{A_PMOD_N,DSP_REG_X},{PPI,MOVX,HEX_3}, arch_sh_dsp_up},
/* n*m*1*01** movx.w <DSP_REG_M>,@<REG_N> */    {"movx.w",{DSP_REG_M,A_IND_N},{PPI,MOVX,HEX_9}, arch_sh_dsp_up},
/* n*m*1*10** movx.w <DSP_REG_M>,@<REG_N>+ */   {"movx.w",{DSP_REG_M,A_INC_N},{PPI,MOVX,HEX_A}, arch_sh_dsp_up},
/* n*m*1*11** movx.w <DSP_REG_M>,@<REG_N>+r8 */ {"movx.w",{DSP_REG_M,A_PMOD_N},{PPI,MOVX,HEX_B}, arch_sh_dsp_up},
/* *n*m*0**01 movy.w @<REG_N>,<DSP_REG_Y> */    {"movy.w",{A_IND_N,DSP_REG_Y},{PPI,MOVY,HEX_1}, arch_sh_dsp_up},
/* *n*m*0**10 movy.w @<REG_N>+,<DSP_REG_Y> */   {"movy.w",{A_INC_N,DSP_REG_Y},{PPI,MOVY,HEX_2}, arch_sh_dsp_up},
/* *n*m*0**11 movy.w @<REG_N>+r9,<DSP_REG_Y> */ {"movy.w",{A_PMODY_N,DSP_REG_Y},{PPI,MOVY,HEX_3}, arch_sh_dsp_up},
/* *n*m*1**01 movy.w <DSP_REG_M>,@<REG_N> */    {"movy.w",{DSP_REG_M,A_IND_N},{PPI,MOVY,HEX_9}, arch_sh_dsp_up},
/* *n*m*1**10 movy.w <DSP_REG_M>,@<REG_N>+ */   {"movy.w",{DSP_REG_M,A_INC_N},{PPI,MOVY,HEX_A}, arch_sh_dsp_up},
/* *n*m*1**11 movy.w <DSP_REG_M>,@<REG_N>+r9 */ {"movy.w",{DSP_REG_M,A_PMODY_N},{PPI,MOVY,HEX_B}, arch_sh_dsp_up},

/* 01aaeeffxxyyggnn pmuls Se,Sf,Dg */ {"pmuls",{DSP_REG_E,DSP_REG_F,DSP_REG_G},{PPI,PMUL}, arch_sh_dsp_up},
/* 10100000xxyynnnn psubc <DSP_REG_X>,<DSP_REG_Y>,<DSP_REG_N> */
{"psubc",{DSP_REG_X,DSP_REG_Y,DSP_REG_N},{PPI,PPI3,HEX_A,HEX_0}, arch_sh_dsp_up},
/* 10110000xxyynnnn paddc <DSP_REG_X>,<DSP_REG_Y>,<DSP_REG_N> */
{"paddc",{DSP_REG_X,DSP_REG_Y,DSP_REG_N},{PPI,PPI3,HEX_B,HEX_0}, arch_sh_dsp_up},
/* 10000100xxyynnnn pcmp <DSP_REG_X>,<DSP_REG_Y> */
{"pcmp", {DSP_REG_X,DSP_REG_Y},{PPI,PPI3,HEX_8,HEX_4}, arch_sh_dsp_up},
/* 10100100xxyynnnn pwsb <DSP_REG_X>,<DSP_REG_Y>,<DSP_REG_N> */
{"pwsb", {DSP_REG_X,DSP_REG_Y,DSP_REG_N},{PPI,PPI3,HEX_A,HEX_4}, arch_sh_dsp_up},
/* 10110100xxyynnnn pwad <DSP_REG_X>,<DSP_REG_Y>,<DSP_REG_N> */
{"pwad", {DSP_REG_X,DSP_REG_Y,DSP_REG_N},{PPI,PPI3,HEX_B,HEX_4}, arch_sh_dsp_up},
/* 10001000xxyynnnn pabs <DSP_REG_X>,<DSP_REG_N> */
{"pabs", {DSP_REG_X,DSP_REG_N},{PPI,PPI3,HEX_8,HEX_8}, arch_sh_dsp_up},
/* 10011000xxyynnnn prnd <DSP_REG_X>,<DSP_REG_N> */
{"prnd", {DSP_REG_X,DSP_REG_N},{PPI,PPI3,HEX_9,HEX_8}, arch_sh_dsp_up},
/* 10101000xxyynnnn pabs <DSP_REG_Y>,<DSP_REG_N> */
{"pabs", {DSP_REG_Y,DSP_REG_N},{PPI,PPI3,HEX_A,HEX_8}, arch_sh_dsp_up},
/* 10111000xxyynnnn prnd <DSP_REG_Y>,<DSP_REG_N> */
{"prnd", {DSP_REG_Y,DSP_REG_N},{PPI,PPI3,HEX_B,HEX_8}, arch_sh_dsp_up},

{"dct",{0},{PPI,PDC,HEX_1}, arch_sh_dsp_up},
{"dcf",{0},{PPI,PDC,HEX_2}, arch_sh_dsp_up},

/* 00000iiiiiiinnnn pshl #<imm>,<DSP_REG_N> */ {"pshl",{A_IMM,DSP_REG_N},{PPI,PSH,HEX_0}, arch_sh_dsp_up},
/* 10000001xxyynnnn pshl <DSP_REG_X>,<DSP_REG_Y>,<DSP_REG_N> */
{"pshl", {DSP_REG_X,DSP_REG_Y,DSP_REG_N},{PPI,PPIC,HEX_8,HEX_1}, arch_sh_dsp_up},
/* 00010iiiiiiinnnn psha #<imm>,<DSP_REG_N> */ {"psha",{A_IMM,DSP_REG_N},{PPI,PSH,HEX_1}, arch_sh_dsp_up},
/* 10010001xxyynnnn psha <DSP_REG_X>,<DSP_REG_Y>,<DSP_REG_N> */
{"psha", {DSP_REG_X,DSP_REG_Y,DSP_REG_N},{PPI,PPIC,HEX_9,HEX_1}, arch_sh_dsp_up},
/* 10100001xxyynnnn psub <DSP_REG_X>,<DSP_REG_Y>,<DSP_REG_N> */
{"psub", {DSP_REG_X,DSP_REG_Y,DSP_REG_N},{PPI,PPIC,HEX_A,HEX_1}, arch_sh_dsp_up},
/* 10110001xxyynnnn padd <DSP_REG_X>,<DSP_REG_Y>,<DSP_REG_N> */
{"padd", {DSP_REG_X,DSP_REG_Y,DSP_REG_N},{PPI,PPIC,HEX_B,HEX_1}, arch_sh_dsp_up},
/* 10010101xxyynnnn pand <DSP_REG_X>,<DSP_REG_Y>,<DSP_REG_N> */
{"pand", {DSP_REG_X,DSP_REG_Y,DSP_REG_N},{PPI,PPIC,HEX_9,HEX_5}, arch_sh_dsp_up},
/* 10100101xxyynnnn pxor <DSP_REG_X>,<DSP_REG_Y>,<DSP_REG_N> */
{"pxor", {DSP_REG_X,DSP_REG_Y,DSP_REG_N},{PPI,PPIC,HEX_A,HEX_5}, arch_sh_dsp_up},
/* 10110101xxyynnnn por  <DSP_REG_X>,<DSP_REG_Y>,<DSP_REG_N> */
{"por",  {DSP_REG_X,DSP_REG_Y,DSP_REG_N},{PPI,PPIC,HEX_B,HEX_5}, arch_sh_dsp_up},
/* 10001001xxyynnnn pdec <DSP_REG_X>,<DSP_REG_N> */
{"pdec", {DSP_REG_X,DSP_REG_N},{PPI,PPIC,HEX_8,HEX_9}, arch_sh_dsp_up},
/* 10011001xxyynnnn pinc <DSP_REG_X>,<DSP_REG_N> */
{"pinc", {DSP_REG_X,DSP_REG_N},{PPI,PPIC,HEX_9,HEX_9}, arch_sh_dsp_up},
/* 10101001xxyynnnn pdec <DSP_REG_Y>,<DSP_REG_N> */
{"pdec", {DSP_REG_Y,DSP_REG_N},{PPI,PPIC,HEX_A,HEX_9}, arch_sh_dsp_up},
/* 10111001xxyynnnn pinc <DSP_REG_Y>,<DSP_REG_N> */
{"pinc", {DSP_REG_Y,DSP_REG_N},{PPI,PPIC,HEX_B,HEX_9}, arch_sh_dsp_up},
/* 10001101xxyynnnn pclr <DSP_REG_N> */
{"pclr", {DSP_REG_N},{PPI,PPIC,HEX_8,HEX_D}, arch_sh_dsp_up},
/* 10011101xxyynnnn pdmsb <DSP_REG_X>,<DSP_REG_N> */
{"pdmsb", {DSP_REG_X,DSP_REG_N},{PPI,PPIC,HEX_9,HEX_D}, arch_sh_dsp_up},
/* 10111101xxyynnnn pdmsb <DSP_REG_Y>,<DSP_REG_N> */
{"pdmsb", {DSP_REG_Y,DSP_REG_N},{PPI,PPIC,HEX_B,HEX_D}, arch_sh_dsp_up},
/* 11001001xxyynnnn pneg  <DSP_REG_X>,<DSP_REG_N> */
{"pneg",  {DSP_REG_X,DSP_REG_N},{PPI,PPIC,HEX_C,HEX_9}, arch_sh_dsp_up},
/* 11011001xxyynnnn pcopy <DSP_REG_X>,<DSP_REG_N> */
{"pcopy", {DSP_REG_X,DSP_REG_N},{PPI,PPIC,HEX_D,HEX_9}, arch_sh_dsp_up},
/* 11101001xxyynnnn pneg  <DSP_REG_Y>,<DSP_REG_N> */
{"pneg",  {DSP_REG_Y,DSP_REG_N},{PPI,PPIC,HEX_E,HEX_9}, arch_sh_dsp_up},
/* 11111001xxyynnnn pcopy <DSP_REG_Y>,<DSP_REG_N> */
{"pcopy", {DSP_REG_Y,DSP_REG_N},{PPI,PPIC,HEX_F,HEX_9}, arch_sh_dsp_up},
/* 11001101xxyynnnn psts MACH,<DSP_REG_N> */
{"psts", {A_MACH,DSP_REG_N},{PPI,PPIC,HEX_C,HEX_D}, arch_sh_dsp_up},
/* 11011101xxyynnnn psts MACL,<DSP_REG_N> */
{"psts", {A_MACL,DSP_REG_N},{PPI,PPIC,HEX_D,HEX_D}, arch_sh_dsp_up},
/* 11101101xxyynnnn plds <DSP_REG_N>,MACH */
{"plds", {DSP_REG_N,A_MACH},{PPI,PPIC,HEX_E,HEX_D}, arch_sh_dsp_up},
/* 11111101xxyynnnn plds <DSP_REG_N>,MACL */
{"plds", {DSP_REG_N,A_MACL},{PPI,PPIC,HEX_F,HEX_D}, arch_sh_dsp_up},

/* 1111nnnn01011101 fabs <F_REG_N>     */{"fabs",{F_REG_N},{HEX_F,REG_N,HEX_5,HEX_D}, arch_sh3e_up},
/* 1111nnnn01011101 fabs <D_REG_N>     */{"fabs",{D_REG_N},{HEX_F,REG_N,HEX_5,HEX_D}, arch_sh4_up},

/* 1111nnnnmmmm0000 fadd <F_REG_M>,<F_REG_N>*/{"fadd",{F_REG_M,F_REG_N},{HEX_F,REG_N,REG_M,HEX_0}, arch_sh3e_up},
/* 1111nnn0mmm00000 fadd <D_REG_M>,<D_REG_N>*/{"fadd",{D_REG_M,D_REG_N},{HEX_F,REG_N,REG_M,HEX_0}, arch_sh4_up},

/* 1111nnnnmmmm0100 fcmp/eq <F_REG_M>,<F_REG_N>*/{"fcmp/eq",{F_REG_M,F_REG_N},{HEX_F,REG_N,REG_M,HEX_4}, arch_sh3e_up},
/* 1111nnn0mmm00100 fcmp/eq <D_REG_M>,<D_REG_N>*/{"fcmp/eq",{D_REG_M,D_REG_N},{HEX_F,REG_N,REG_M,HEX_4}, arch_sh4_up},

/* 1111nnnnmmmm0101 fcmp/gt <F_REG_M>,<F_REG_N>*/{"fcmp/gt",{F_REG_M,F_REG_N},{HEX_F,REG_N,REG_M,HEX_5}, arch_sh3e_up},
/* 1111nnn0mmm00101 fcmp/gt <D_REG_M>,<D_REG_N>*/{"fcmp/gt",{D_REG_M,D_REG_N},{HEX_F,REG_N,REG_M,HEX_5}, arch_sh4_up},

/* 1111nnn010111101 fcnvds <D_REG_N>,FPUL*/{"fcnvds",{D_REG_N,FPUL_M},{HEX_F,REG_N,HEX_B,HEX_D}, arch_sh4_up},

/* 1111nnn010101101 fcnvsd FPUL,<D_REG_N>*/{"fcnvsd",{FPUL_M,D_REG_N},{HEX_F,REG_N,HEX_A,HEX_D}, arch_sh4_up},

/* 1111nnnnmmmm0011 fdiv <F_REG_M>,<F_REG_N>*/{"fdiv",{F_REG_M,F_REG_N},{HEX_F,REG_N,REG_M,HEX_3}, arch_sh3e_up},
/* 1111nnn0mmm00011 fdiv <D_REG_M>,<D_REG_N>*/{"fdiv",{D_REG_M,D_REG_N},{HEX_F,REG_N,REG_M,HEX_3}, arch_sh4_up},

/* 1111nnmm11101101 fipr <V_REG_M>,<V_REG_N>*/{"fipr",{V_REG_M,V_REG_N},{HEX_F,REG_NM,HEX_E,HEX_D}, arch_sh4_up},

/* 1111nnnn10001101 fldi0 <F_REG_N>    */{"fldi0",{F_REG_N},{HEX_F,REG_N,HEX_8,HEX_D}, arch_sh3e_up},

/* 1111nnnn10011101 fldi1 <F_REG_N>    */{"fldi1",{F_REG_N},{HEX_F,REG_N,HEX_9,HEX_D}, arch_sh3e_up},

/* 1111nnnn00011101 flds <F_REG_N>,FPUL*/{"flds",{F_REG_N,FPUL_M},{HEX_F,REG_N,HEX_1,HEX_D}, arch_sh3e_up},

/* 1111nnnn00101101 float FPUL,<F_REG_N>*/{"float",{FPUL_M,F_REG_N},{HEX_F,REG_N,HEX_2,HEX_D}, arch_sh3e_up},
/* 1111nnnn00101101 float FPUL,<D_REG_N>*/{"float",{FPUL_M,D_REG_N},{HEX_F,REG_N,HEX_2,HEX_D}, arch_sh4_up},

/* 1111nnnnmmmm1110 fmac FR0,<F_REG_M>,<F_REG_N>*/{"fmac",{F_FR0,F_REG_M,F_REG_N},{HEX_F,REG_N,REG_M,HEX_E}, arch_sh3e_up},

/* 1111nnnnmmmm1100 fmov <F_REG_M>,<F_REG_N>*/{"fmov",{F_REG_M,F_REG_N},{HEX_F,REG_N,REG_M,HEX_C}, arch_sh3e_up},
/* 1111nnnnmmmm1100 fmov <DX_REG_M>,<DX_REG_N>*/{"fmov",{DX_REG_M,DX_REG_N},{HEX_F,REG_N,REG_M,HEX_C}, arch_sh4_up},

/* 1111nnnnmmmm1000 fmov @<REG_M>,<F_REG_N>*/{"fmov",{A_IND_M,F_REG_N},{HEX_F,REG_N,REG_M,HEX_8}, arch_sh3e_up},
/* 1111nnnnmmmm1000 fmov @<REG_M>,<DX_REG_N>*/{"fmov",{A_IND_M,DX_REG_N},{HEX_F,REG_N,REG_M,HEX_8}, arch_sh4_up},

/* 1111nnnnmmmm1010 fmov <F_REG_M>,@<REG_N>*/{"fmov",{F_REG_M,A_IND_N},{HEX_F,REG_N,REG_M,HEX_A}, arch_sh3e_up},
/* 1111nnnnmmmm1010 fmov <DX_REG_M>,@<REG_N>*/{"fmov",{DX_REG_M,A_IND_N},{HEX_F,REG_N,REG_M,HEX_A}, arch_sh4_up},

/* 1111nnnnmmmm1001 fmov @<REG_M>+,<F_REG_N>*/{"fmov",{A_INC_M,F_REG_N},{HEX_F,REG_N,REG_M,HEX_9}, arch_sh3e_up},
/* 1111nnnnmmmm1001 fmov @<REG_M>+,<DX_REG_N>*/{"fmov",{A_INC_M,DX_REG_N},{HEX_F,REG_N,REG_M,HEX_9}, arch_sh4_up},

/* 1111nnnnmmmm1011 fmov <F_REG_M>,@-<REG_N>*/{"fmov",{F_REG_M,A_DEC_N},{HEX_F,REG_N,REG_M,HEX_B}, arch_sh3e_up},
/* 1111nnnnmmmm1011 fmov <DX_REG_M>,@-<REG_N>*/{"fmov",{DX_REG_M,A_DEC_N},{HEX_F,REG_N,REG_M,HEX_B}, arch_sh4_up},

/* 1111nnnnmmmm0110 fmov @(R0,<REG_M>),<F_REG_N>*/{"fmov",{A_IND_R0_REG_M,F_REG_N},{HEX_F,REG_N,REG_M,HEX_6}, arch_sh3e_up},
/* 1111nnnnmmmm0110 fmov @(R0,<REG_M>),<DX_REG_N>*/{"fmov",{A_IND_R0_REG_M,DX_REG_N},{HEX_F,REG_N,REG_M,HEX_6}, arch_sh4_up},

/* 1111nnnnmmmm0111 fmov <F_REG_M>,@(R0,<REG_N>)*/{"fmov",{F_REG_M,A_IND_R0_REG_N},{HEX_F,REG_N,REG_M,HEX_7}, arch_sh3e_up},
/* 1111nnnnmmmm0111 fmov <DX_REG_M>,@(R0,<REG_N>)*/{"fmov",{DX_REG_M,A_IND_R0_REG_N},{HEX_F,REG_N,REG_M,HEX_7}, arch_sh4_up},

/* 1111nnnnmmmm1000 fmov.d @<REG_M>,<DX_REG_N>*/{"fmov.d",{A_IND_M,DX_REG_N},{HEX_F,REG_N,REG_M,HEX_8}, arch_sh4_up},

/* 1111nnnnmmmm1010 fmov.d <DX_REG_M>,@<REG_N>*/{"fmov.d",{DX_REG_M,A_IND_N},{HEX_F,REG_N,REG_M,HEX_A}, arch_sh4_up},

/* 1111nnnnmmmm1001 fmov.d @<REG_M>+,<DX_REG_N>*/{"fmov.d",{A_INC_M,DX_REG_N},{HEX_F,REG_N,REG_M,HEX_9}, arch_sh4_up},

/* 1111nnnnmmmm1011 fmov.d <DX_REG_M>,@-<REG_N>*/{"fmov.d",{DX_REG_M,A_DEC_N},{HEX_F,REG_N,REG_M,HEX_B}, arch_sh4_up},

/* 1111nnnnmmmm0110 fmov.d @(R0,<REG_M>),<DX_REG_N>*/{"fmov.d",{A_IND_R0_REG_M,DX_REG_N},{HEX_F,REG_N,REG_M,HEX_6}, arch_sh4_up},

/* 1111nnnnmmmm0111 fmov.d <DX_REG_M>,@(R0,<REG_N>)*/{"fmov.d",{DX_REG_M,A_IND_R0_REG_N},{HEX_F,REG_N,REG_M,HEX_7}, arch_sh4_up},

/* 1111nnnnmmmm1000 fmov.s @<REG_M>,<F_REG_N>*/{"fmov.s",{A_IND_M,F_REG_N},{HEX_F,REG_N,REG_M,HEX_8}, arch_sh3e_up},

/* 1111nnnnmmmm1010 fmov.s <F_REG_M>,@<REG_N>*/{"fmov.s",{F_REG_M,A_IND_N},{HEX_F,REG_N,REG_M,HEX_A}, arch_sh3e_up},

/* 1111nnnnmmmm1001 fmov.s @<REG_M>+,<F_REG_N>*/{"fmov.s",{A_INC_M,F_REG_N},{HEX_F,REG_N,REG_M,HEX_9}, arch_sh3e_up},

/* 1111nnnnmmmm1011 fmov.s <F_REG_M>,@-<REG_N>*/{"fmov.s",{F_REG_M,A_DEC_N},{HEX_F,REG_N,REG_M,HEX_B}, arch_sh3e_up},

/* 1111nnnnmmmm0110 fmov.s @(R0,<REG_M>),<F_REG_N>*/{"fmov.s",{A_IND_R0_REG_M,F_REG_N},{HEX_F,REG_N,REG_M,HEX_6}, arch_sh3e_up},

/* 1111nnnnmmmm0111 fmov.s <F_REG_M>,@(R0,<REG_N>)*/{"fmov.s",{F_REG_M,A_IND_R0_REG_N},{HEX_F,REG_N,REG_M,HEX_7}, arch_sh3e_up},

/* 1111nnnnmmmm0010 fmul <F_REG_M>,<F_REG_N>*/{"fmul",{F_REG_M,F_REG_N},{HEX_F,REG_N,REG_M,HEX_2}, arch_sh3e_up},
/* 1111nnn0mmm00010 fmul <D_REG_M>,<D_REG_N>*/{"fmul",{D_REG_M,D_REG_N},{HEX_F,REG_N,REG_M,HEX_2}, arch_sh4_up},

/* 1111nnnn01001101 fneg <F_REG_N>     */{"fneg",{F_REG_N},{HEX_F,REG_N,HEX_4,HEX_D}, arch_sh3e_up},
/* 1111nnnn01001101 fneg <D_REG_N>     */{"fneg",{D_REG_N},{HEX_F,REG_N,HEX_4,HEX_D}, arch_sh4_up},

/* 1111101111111101 frchg               */{"frchg",{0},{HEX_F,HEX_B,HEX_F,HEX_D}, arch_sh4_up},

/* 1111001111111101 fschg               */{"fschg",{0},{HEX_F,HEX_3,HEX_F,HEX_D}, arch_sh4_up},

/* 1111nnnn01101101 fsqrt <F_REG_N>    */{"fsqrt",{F_REG_N},{HEX_F,REG_N,HEX_6,HEX_D}, arch_sh3e_up},
/* 1111nnnn01101101 fsqrt <D_REG_N>    */{"fsqrt",{D_REG_N},{HEX_F,REG_N,HEX_6,HEX_D}, arch_sh4_up},

/* 1111nnnn00001101 fsts FPUL,<F_REG_N>*/{"fsts",{FPUL_M,F_REG_N},{HEX_F,REG_N,HEX_0,HEX_D}, arch_sh3e_up},

/* 1111nnnnmmmm0001 fsub <F_REG_M>,<F_REG_N>*/{"fsub",{F_REG_M,F_REG_N},{HEX_F,REG_N,REG_M,HEX_1}, arch_sh3e_up},
/* 1111nnn0mmm00001 fsub <D_REG_M>,<D_REG_N>*/{"fsub",{D_REG_M,D_REG_N},{HEX_F,REG_N,REG_M,HEX_1}, arch_sh4_up},

/* 1111nnnn00111101 ftrc <F_REG_N>,FPUL*/{"ftrc",{F_REG_N,FPUL_M},{HEX_F,REG_N,HEX_3,HEX_D}, arch_sh3e_up},
/* 1111nnnn00111101 ftrc <D_REG_N>,FPUL*/{"ftrc",{D_REG_N,FPUL_M},{HEX_F,REG_N,HEX_3,HEX_D}, arch_sh4_up},

/* 1111nn0111111101 ftrv XMTRX_M4,<V_REG_n>*/{"ftrv",{XMTRX_M4,V_REG_N},{HEX_F,REG_NM,HEX_F,HEX_D}, arch_sh4_up},

{ 0 } 
};

#endif
