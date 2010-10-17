/* Instruction opcode header for Renesas 8500.

   Copyright 2001, 2003 Free Software Foundation, Inc.

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
   59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.  */

typedef enum
{
  GR0,GR1,GR2,GR3,GR4,GR5,GR6,GR7,
  GPR0, GPR1, GPR2, GPR3, GPR4, GPR5, GPR6, GPR7,
  GCCR, GPC,
  GSEGC, GSEGD, GSEGE, GSEGT,GLAST
} gdbreg_type;

#define O_XORC 1
#define O_XOR 2
#define O_XCH 3
#define O_UNLK 4
#define O_TST 5
#define O_TRAPA 6
#define O_TRAP_VS 7
#define O_TAS 8
#define O_SWAP 9
#define O_SUBX 10
#define O_SUBS 11
#define O_SUB 12
#define O_STM 13
#define O_STC 14
#define O_SLEEP 15
#define O_SHLR 16
#define O_SHLL 17
#define O_SHAR 18
#define O_SHAL 19
#define O_SCB_NE 20
#define O_SCB_F 21
#define O_SCB_EQ 22
#define O_RTS 23
#define O_RTD 24
#define O_ROTXR 25
#define O_ROTXL 26
#define O_ROTR 27
#define O_ROTL 28
#define O_PRTS 29
#define O_PRTD 30
#define O_PJSR 31
#define O_PJMP 32
#define O_ORC 33
#define O_OR 34
#define O_NOT 35
#define O_NOP 36
#define O_NEG 37
#define O_MULXU 38
#define O_MOVTPE 39
#define O_MOVFPE 40
#define O_MOV 41
#define O_LINK 42
#define O_LDM 43
#define O_LDC 44
#define O_JSR 45
#define O_JMP 46
#define O_EXTU 47
#define O_EXTS 48
#define O_DSUB 49
#define O_DIVXU 50
#define O_DADD 51
#define O_CMP 52
#define O_CLR 53
#define O_BVS 54
#define O_BVC 55
#define O_BTST 56
#define O_BT 57
#define O_BSR 58
#define O_BSET 59
#define O_BRN 60
#define O_BRA 61
#define O_BPT 62
#define O_BPL 63
#define O_BNOT 64
#define O_BNE 65
#define O_BMI 66
#define O_BLT 67
#define O_BLS 68
#define O_BLO 69
#define O_BLE 70
#define O_BHS 71
#define O_BHI 72
#define O_BGT 73
#define O_BGE 74
#define O_BF 75
#define O_BEQ 76
#define O_BCS 77
#define O_BCLR 78
#define O_BCC 79
#define O_ANDC 80
#define O_AND 81
#define O_ADDX 82
#define O_ADDS 83
#define O_ADD 84
#define O_BYTE 128
#define O_WORD 0x000
#define O_UNSZ 0x000
#define FPIND_D8	10
#define RDIND_D16	11
#define RDIND_D8	12
#define SPDEC	13
#define RDIND	14
#define RN	15
#define RNIND_D8	16
#define RNIND_D16	17
#define RNDEC	18
#define RNINC	19
#define RNIND	20
#define SPINC	21
#define ABS16	22
#define ABS24	23
#define PCREL16	24
#define PCREL8	25
#define ABS8	26
#define CRB	27
#define CR	28
#define CRW	29
#define DISP16	30
#define DISP8	31
#define FP	32
#define IMM16	33
#define IMM4	34
#define IMM8	35
#define RLIST	36
#define QIM	37
#define RD	38
#define RS	39
#define SP	40
typedef enum { AC_BAD, AC_EI, AC_RI, AC_D, AC_,AC_ERR, AC_X,AC_B, AC_EE,AC_RR,AC_IE,
 AC_RE,AC_E, AC_I, AC_ER,AC_IRR, AC_IR, AC_RER, AC_ERE,AC_EIE } addr_class_type;
typedef struct {
	short int idx;
	char flags,src1,src2,dst;
	unsigned char flavor;
	char *name;
	int nargs;
	int arg_type[2];
	int length;
	struct { unsigned char contents;unsigned char mask; char insert; } bytes[6];
} h8500_opcode_info;
const h8500_opcode_info h8500_table[]
#ifdef ASSEMBLER_TABLE
#ifdef DEFINE_TABLE
={
/*
{101,'m','I','!','E',O_MOV|O_WORD,"mov.w",2,{IMM16,ABS16},6,	{{0x1d,0xff, },
								   {0x00,0x00,ABS16 },
								   {0x00,0x00, },
								   {0x07,0xff, },
								   {0x00,0x00,IMM16 },{0x00,0x00, }}},*/

{1,'s','E','C','C',O_XORC|O_WORD,"xorc.w",2,{IMM16,CRW},4,	{{0x0c,0xff,0 },{0x00,0x00,IMM16 },{0x00,0x00,0 },{0x68,0xf8,CRW }}},
{2,'s','E','C','C',O_XORC|O_BYTE,"xorc.b",2,{IMM8,CRB},3,	{{0x04,0xff,0 },{0x00,0x00,IMM8 },{0x68,0xf8,CRB }}},
{3,'m','E','D','D',O_XOR|O_WORD,"xor.w",2,{RN,RD},2,	{{0xa8,0xf8,RN },{0x60,0xf8,RD }}},
{3,'m','E','D','D',O_XOR|O_WORD,"xor.w",2,{RNDEC,RD},2,	{{0xb8,0xf8,RN },{0x60,0xf8,RD }}},
{3,'m','E','D','D',O_XOR|O_WORD,"xor.w",2,{RNINC,RD},2,	{{0xc8,0xf8,RN },{0x60,0xf8,RD }}},
{3,'m','E','D','D',O_XOR|O_WORD,"xor.w",2,{RNIND,RD},2,	{{0xd8,0xf8,RN },{0x60,0xf8,RD }}},
{3,'m','E','D','D',O_XOR|O_WORD,"xor.w",2,{ABS8,RD},3,	{{0x0d,0xff,0 },{0x00,0x00,ABS8 },{0x60,0xf8,RD }}},
{3,'m','E','D','D',O_XOR|O_WORD,"xor.w",2,{RNIND_D8,RD},3,	{{0xe8,0xf8,RN },{0x00,0x00,DISP8 },{0x60,0xf8,RD }}},
{3,'m','E','D','D',O_XOR|O_WORD,"xor.w",2,{ABS16,RD},4,	{{0x1d,0xff,0 },{0x00,0x00,ABS16 },{0x00,0x00,0 },{0x60,0xf8,RD }}},
{3,'m','E','D','D',O_XOR|O_WORD,"xor.w",2,{IMM16,RD},4,	{{0x0c,0xff,0 },{0x00,0x00,IMM16 },{0x00,0x00,0 },{0x60,0xf8,RD }}},
{3,'m','E','D','D',O_XOR|O_WORD,"xor.w",2,{RNIND_D16,RD},4,	{{0xf8,0xf8,RN },{0x00,0x00,DISP16 },{0x00,0x00,0 },{0x60,0xf8,RD }}},
{4,'m','E','D','D',O_XOR|O_BYTE,"xor.b",2,{RN,RD},2,	{{0xa0,0xf8,RN },{0x60,0xf8,RD }}},
{4,'m','E','D','D',O_XOR|O_BYTE,"xor.b",2,{RNDEC,RD},2,	{{0xb0,0xf8,RN },{0x60,0xf8,RD }}},
{4,'m','E','D','D',O_XOR|O_BYTE,"xor.b",2,{RNINC,RD},2,	{{0xc0,0xf8,RN },{0x60,0xf8,RD }}},
{4,'m','E','D','D',O_XOR|O_BYTE,"xor.b",2,{RNIND,RD},2,	{{0xd0,0xf8,RN },{0x60,0xf8,RD }}},
{4,'m','E','D','D',O_XOR|O_BYTE,"xor.b",2,{IMM8,RD},3,	{{0x04,0xff,0 },{0x00,0x00,IMM8 },{0x60,0xf8,RD }}},
{4,'m','E','D','D',O_XOR|O_BYTE,"xor.b",2,{ABS8,RD},3,	{{0x05,0xff,0 },{0x00,0x00,ABS8 },{0x60,0xf8,RD }}},
{4,'m','E','D','D',O_XOR|O_BYTE,"xor.b",2,{RNIND_D8,RD},3,	{{0xe0,0xf8,RN },{0x00,0x00,DISP8 },{0x60,0xf8,RD }}},
{4,'m','E','D','D',O_XOR|O_BYTE,"xor.b",2,{ABS16,RD},4,	{{0x15,0xff,0 },{0x00,0x00,ABS16 },{0x00,0x00,0 },{0x60,0xf8,RD }}},
{4,'m','E','D','D',O_XOR|O_BYTE,"xor.b",2,{RNIND_D16,RD},4,	{{0xf0,0xf8,RN },{0x00,0x00,DISP16 },{0x00,0x00,0 },{0x60,0xf8,RD }}},
{5,'m','E','D','D',O_XOR|O_UNSZ,"xor",2,{RN,RD},2,	{{0xa8,0xf8,RN },{0x60,0xf8,RD }}},
{5,'m','E','D','D',O_XOR|O_UNSZ,"xor",2,{RNDEC,RD},2,	{{0xb8,0xf8,RN },{0x60,0xf8,RD }}},
{5,'m','E','D','D',O_XOR|O_UNSZ,"xor",2,{RNIND,RD},2,	{{0xd8,0xf8,RN },{0x60,0xf8,RD }}},
{5,'m','E','D','D',O_XOR|O_UNSZ,"xor",2,{RNINC,RD},2,	{{0xc8,0xf8,RN },{0x60,0xf8,RD }}},
{5,'m','E','D','D',O_XOR|O_UNSZ,"xor",2,{RNIND_D8,RD},3,	{{0xe8,0xf8,RN },{0x00,0x00,DISP8 },{0x60,0xf8,RD }}},
{5,'m','E','D','D',O_XOR|O_UNSZ,"xor",2,{ABS8,RD},3,	{{0x0d,0xff,0 },{0x00,0x00,ABS8 },{0x60,0xf8,RD }}},
{5,'m','E','D','D',O_XOR|O_UNSZ,"xor",2,{IMM16,RD},4,	{{0x0c,0xff,0 },{0x00,0x00,IMM16 },{0x00,0x00,0 },{0x60,0xf8,RD }}},
{5,'m','E','D','D',O_XOR|O_UNSZ,"xor",2,{ABS16,RD},4,	{{0x1d,0xff,0 },{0x00,0x00,ABS16 },{0x00,0x00,0 },{0x60,0xf8,RD }}},
{5,'m','E','D','D',O_XOR|O_UNSZ,"xor",2,{RNIND_D16,RD},4,	{{0xf8,0xf8,RN },{0x00,0x00,DISP16 },{0x00,0x00,0 },{0x60,0xf8,RD }}},
{6,'-','X','!','!',O_XCH|O_WORD,"xch.w",2,{RS,RD},2,	{{0xa8,0xf8,RS },{0x90,0xf8,RD }}},
{7,'-','X','!','!',O_XCH|O_UNSZ,"xch",2,{RS,RD},2,	{{0xa8,0xf8,RS },{0x90,0xf8,RD }}},
{8,'-','B','!','!',O_UNLK|O_UNSZ,"unlk",1,{FP,0},1,	{{0x0f,0xff,0 }}},
{9,'a','E','!','!',O_TST|O_WORD,"tst.w",1,{RN,0},2,	{{0xa8,0xf8,RN },{0x16,0xff,0 }}},
{9,'a','E','!','!',O_TST|O_WORD,"tst.w",1,{RNINC,0},2,	{{0xc8,0xf8,RN },{0x16,0xff,0 }}},
{9,'a','E','!','!',O_TST|O_WORD,"tst.w",1,{RNDEC,0},2,	{{0xb8,0xf8,RN },{0x16,0xff,0 }}},
{9,'a','E','!','!',O_TST|O_WORD,"tst.w",1,{RNIND,0},2,	{{0xd8,0xf8,RN },{0x16,0xff,0 }}},
{9,'a','E','!','!',O_TST|O_WORD,"tst.w",1,{ABS8,0},3,	{{0x0d,0xff,0 },{0x00,0x00,ABS8 },{0x16,0xff,0 }}},
{9,'a','E','!','!',O_TST|O_WORD,"tst.w",1,{RNIND_D8,0},3,	{{0xe8,0xf8,RN },{0x00,0x00,DISP8 },{0x16,0xff,0 }}},
{9,'a','E','!','!',O_TST|O_WORD,"tst.w",1,{ABS16,0},4,	{{0x1d,0xff,0 },{0x00,0x00,ABS16 },{0x00,0x00,0 },{0x16,0xff,0 }}},
{9,'a','E','!','!',O_TST|O_WORD,"tst.w",1,{IMM16,0},4,	{{0x0c,0xff,0 },{0x00,0x00,IMM16 },{0x00,0x00,0 },{0x16,0xff,0 }}},
{9,'a','E','!','!',O_TST|O_WORD,"tst.w",1,{RNIND_D16,0},4,	{{0xf8,0xf8,RN },{0x00,0x00,DISP16 },{0x00,0x00,0 },{0x16,0xff,0 }}},
{10,'a','E','!','!',O_TST|O_BYTE,"tst.b",1,{RN,0},2,	{{0xa0,0xf8,RN },{0x16,0xff,0 }}},
{10,'a','E','!','!',O_TST|O_BYTE,"tst.b",1,{RNDEC,0},2,	{{0xb0,0xf8,RN },{0x16,0xff,0 }}},
{10,'a','E','!','!',O_TST|O_BYTE,"tst.b",1,{RNINC,0},2,	{{0xc0,0xf8,RN },{0x16,0xff,0 }}},
{10,'a','E','!','!',O_TST|O_BYTE,"tst.b",1,{RNIND,0},2,	{{0xd0,0xf8,RN },{0x16,0xff,0 }}},
{10,'a','E','!','!',O_TST|O_BYTE,"tst.b",1,{IMM8,0},3,	{{0x04,0xff,0 },{0x00,0x00,IMM8 },{0x16,0xff,0 }}},
{10,'a','E','!','!',O_TST|O_BYTE,"tst.b",1,{ABS8,0},3,	{{0x05,0xff,0 },{0x00,0x00,ABS8 },{0x16,0xff,0 }}},
{10,'a','E','!','!',O_TST|O_BYTE,"tst.b",1,{RNIND_D8,0},3,	{{0xe0,0xf8,RN },{0x00,0x00,DISP8 },{0x16,0xff,0 }}},
{10,'a','E','!','!',O_TST|O_BYTE,"tst.b",1,{ABS16,0},4,	{{0x15,0xff,0 },{0x00,0x00,ABS16 },{0x00,0x00,0 },{0x16,0xff,0 }}},
{10,'a','E','!','!',O_TST|O_BYTE,"tst.b",1,{RNIND_D16,0},4,	{{0xf0,0xf8,RN },{0x00,0x00,DISP16 },{0x00,0x00,0 },{0x16,0xff,0 }}},
{11,'a','E','!','!',O_TST|O_UNSZ,"tst",1,{RN,0},2,	{{0xa8,0xf8,RN },{0x16,0xff,0 }}},
{11,'a','E','!','!',O_TST|O_UNSZ,"tst",1,{RNIND,0},2,	{{0xd8,0xf8,RN },{0x16,0xff,0 }}},
{11,'a','E','!','!',O_TST|O_UNSZ,"tst",1,{RNDEC,0},2,	{{0xb8,0xf8,RN },{0x16,0xff,0 }}},
{11,'a','E','!','!',O_TST|O_UNSZ,"tst",1,{RNINC,0},2,	{{0xc8,0xf8,RN },{0x16,0xff,0 }}},
{11,'a','E','!','!',O_TST|O_UNSZ,"tst",1,{ABS8,0},3,	{{0x0d,0xff,0 },{0x00,0x00,ABS8 },{0x16,0xff,0 }}},
{11,'a','E','!','!',O_TST|O_UNSZ,"tst",1,{RNIND_D8,0},3,	{{0xe8,0xf8,RN },{0x00,0x00,DISP8 },{0x16,0xff,0 }}},
{11,'a','E','!','!',O_TST|O_UNSZ,"tst",1,{IMM16,0},4,	{{0x0c,0xff,0 },{0x00,0x00,IMM16 },{0x00,0x00,0 },{0x16,0xff,0 }}},
{11,'a','E','!','!',O_TST|O_UNSZ,"tst",1,{ABS16,0},4,	{{0x1d,0xff,0 },{0x00,0x00,ABS16 },{0x00,0x00,0 },{0x16,0xff,0 }}},
{11,'a','E','!','!',O_TST|O_UNSZ,"tst",1,{RNIND_D16,0},4,	{{0xf8,0xf8,RN },{0x00,0x00,DISP16 },{0x00,0x00,0 },{0x16,0xff,0 }}},
{12,'-','I','!','!',O_TRAPA|O_UNSZ,"trapa",1,{IMM4,0},2,	{{0x08,0xff,0 },{0x10,0xf0,IMM4 }}},
{13,'-','B','!','!',O_TRAP_VS|O_UNSZ,"trap/vs",0,{0,0},1,	{{0x09,0xff,0 }}},
{14,'s','E','!','E',O_TAS|O_BYTE,"tas.b",1,{RN,0},2,	{{0xa0,0xf8,RN },{0x17,0xff,0 }}},
{14,'s','E','!','E',O_TAS|O_BYTE,"tas.b",1,{RNINC,0},2,	{{0xc0,0xf8,RN },{0x17,0xff,0 }}},
{14,'s','E','!','E',O_TAS|O_BYTE,"tas.b",1,{RNDEC,0},2,	{{0xb0,0xf8,RN },{0x17,0xff,0 }}},
{14,'s','E','!','E',O_TAS|O_BYTE,"tas.b",1,{RNIND,0},2,	{{0xd0,0xf8,RN },{0x17,0xff,0 }}},
{14,'s','E','!','E',O_TAS|O_BYTE,"tas.b",1,{ABS8,0},3,	{{0x05,0xff,0 },{0x00,0x00,ABS8 },{0x17,0xff,0 }}},
{14,'s','E','!','E',O_TAS|O_BYTE,"tas.b",1,{IMM8,0},3,	{{0x04,0xff,0 },{0x00,0x00,IMM8 },{0x17,0xff,0 }}},
{14,'s','E','!','E',O_TAS|O_BYTE,"tas.b",1,{RNIND_D8,0},3,	{{0xe0,0xf8,RN },{0x00,0x00,DISP8 },{0x17,0xff,0 }}},
{14,'s','E','!','E',O_TAS|O_BYTE,"tas.b",1,{ABS16,0},4,	{{0x15,0xff,0 },{0x00,0x00,ABS16 },{0x00,0x00,0 },{0x17,0xff,0 }}},
{14,'s','E','!','E',O_TAS|O_BYTE,"tas.b",1,{RNIND_D16,0},4,	{{0xf0,0xf8,RN },{0x00,0x00,DISP16 },{0x00,0x00,0 },{0x17,0xff,0 }}},
{15,'s','E','!','E',O_TAS|O_UNSZ,"tas",1,{RN,0},2,	{{0xa0,0xf8,RN },{0x17,0xff,0 }}},
{15,'s','E','!','E',O_TAS|O_UNSZ,"tas",1,{RNIND,0},2,	{{0xd0,0xf8,RN },{0x17,0xff,0 }}},
{15,'s','E','!','E',O_TAS|O_UNSZ,"tas",1,{RNINC,0},2,	{{0xc0,0xf8,RN },{0x17,0xff,0 }}},
{15,'s','E','!','E',O_TAS|O_UNSZ,"tas",1,{RNDEC,0},2,	{{0xb0,0xf8,RN },{0x17,0xff,0 }}},
{15,'s','E','!','E',O_TAS|O_UNSZ,"tas",1,{IMM8,0},3,	{{0x04,0xff,0 },{0x00,0x00,IMM8 },{0x17,0xff,0 }}},
{15,'s','E','!','E',O_TAS|O_UNSZ,"tas",1,{ABS8,0},3,	{{0x05,0xff,0 },{0x00,0x00,ABS8 },{0x17,0xff,0 }}},
{15,'s','E','!','E',O_TAS|O_UNSZ,"tas",1,{RNIND_D8,0},3,	{{0xe0,0xf8,RN },{0x00,0x00,DISP8 },{0x17,0xff,0 }}},
{15,'s','E','!','E',O_TAS|O_UNSZ,"tas",1,{ABS16,0},4,	{{0x15,0xff,0 },{0x00,0x00,ABS16 },{0x00,0x00,0 },{0x17,0xff,0 }}},
{15,'s','E','!','E',O_TAS|O_UNSZ,"tas",1,{RNIND_D16,0},4,	{{0xf0,0xf8,RN },{0x00,0x00,DISP16 },{0x00,0x00,0 },{0x17,0xff,0 }}},
{16,'m','D','!','D',O_SWAP|O_BYTE,"swap.b",1,{RD,0},2,	{{0xa0,0xf8,RD },{0x10,0xff,0 }}},
{17,'m','D','!','D',O_SWAP|O_UNSZ,"swap",1,{RD,0},2,	{{0xa0,0xf8,RD },{0x10,0xff,0 }}},
{18,'a','E','D','D',O_SUBX|O_WORD,"subx.w",2,{RN,RD},2,	{{0xa8,0xf8,RN },{0xb0,0xf8,RD }}},
{18,'a','E','D','D',O_SUBX|O_WORD,"subx.w",2,{RNDEC,RD},2,	{{0xb8,0xf8,RN },{0xb0,0xf8,RD }}},
{18,'a','E','D','D',O_SUBX|O_WORD,"subx.w",2,{RNIND,RD},2,	{{0xd8,0xf8,RN },{0xb0,0xf8,RD }}},
{18,'a','E','D','D',O_SUBX|O_WORD,"subx.w",2,{RNINC,RD},2,	{{0xc8,0xf8,RN },{0xb0,0xf8,RD }}},
{18,'a','E','D','D',O_SUBX|O_WORD,"subx.w",2,{ABS8,RD},3,	{{0x0d,0xff,0 },{0x00,0x00,ABS8 },{0xb0,0xf8,RD }}},
{18,'a','E','D','D',O_SUBX|O_WORD,"subx.w",2,{RNIND_D8,RD},3,	{{0xe8,0xf8,RN },{0x00,0x00,DISP8 },{0xb0,0xf8,RD }}},
{18,'a','E','D','D',O_SUBX|O_WORD,"subx.w",2,{ABS16,RD},4,	{{0x1d,0xff,0 },{0x00,0x00,ABS16 },{0x00,0x00,0 },{0xb0,0xf8,RD }}},
{18,'a','E','D','D',O_SUBX|O_WORD,"subx.w",2,{IMM16,RD},4,	{{0x0c,0xff,0 },{0x00,0x00,IMM16 },{0x00,0x00,0 },{0xb0,0xf8,RD }}},
{18,'a','E','D','D',O_SUBX|O_WORD,"subx.w",2,{RNIND_D16,RD},4,	{{0xf8,0xf8,RN },{0x00,0x00,DISP16 },{0x00,0x00,0 },{0xb0,0xf8,RD }}},
{19,'a','E','D','D',O_SUBX|O_BYTE,"subx.b",2,{RN,RD},2,	{{0xa0,0xf8,RN },{0xb0,0xf8,RD }}},
{19,'a','E','D','D',O_SUBX|O_BYTE,"subx.b",2,{RNINC,RD},2,	{{0xc0,0xf8,RN },{0xb0,0xf8,RD }}},
{19,'a','E','D','D',O_SUBX|O_BYTE,"subx.b",2,{RNIND,RD},2,	{{0xd0,0xf8,RN },{0xb0,0xf8,RD }}},
{19,'a','E','D','D',O_SUBX|O_BYTE,"subx.b",2,{RNDEC,RD},2,	{{0xb0,0xf8,RN },{0xb0,0xf8,RD }}},
{19,'a','E','D','D',O_SUBX|O_BYTE,"subx.b",2,{ABS8,RD},3,	{{0x05,0xff,0 },{0x00,0x00,ABS8 },{0xb0,0xf8,RD }}},
{19,'a','E','D','D',O_SUBX|O_BYTE,"subx.b",2,{IMM8,RD},3,	{{0x04,0xff,0 },{0x00,0x00,IMM8 },{0xb0,0xf8,RD }}},
{19,'a','E','D','D',O_SUBX|O_BYTE,"subx.b",2,{RNIND_D8,RD},3,	{{0xe0,0xf8,RN },{0x00,0x00,DISP8 },{0xb0,0xf8,RD }}},
{19,'a','E','D','D',O_SUBX|O_BYTE,"subx.b",2,{ABS16,RD},4,	{{0x15,0xff,0 },{0x00,0x00,ABS16 },{0x00,0x00,0 },{0xb0,0xf8,RD }}},
{19,'a','E','D','D',O_SUBX|O_BYTE,"subx.b",2,{RNIND_D16,RD},4,	{{0xf0,0xf8,RN },{0x00,0x00,DISP16 },{0x00,0x00,0 },{0xb0,0xf8,RD }}},
{20,'a','E','D','D',O_SUBX|O_UNSZ,"subx",2,{RN,RD},2,	{{0xa8,0xf8,RN },{0xb0,0xf8,RD }}},
{20,'a','E','D','D',O_SUBX|O_UNSZ,"subx",2,{RNDEC,RD},2,	{{0xb8,0xf8,RN },{0xb0,0xf8,RD }}},
{20,'a','E','D','D',O_SUBX|O_UNSZ,"subx",2,{RNINC,RD},2,	{{0xc8,0xf8,RN },{0xb0,0xf8,RD }}},
{20,'a','E','D','D',O_SUBX|O_UNSZ,"subx",2,{RNIND,RD},2,	{{0xd8,0xf8,RN },{0xb0,0xf8,RD }}},
{20,'a','E','D','D',O_SUBX|O_UNSZ,"subx",2,{ABS8,RD},3,	{{0x0d,0xff,0 },{0x00,0x00,ABS8 },{0xb0,0xf8,RD }}},
{20,'a','E','D','D',O_SUBX|O_UNSZ,"subx",2,{RNIND_D8,RD},3,	{{0xe8,0xf8,RN },{0x00,0x00,DISP8 },{0xb0,0xf8,RD }}},
{20,'a','E','D','D',O_SUBX|O_UNSZ,"subx",2,{IMM16,RD},4,	{{0x0c,0xff,0 },{0x00,0x00,IMM16 },{0x00,0x00,0 },{0xb0,0xf8,RD }}},
{20,'a','E','D','D',O_SUBX|O_UNSZ,"subx",2,{ABS16,RD},4,	{{0x1d,0xff,0 },{0x00,0x00,ABS16 },{0x00,0x00,0 },{0xb0,0xf8,RD }}},
{20,'a','E','D','D',O_SUBX|O_UNSZ,"subx",2,{RNIND_D16,RD},4,	{{0xf8,0xf8,RN },{0x00,0x00,DISP16 },{0x00,0x00,0 },{0xb0,0xf8,RD }}},
{21,'-','E','D','D',O_SUBS|O_WORD,"subs.w",2,{RN,RD},2,	{{0xa8,0xf8,RN },{0x38,0xf8,RD }}},
{21,'-','E','D','D',O_SUBS|O_WORD,"subs.w",2,{RNIND,RD},2,	{{0xd8,0xf8,RN },{0x38,0xf8,RD }}},
{21,'-','E','D','D',O_SUBS|O_WORD,"subs.w",2,{RNINC,RD},2,	{{0xc8,0xf8,RN },{0x38,0xf8,RD }}},
{21,'-','E','D','D',O_SUBS|O_WORD,"subs.w",2,{RNDEC,RD},2,	{{0xb8,0xf8,RN },{0x38,0xf8,RD }}},
{21,'-','E','D','D',O_SUBS|O_WORD,"subs.w",2,{ABS8,RD},3,	{{0x0d,0xff,0 },{0x00,0x00,ABS8 },{0x38,0xf8,RD }}},
{21,'-','E','D','D',O_SUBS|O_WORD,"subs.w",2,{RNIND_D8,RD},3,	{{0xe8,0xf8,RN },{0x00,0x00,DISP8 },{0x38,0xf8,RD }}},
{21,'-','E','D','D',O_SUBS|O_WORD,"subs.w",2,{ABS16,RD},4,	{{0x1d,0xff,0 },{0x00,0x00,ABS16 },{0x00,0x00,0 },{0x38,0xf8,RD }}},
{21,'-','E','D','D',O_SUBS|O_WORD,"subs.w",2,{IMM16,RD},4,	{{0x0c,0xff,0 },{0x00,0x00,IMM16 },{0x00,0x00,0 },{0x38,0xf8,RD }}},
{21,'-','E','D','D',O_SUBS|O_WORD,"subs.w",2,{RNIND_D16,RD},4,	{{0xf8,0xf8,RN },{0x00,0x00,DISP16 },{0x00,0x00,0 },{0x38,0xf8,RD }}},
{22,'-','E','D','D',O_SUBS|O_BYTE,"subs.b",2,{RN,RD},2,	{{0xa0,0xf8,RN },{0x38,0xf8,RD }}},
{22,'-','E','D','D',O_SUBS|O_BYTE,"subs.b",2,{RNINC,RD},2,	{{0xc0,0xf8,RN },{0x38,0xf8,RD }}},
{22,'-','E','D','D',O_SUBS|O_BYTE,"subs.b",2,{RNDEC,RD},2,	{{0xb0,0xf8,RN },{0x38,0xf8,RD }}},
{22,'-','E','D','D',O_SUBS|O_BYTE,"subs.b",2,{RNIND,RD},2,	{{0xd0,0xf8,RN },{0x38,0xf8,RD }}},
{22,'-','E','D','D',O_SUBS|O_BYTE,"subs.b",2,{ABS8,RD},3,	{{0x05,0xff,0 },{0x00,0x00,ABS8 },{0x38,0xf8,RD }}},
{22,'-','E','D','D',O_SUBS|O_BYTE,"subs.b",2,{IMM8,RD},3,	{{0x04,0xff,0 },{0x00,0x00,IMM8 },{0x38,0xf8,RD }}},
{22,'-','E','D','D',O_SUBS|O_BYTE,"subs.b",2,{RNIND_D8,RD},3,	{{0xe0,0xf8,RN },{0x00,0x00,DISP8 },{0x38,0xf8,RD }}},
{22,'-','E','D','D',O_SUBS|O_BYTE,"subs.b",2,{ABS16,RD},4,	{{0x15,0xff,0 },{0x00,0x00,ABS16 },{0x00,0x00,0 },{0x38,0xf8,RD }}},
{22,'-','E','D','D',O_SUBS|O_BYTE,"subs.b",2,{RNIND_D16,RD},4,	{{0xf0,0xf8,RN },{0x00,0x00,DISP16 },{0x00,0x00,0 },{0x38,0xf8,RD }}},
{23,'-','E','D','D',O_SUBS|O_UNSZ,"subs",2,{RN,RD},2,	{{0xa8,0xf8,RN },{0x38,0xf8,RD }}},
{23,'-','E','D','D',O_SUBS|O_UNSZ,"subs",2,{RNDEC,RD},2,	{{0xb8,0xf8,RN },{0x38,0xf8,RD }}},
{23,'-','E','D','D',O_SUBS|O_UNSZ,"subs",2,{RNIND,RD},2,	{{0xd8,0xf8,RN },{0x38,0xf8,RD }}},
{23,'-','E','D','D',O_SUBS|O_UNSZ,"subs",2,{RNINC,RD},2,	{{0xc8,0xf8,RN },{0x38,0xf8,RD }}},
{23,'-','E','D','D',O_SUBS|O_UNSZ,"subs",2,{ABS8,RD},3,	{{0x0d,0xff,0 },{0x00,0x00,ABS8 },{0x38,0xf8,RD }}},
{23,'-','E','D','D',O_SUBS|O_UNSZ,"subs",2,{RNIND_D8,RD},3,	{{0xe8,0xf8,RN },{0x00,0x00,DISP8 },{0x38,0xf8,RD }}},
{23,'-','E','D','D',O_SUBS|O_UNSZ,"subs",2,{ABS16,RD},4,	{{0x1d,0xff,0 },{0x00,0x00,ABS16 },{0x00,0x00,0 },{0x38,0xf8,RD }}},
{23,'-','E','D','D',O_SUBS|O_UNSZ,"subs",2,{IMM16,RD},4,	{{0x0c,0xff,0 },{0x00,0x00,IMM16 },{0x00,0x00,0 },{0x38,0xf8,RD }}},
{23,'-','E','D','D',O_SUBS|O_UNSZ,"subs",2,{RNIND_D16,RD},4,	{{0xf8,0xf8,RN },{0x00,0x00,DISP16 },{0x00,0x00,0 },{0x38,0xf8,RD }}},
{24,'a','E','D','D',O_SUB|O_WORD,"sub.w",2,{RN,RD},2,	{{0xa8,0xf8,RN },{0x30,0xf8,RD }}},
{24,'a','E','D','D',O_SUB|O_WORD,"sub.w",2,{RNINC,RD},2,	{{0xc8,0xf8,RN },{0x30,0xf8,RD }}},
{24,'a','E','D','D',O_SUB|O_WORD,"sub.w",2,{RNIND,RD},2,	{{0xd8,0xf8,RN },{0x30,0xf8,RD }}},
{24,'a','E','D','D',O_SUB|O_WORD,"sub.w",2,{RNDEC,RD},2,	{{0xb8,0xf8,RN },{0x30,0xf8,RD }}},
{24,'a','E','D','D',O_SUB|O_WORD,"sub.w",2,{ABS8,RD},3,	{{0x0d,0xff,0 },{0x00,0x00,ABS8 },{0x30,0xf8,RD }}},
{24,'a','E','D','D',O_SUB|O_WORD,"sub.w",2,{RNIND_D8,RD},3,	{{0xe8,0xf8,RN },{0x00,0x00,DISP8 },{0x30,0xf8,RD }}},
{24,'a','E','D','D',O_SUB|O_WORD,"sub.w",2,{ABS16,RD},4,	{{0x1d,0xff,0 },{0x00,0x00,ABS16 },{0x00,0x00,0 },{0x30,0xf8,RD }}},
{24,'a','E','D','D',O_SUB|O_WORD,"sub.w",2,{IMM16,RD},4,	{{0x0c,0xff,0 },{0x00,0x00,IMM16 },{0x00,0x00,0 },{0x30,0xf8,RD }}},
{24,'a','E','D','D',O_SUB|O_WORD,"sub.w",2,{RNIND_D16,RD},4,	{{0xf8,0xf8,RN },{0x00,0x00,DISP16 },{0x00,0x00,0 },{0x30,0xf8,RD }}},
{25,'a','E','D','D',O_SUB|O_BYTE,"sub.b",2,{RN,RD},2,	{{0xa0,0xf8,RN },{0x30,0xf8,RD }}},
{25,'a','E','D','D',O_SUB|O_BYTE,"sub.b",2,{RNIND,RD},2,	{{0xd0,0xf8,RN },{0x30,0xf8,RD }}},
{25,'a','E','D','D',O_SUB|O_BYTE,"sub.b",2,{RNDEC,RD},2,	{{0xb0,0xf8,RN },{0x30,0xf8,RD }}},
{25,'a','E','D','D',O_SUB|O_BYTE,"sub.b",2,{RNINC,RD},2,	{{0xc0,0xf8,RN },{0x30,0xf8,RD }}},
{25,'a','E','D','D',O_SUB|O_BYTE,"sub.b",2,{ABS8,RD},3,	{{0x05,0xff,0 },{0x00,0x00,ABS8 },{0x30,0xf8,RD }}},
{25,'a','E','D','D',O_SUB|O_BYTE,"sub.b",2,{IMM8,RD},3,	{{0x04,0xff,0 },{0x00,0x00,IMM8 },{0x30,0xf8,RD }}},
{25,'a','E','D','D',O_SUB|O_BYTE,"sub.b",2,{RNIND_D8,RD},3,	{{0xe0,0xf8,RN },{0x00,0x00,DISP8 },{0x30,0xf8,RD }}},
{25,'a','E','D','D',O_SUB|O_BYTE,"sub.b",2,{ABS16,RD},4,	{{0x15,0xff,0 },{0x00,0x00,ABS16 },{0x00,0x00,0 },{0x30,0xf8,RD }}},
{25,'a','E','D','D',O_SUB|O_BYTE,"sub.b",2,{RNIND_D16,RD},4,	{{0xf0,0xf8,RN },{0x00,0x00,DISP16 },{0x00,0x00,0 },{0x30,0xf8,RD }}},
{26,'a','E','D','D',O_SUB|O_UNSZ,"sub",2,{RN,RD},2,	{{0xa8,0xf8,RN },{0x30,0xf8,RD }}},
{26,'a','E','D','D',O_SUB|O_UNSZ,"sub",2,{RNIND,RD},2,	{{0xd8,0xf8,RN },{0x30,0xf8,RD }}},
{26,'a','E','D','D',O_SUB|O_UNSZ,"sub",2,{RNINC,RD},2,	{{0xc8,0xf8,RN },{0x30,0xf8,RD }}},
{26,'a','E','D','D',O_SUB|O_UNSZ,"sub",2,{RNDEC,RD},2,	{{0xb8,0xf8,RN },{0x30,0xf8,RD }}},
{26,'a','E','D','D',O_SUB|O_UNSZ,"sub",2,{ABS8,RD},3,	{{0x0d,0xff,0 },{0x00,0x00,ABS8 },{0x30,0xf8,RD }}},
{26,'a','E','D','D',O_SUB|O_UNSZ,"sub",2,{RNIND_D8,RD},3,	{{0xe8,0xf8,RN },{0x00,0x00,DISP8 },{0x30,0xf8,RD }}},
{26,'a','E','D','D',O_SUB|O_UNSZ,"sub",2,{IMM16,RD},4,	{{0x0c,0xff,0 },{0x00,0x00,IMM16 },{0x00,0x00,0 },{0x30,0xf8,RD }}},
{26,'a','E','D','D',O_SUB|O_UNSZ,"sub",2,{ABS16,RD},4,	{{0x1d,0xff,0 },{0x00,0x00,ABS16 },{0x00,0x00,0 },{0x30,0xf8,RD }}},
{26,'a','E','D','D',O_SUB|O_UNSZ,"sub",2,{RNIND_D16,RD},4,	{{0xf8,0xf8,RN },{0x00,0x00,DISP16 },{0x00,0x00,0 },{0x30,0xf8,RD }}},
{27,'-','I','!','E',O_STM|O_UNSZ,"stm",2,{RLIST,SPDEC},2,	{{0x12,0xff,0 },{0x00,0x00,RLIST }}},
{28,'s','C','!','E',O_STC|O_WORD,"stc.w",2,{CRW,RN},2,		{{0xa8,0xf8,RN },{0x98,0xf8,CRW }}},
{28,'s','C','!','E',O_STC|O_WORD,"stc.w",2,{CRW,RNDEC},2,	{{0xb8,0xf8,RN },{0x98,0xf8,CRW }}},
{28,'s','C','!','E',O_STC|O_WORD,"stc.w",2,{CRW,RNINC},2,	{{0xc8,0xf8,RN },{0x98,0xf8,CRW }}},
{28,'s','C','!','E',O_STC|O_WORD,"stc.w",2,{CRW,RNIND},2,	{{0xd8,0xf8,RN },{0x98,0xf8,CRW }}},
{28,'s','C','!','E',O_STC|O_WORD,"stc.w",2,{CRW,ABS8},3,	{{0x0d,0xff,0 },{0x00,0x00,ABS8 },{0x98,0xf8,CRW }}},
{28,'s','C','!','E',O_STC|O_WORD,"stc.w",2,{CRW,RNIND_D8},3,	{{0xe8,0xf8,RN },{0x00,0x00,DISP8 },{0x98,0xf8,CRW }}},
{28,'s','C','!','E',O_STC|O_WORD,"stc.w",2,{CRW,ABS16},4,	{{0x1d,0xff,0 },{0x00,0x00,ABS16 },{0x00,0x00,0 },{0x98,0xf8,CRW }}},
{28,'s','C','!','E',O_STC|O_WORD,"stc.w",2,{CRW,RNIND_D16},4,	{{0xf8,0xf8,RN },{0x00,0x00,DISP16 },{0x00,0x00,0 },{0x98,0xf8,CRW }}},
{29,'s','C','!','E',O_STC|O_BYTE,"stc.b",2,{CRB,RN},2,		{{0xa0,0xf8,RN },{0x98,0xf8,CRB }}},
{29,'s','C','!','E',O_STC|O_BYTE,"stc.b",2,{CRB,RNDEC},2,	{{0xb0,0xf8,RN },{0x98,0xf8,CRB }}},
{29,'s','C','!','E',O_STC|O_BYTE,"stc.b",2,{CRB,RNINC},2,	{{0xc0,0xf8,RN },{0x98,0xf8,CRB }}},
{29,'s','C','!','E',O_STC|O_BYTE,"stc.b",2,{CRB,RNIND},2,	{{0xd0,0xf8,RN },{0x98,0xf8,CRB }}},
{29,'s','C','!','E',O_STC|O_BYTE,"stc.b",2,{CRB,ABS8},3,	{{0x05,0xff,0 },{0x00,0x00,ABS8 },{0x98,0xf8,CRB }}},
{29,'s','C','!','E',O_STC|O_BYTE,"stc.b",2,{CRB,RNIND_D8},3,	{{0xe0,0xf8,RN },{0x00,0x00,DISP8 },{0x98,0xf8,CRB }}},
{29,'s','C','!','E',O_STC|O_BYTE,"stc.b",2,{CRB,ABS16},4,	{{0x15,0xff,0 },{0x00,0x00,ABS16 },{0x00,0x00,0 },{0x98,0xf8,CRB }}},
{29,'s','C','!','E',O_STC|O_BYTE,"stc.b",2,{CRB,RNIND_D16},4,	{{0xf0,0xf8,RN },{0x00,0x00,DISP16 },{0x00,0x00,0 },{0x98,0xf8,CRB }}},
{30,'s','C','!','E',O_STC|O_UNSZ,"stc",2,{CRW,RN},2,	{{0xa8,0xf8,RN },{0x98,0xf8,CRW }}},
{30,'s','C','!','E',O_STC|O_UNSZ,"stc",2,{CRB,RN},2,	{{0xa0,0xf8,RN },{0x98,0xf8,CRB }}},
{30,'s','C','!','E',O_STC|O_UNSZ,"stc",2,{CRB,RNDEC},2,	{{0xb0,0xf8,RN },{0x98,0xf8,CRB }}},
{30,'s','C','!','E',O_STC|O_UNSZ,"stc",2,{CRW,RNIND},2,	{{0xd8,0xf8,RN },{0x98,0xf8,CRW }}},
{30,'s','C','!','E',O_STC|O_UNSZ,"stc",2,{CRW,RNINC},2,	{{0xc8,0xf8,RN },{0x98,0xf8,CRW }}},
{30,'s','C','!','E',O_STC|O_UNSZ,"stc",2,{CRW,RNDEC},2,	{{0xb8,0xf8,RN },{0x98,0xf8,CRW }}},
{30,'s','C','!','E',O_STC|O_UNSZ,"stc",2,{CRB,RNIND},2,	{{0xd0,0xf8,RN },{0x98,0xf8,CRB }}},
{30,'s','C','!','E',O_STC|O_UNSZ,"stc",2,{CRB,RNINC},2,	{{0xc0,0xf8,RN },{0x98,0xf8,CRB }}},
{30,'s','C','!','E',O_STC|O_UNSZ,"stc",2,{CRW,RNIND_D8},3,	{{0xe8,0xf8,RN },{0x00,0x00,DISP8 },{0x98,0xf8,CRW }}},
{30,'s','C','!','E',O_STC|O_UNSZ,"stc",2,{CRB,ABS8},3,	{{0x05,0xff,0 },{0x00,0x00,ABS8 },{0x98,0xf8,CRB }}},
{30,'s','C','!','E',O_STC|O_UNSZ,"stc",2,{CRB,RNIND_D8},3,	{{0xe0,0xf8,RN },{0x00,0x00,DISP8 },{0x98,0xf8,CRB }}},
{30,'s','C','!','E',O_STC|O_UNSZ,"stc",2,{CRW,ABS8},3,	{{0x0d,0xff,0 },{0x00,0x00,ABS8 },{0x98,0xf8,CRW }}},
{30,'s','C','!','E',O_STC|O_UNSZ,"stc",2,{CRW,RNIND_D16},4,	{{0xf8,0xf8,RN },{0x00,0x00,DISP16 },{0x00,0x00,0 },{0x98,0xf8,CRW }}},
{30,'s','C','!','E',O_STC|O_UNSZ,"stc",2,{CRW,ABS16},4,	{{0x1d,0xff,0 },{0x00,0x00,ABS16 },{0x00,0x00,0 },{0x98,0xf8,CRW }}},
{30,'s','C','!','E',O_STC|O_UNSZ,"stc",2,{CRB,ABS16},4,	{{0x15,0xff,0 },{0x00,0x00,ABS16 },{0x00,0x00,0 },{0x98,0xf8,CRB }}},
{30,'s','C','!','E',O_STC|O_UNSZ,"stc",2,{CRB,RNIND_D16},4,	{{0xf0,0xf8,RN },{0x00,0x00,DISP16 },{0x00,0x00,0 },{0x98,0xf8,CRB }}},
{31,'-','!','!','!',O_SLEEP|O_UNSZ,"sleep",0,{0,0},1,	{{0x1a,0xff,0 }}},
{32,'h','E','!','E',O_SHLR|O_WORD,"shlr.w",1,{RN,0},2,	{{0xa8,0xf8,RN },{0x1b,0xff,0 }}},
{32,'h','E','!','E',O_SHLR|O_WORD,"shlr.w",1,{RNIND,0},2,	{{0xd8,0xf8,RN },{0x1b,0xff,0 }}},
{32,'h','E','!','E',O_SHLR|O_WORD,"shlr.w",1,{RNDEC,0},2,	{{0xb8,0xf8,RN },{0x1b,0xff,0 }}},
{32,'h','E','!','E',O_SHLR|O_WORD,"shlr.w",1,{RNINC,0},2,	{{0xc8,0xf8,RN },{0x1b,0xff,0 }}},
{32,'h','E','!','E',O_SHLR|O_WORD,"shlr.w",1,{ABS8,0},3,	{{0x0d,0xff,0 },{0x00,0x00,ABS8 },{0x1b,0xff,0 }}},
{32,'h','E','!','E',O_SHLR|O_WORD,"shlr.w",1,{RNIND_D8,0},3,	{{0xe8,0xf8,RN },{0x00,0x00,DISP8 },{0x1b,0xff,0 }}},
{32,'h','E','!','E',O_SHLR|O_WORD,"shlr.w",1,{ABS16,0},4,	{{0x1d,0xff,0 },{0x00,0x00,ABS16 },{0x00,0x00,0 },{0x1b,0xff,0 }}},
{32,'h','E','!','E',O_SHLR|O_WORD,"shlr.w",1,{IMM16,0},4,	{{0x0c,0xff,0 },{0x00,0x00,IMM16 },{0x00,0x00,0 },{0x1b,0xff,0 }}},
{32,'h','E','!','E',O_SHLR|O_WORD,"shlr.w",1,{RNIND_D16,0},4,	{{0xf8,0xf8,RN },{0x00,0x00,DISP16 },{0x00,0x00,0 },{0x1b,0xff,0 }}},
{33,'h','E','!','E',O_SHLR|O_BYTE,"shlr.b",1,{RN,0},2,	{{0xa0,0xf8,RN },{0x1b,0xff,0 }}},
{33,'h','E','!','E',O_SHLR|O_BYTE,"shlr.b",1,{RNINC,0},2,	{{0xc0,0xf8,RN },{0x1b,0xff,0 }}},
{33,'h','E','!','E',O_SHLR|O_BYTE,"shlr.b",1,{RNDEC,0},2,	{{0xb0,0xf8,RN },{0x1b,0xff,0 }}},
{33,'h','E','!','E',O_SHLR|O_BYTE,"shlr.b",1,{RNIND,0},2,	{{0xd0,0xf8,RN },{0x1b,0xff,0 }}},
{33,'h','E','!','E',O_SHLR|O_BYTE,"shlr.b",1,{ABS8,0},3,	{{0x05,0xff,0 },{0x00,0x00,ABS8 },{0x1b,0xff,0 }}},
{33,'h','E','!','E',O_SHLR|O_BYTE,"shlr.b",1,{IMM8,0},3,	{{0x04,0xff,0 },{0x00,0x00,IMM8 },{0x1b,0xff,0 }}},
{33,'h','E','!','E',O_SHLR|O_BYTE,"shlr.b",1,{RNIND_D8,0},3,	{{0xe0,0xf8,RN },{0x00,0x00,DISP8 },{0x1b,0xff,0 }}},
{33,'h','E','!','E',O_SHLR|O_BYTE,"shlr.b",1,{ABS16,0},4,	{{0x15,0xff,0 },{0x00,0x00,ABS16 },{0x00,0x00,0 },{0x1b,0xff,0 }}},
{33,'h','E','!','E',O_SHLR|O_BYTE,"shlr.b",1,{RNIND_D16,0},4,	{{0xf0,0xf8,RN },{0x00,0x00,DISP16 },{0x00,0x00,0 },{0x1b,0xff,0 }}},
{34,'h','E','!','E',O_SHLR|O_UNSZ,"shlr",1,{RN,0},2,	{{0xa8,0xf8,RN },{0x1b,0xff,0 }}},
{34,'h','E','!','E',O_SHLR|O_UNSZ,"shlr",1,{RNIND,0},2,	{{0xd8,0xf8,RN },{0x1b,0xff,0 }}},
{34,'h','E','!','E',O_SHLR|O_UNSZ,"shlr",1,{RNDEC,0},2,	{{0xb8,0xf8,RN },{0x1b,0xff,0 }}},
{34,'h','E','!','E',O_SHLR|O_UNSZ,"shlr",1,{RNINC,0},2,	{{0xc8,0xf8,RN },{0x1b,0xff,0 }}},
{34,'h','E','!','E',O_SHLR|O_UNSZ,"shlr",1,{ABS8,0},3,	{{0x0d,0xff,0 },{0x00,0x00,ABS8 },{0x1b,0xff,0 }}},
{34,'h','E','!','E',O_SHLR|O_UNSZ,"shlr",1,{RNIND_D8,0},3,	{{0xe8,0xf8,RN },{0x00,0x00,DISP8 },{0x1b,0xff,0 }}},
{34,'h','E','!','E',O_SHLR|O_UNSZ,"shlr",1,{ABS16,0},4,	{{0x1d,0xff,0 },{0x00,0x00,ABS16 },{0x00,0x00,0 },{0x1b,0xff,0 }}},
{34,'h','E','!','E',O_SHLR|O_UNSZ,"shlr",1,{IMM16,0},4,	{{0x0c,0xff,0 },{0x00,0x00,IMM16 },{0x00,0x00,0 },{0x1b,0xff,0 }}},
{34,'h','E','!','E',O_SHLR|O_UNSZ,"shlr",1,{RNIND_D16,0},4,	{{0xf8,0xf8,RN },{0x00,0x00,DISP16 },{0x00,0x00,0 },{0x1b,0xff,0 }}},
{35,'h','E','!','E',O_SHLL|O_WORD,"shll.w",1,{RN,0},2,	{{0xa8,0xf8,RN },{0x1a,0xff,0 }}},
{35,'h','E','!','E',O_SHLL|O_WORD,"shll.w",1,{RNIND,0},2,	{{0xd8,0xf8,RN },{0x1a,0xff,0 }}},
{35,'h','E','!','E',O_SHLL|O_WORD,"shll.w",1,{RNINC,0},2,	{{0xc8,0xf8,RN },{0x1a,0xff,0 }}},
{35,'h','E','!','E',O_SHLL|O_WORD,"shll.w",1,{RNDEC,0},2,	{{0xb8,0xf8,RN },{0x1a,0xff,0 }}},
{35,'h','E','!','E',O_SHLL|O_WORD,"shll.w",1,{ABS8,0},3,	{{0x0d,0xff,0 },{0x00,0x00,ABS8 },{0x1a,0xff,0 }}},
{35,'h','E','!','E',O_SHLL|O_WORD,"shll.w",1,{RNIND_D8,0},3,	{{0xe8,0xf8,RN },{0x00,0x00,DISP8 },{0x1a,0xff,0 }}},
{35,'h','E','!','E',O_SHLL|O_WORD,"shll.w",1,{IMM16,0},4,	{{0x0c,0xff,0 },{0x00,0x00,IMM16 },{0x00,0x00,0 },{0x1a,0xff,0 }}},
{35,'h','E','!','E',O_SHLL|O_WORD,"shll.w",1,{ABS16,0},4,	{{0x1d,0xff,0 },{0x00,0x00,ABS16 },{0x00,0x00,0 },{0x1a,0xff,0 }}},
{35,'h','E','!','E',O_SHLL|O_WORD,"shll.w",1,{RNIND_D16,0},4,	{{0xf8,0xf8,RN },{0x00,0x00,DISP16 },{0x00,0x00,0 },{0x1a,0xff,0 }}},
{36,'h','E','!','E',O_SHLL|O_BYTE,"shll.b",1,{RN,0},2,	{{0xa0,0xf8,RN },{0x1a,0xff,0 }}},
{36,'h','E','!','E',O_SHLL|O_BYTE,"shll.b",1,{RNIND,0},2,	{{0xd0,0xf8,RN },{0x1a,0xff,0 }}},
{36,'h','E','!','E',O_SHLL|O_BYTE,"shll.b",1,{RNDEC,0},2,	{{0xb0,0xf8,RN },{0x1a,0xff,0 }}},
{36,'h','E','!','E',O_SHLL|O_BYTE,"shll.b",1,{RNINC,0},2,	{{0xc0,0xf8,RN },{0x1a,0xff,0 }}},
{36,'h','E','!','E',O_SHLL|O_BYTE,"shll.b",1,{RNIND_D8,0},3,	{{0xe0,0xf8,RN },{0x00,0x00,DISP8 },{0x1a,0xff,0 }}},
{36,'h','E','!','E',O_SHLL|O_BYTE,"shll.b",1,{ABS8,0},3,	{{0x05,0xff,0 },{0x00,0x00,ABS8 },{0x1a,0xff,0 }}},
{36,'h','E','!','E',O_SHLL|O_BYTE,"shll.b",1,{IMM8,0},3,	{{0x04,0xff,0 },{0x00,0x00,IMM8 },{0x1a,0xff,0 }}},
{36,'h','E','!','E',O_SHLL|O_BYTE,"shll.b",1,{ABS16,0},4,	{{0x15,0xff,0 },{0x00,0x00,ABS16 },{0x00,0x00,0 },{0x1a,0xff,0 }}},
{36,'h','E','!','E',O_SHLL|O_BYTE,"shll.b",1,{RNIND_D16,0},4,	{{0xf0,0xf8,RN },{0x00,0x00,DISP16 },{0x00,0x00,0 },{0x1a,0xff,0 }}},
{37,'h','E','!','E',O_SHLL|O_UNSZ,"shll",1,{RN,0},2,	{{0xa8,0xf8,RN },{0x1a,0xff,0 }}},
{37,'h','E','!','E',O_SHLL|O_UNSZ,"shll",1,{RNDEC,0},2,	{{0xb8,0xf8,RN },{0x1a,0xff,0 }}},
{37,'h','E','!','E',O_SHLL|O_UNSZ,"shll",1,{RNIND,0},2,	{{0xd8,0xf8,RN },{0x1a,0xff,0 }}},
{37,'h','E','!','E',O_SHLL|O_UNSZ,"shll",1,{RNINC,0},2,	{{0xc8,0xf8,RN },{0x1a,0xff,0 }}},
{37,'h','E','!','E',O_SHLL|O_UNSZ,"shll",1,{ABS8,0},3,	{{0x0d,0xff,0 },{0x00,0x00,ABS8 },{0x1a,0xff,0 }}},
{37,'h','E','!','E',O_SHLL|O_UNSZ,"shll",1,{RNIND_D8,0},3,	{{0xe8,0xf8,RN },{0x00,0x00,DISP8 },{0x1a,0xff,0 }}},
{37,'h','E','!','E',O_SHLL|O_UNSZ,"shll",1,{IMM16,0},4,	{{0x0c,0xff,0 },{0x00,0x00,IMM16 },{0x00,0x00,0 },{0x1a,0xff,0 }}},
{37,'h','E','!','E',O_SHLL|O_UNSZ,"shll",1,{ABS16,0},4,	{{0x1d,0xff,0 },{0x00,0x00,ABS16 },{0x00,0x00,0 },{0x1a,0xff,0 }}},
{37,'h','E','!','E',O_SHLL|O_UNSZ,"shll",1,{RNIND_D16,0},4,	{{0xf8,0xf8,RN },{0x00,0x00,DISP16 },{0x00,0x00,0 },{0x1a,0xff,0 }}},
{38,'h','E','!','E',O_SHAR|O_WORD,"shar.w",1,{RN,0},2,	{{0xa8,0xf8,RN },{0x19,0xff,0 }}},
{38,'h','E','!','E',O_SHAR|O_WORD,"shar.w",1,{RNDEC,0},2,	{{0xb8,0xf8,RN },{0x19,0xff,0 }}},
{38,'h','E','!','E',O_SHAR|O_WORD,"shar.w",1,{RNIND,0},2,	{{0xd8,0xf8,RN },{0x19,0xff,0 }}},
{38,'h','E','!','E',O_SHAR|O_WORD,"shar.w",1,{RNINC,0},2,	{{0xc8,0xf8,RN },{0x19,0xff,0 }}},
{38,'h','E','!','E',O_SHAR|O_WORD,"shar.w",1,{ABS8,0},3,	{{0x0d,0xff,0 },{0x00,0x00,ABS8 },{0x19,0xff,0 }}},
{38,'h','E','!','E',O_SHAR|O_WORD,"shar.w",1,{RNIND_D8,0},3,	{{0xe8,0xf8,RN },{0x00,0x00,DISP8 },{0x19,0xff,0 }}},
{38,'h','E','!','E',O_SHAR|O_WORD,"shar.w",1,{ABS16,0},4,	{{0x1d,0xff,0 },{0x00,0x00,ABS16 },{0x00,0x00,0 },{0x19,0xff,0 }}},
{38,'h','E','!','E',O_SHAR|O_WORD,"shar.w",1,{IMM16,0},4,	{{0x0c,0xff,0 },{0x00,0x00,IMM16 },{0x00,0x00,0 },{0x19,0xff,0 }}},
{38,'h','E','!','E',O_SHAR|O_WORD,"shar.w",1,{RNIND_D16,0},4,	{{0xf8,0xf8,RN },{0x00,0x00,DISP16 },{0x00,0x00,0 },{0x19,0xff,0 }}},
{39,'h','E','!','E',O_SHAR|O_BYTE,"shar.b",1,{RN,0},2,	{{0xa0,0xf8,RN },{0x19,0xff,0 }}},
{39,'h','E','!','E',O_SHAR|O_BYTE,"shar.b",1,{RNDEC,0},2,	{{0xb0,0xf8,RN },{0x19,0xff,0 }}},
{39,'h','E','!','E',O_SHAR|O_BYTE,"shar.b",1,{RNIND,0},2,	{{0xd0,0xf8,RN },{0x19,0xff,0 }}},
{39,'h','E','!','E',O_SHAR|O_BYTE,"shar.b",1,{RNINC,0},2,	{{0xc0,0xf8,RN },{0x19,0xff,0 }}},
{39,'h','E','!','E',O_SHAR|O_BYTE,"shar.b",1,{IMM8,0},3,	{{0x04,0xff,0 },{0x00,0x00,IMM8 },{0x19,0xff,0 }}},
{39,'h','E','!','E',O_SHAR|O_BYTE,"shar.b",1,{RNIND_D8,0},3,	{{0xe0,0xf8,RN },{0x00,0x00,DISP8 },{0x19,0xff,0 }}},
{39,'h','E','!','E',O_SHAR|O_BYTE,"shar.b",1,{ABS8,0},3,	{{0x05,0xff,0 },{0x00,0x00,ABS8 },{0x19,0xff,0 }}},
{39,'h','E','!','E',O_SHAR|O_BYTE,"shar.b",1,{ABS16,0},4,	{{0x15,0xff,0 },{0x00,0x00,ABS16 },{0x00,0x00,0 },{0x19,0xff,0 }}},
{39,'h','E','!','E',O_SHAR|O_BYTE,"shar.b",1,{RNIND_D16,0},4,	{{0xf0,0xf8,RN },{0x00,0x00,DISP16 },{0x00,0x00,0 },{0x19,0xff,0 }}},
{40,'h','E','!','E',O_SHAR|O_UNSZ,"shar",1,{RN,0},2,	{{0xa8,0xf8,RN },{0x19,0xff,0 }}},
{40,'h','E','!','E',O_SHAR|O_UNSZ,"shar",1,{RNDEC,0},2,	{{0xb8,0xf8,RN },{0x19,0xff,0 }}},
{40,'h','E','!','E',O_SHAR|O_UNSZ,"shar",1,{RNIND,0},2,	{{0xd8,0xf8,RN },{0x19,0xff,0 }}},
{40,'h','E','!','E',O_SHAR|O_UNSZ,"shar",1,{RNINC,0},2,	{{0xc8,0xf8,RN },{0x19,0xff,0 }}},
{40,'h','E','!','E',O_SHAR|O_UNSZ,"shar",1,{ABS8,0},3,	{{0x0d,0xff,0 },{0x00,0x00,ABS8 },{0x19,0xff,0 }}},
{40,'h','E','!','E',O_SHAR|O_UNSZ,"shar",1,{RNIND_D8,0},3,	{{0xe8,0xf8,RN },{0x00,0x00,DISP8 },{0x19,0xff,0 }}},
{40,'h','E','!','E',O_SHAR|O_UNSZ,"shar",1,{IMM16,0},4,	{{0x0c,0xff,0 },{0x00,0x00,IMM16 },{0x00,0x00,0 },{0x19,0xff,0 }}},
{40,'h','E','!','E',O_SHAR|O_UNSZ,"shar",1,{ABS16,0},4,	{{0x1d,0xff,0 },{0x00,0x00,ABS16 },{0x00,0x00,0 },{0x19,0xff,0 }}},
{40,'h','E','!','E',O_SHAR|O_UNSZ,"shar",1,{RNIND_D16,0},4,	{{0xf8,0xf8,RN },{0x00,0x00,DISP16 },{0x00,0x00,0 },{0x19,0xff,0 }}},
{41,'h','E','!','E',O_SHAL|O_WORD,"shal.w",1,{RN,0},2,	{{0xa8,0xf8,RN },{0x18,0xff,0 }}},
{41,'h','E','!','E',O_SHAL|O_WORD,"shal.w",1,{RNIND,0},2,	{{0xd8,0xf8,RN },{0x18,0xff,0 }}},
{41,'h','E','!','E',O_SHAL|O_WORD,"shal.w",1,{RNINC,0},2,	{{0xc8,0xf8,RN },{0x18,0xff,0 }}},
{41,'h','E','!','E',O_SHAL|O_WORD,"shal.w",1,{RNDEC,0},2,	{{0xb8,0xf8,RN },{0x18,0xff,0 }}},
{41,'h','E','!','E',O_SHAL|O_WORD,"shal.w",1,{ABS8,0},3,	{{0x0d,0xff,0 },{0x00,0x00,ABS8 },{0x18,0xff,0 }}},
{41,'h','E','!','E',O_SHAL|O_WORD,"shal.w",1,{RNIND_D8,0},3,	{{0xe8,0xf8,RN },{0x00,0x00,DISP8 },{0x18,0xff,0 }}},
{41,'h','E','!','E',O_SHAL|O_WORD,"shal.w",1,{ABS16,0},4,	{{0x1d,0xff,0 },{0x00,0x00,ABS16 },{0x00,0x00,0 },{0x18,0xff,0 }}},
{41,'h','E','!','E',O_SHAL|O_WORD,"shal.w",1,{IMM16,0},4,	{{0x0c,0xff,0 },{0x00,0x00,IMM16 },{0x00,0x00,0 },{0x18,0xff,0 }}},
{41,'h','E','!','E',O_SHAL|O_WORD,"shal.w",1,{RNIND_D16,0},4,	{{0xf8,0xf8,RN },{0x00,0x00,DISP16 },{0x00,0x00,0 },{0x18,0xff,0 }}},
{42,'h','E','!','E',O_SHAL|O_BYTE,"shal.b",1,{RN,0},2,	{{0xa0,0xf8,RN },{0x18,0xff,0 }}},
{42,'h','E','!','E',O_SHAL|O_BYTE,"shal.b",1,{RNIND,0},2,	{{0xd0,0xf8,RN },{0x18,0xff,0 }}},
{42,'h','E','!','E',O_SHAL|O_BYTE,"shal.b",1,{RNDEC,0},2,	{{0xb0,0xf8,RN },{0x18,0xff,0 }}},
{42,'h','E','!','E',O_SHAL|O_BYTE,"shal.b",1,{RNINC,0},2,	{{0xc0,0xf8,RN },{0x18,0xff,0 }}},
{42,'h','E','!','E',O_SHAL|O_BYTE,"shal.b",1,{ABS8,0},3,	{{0x05,0xff,0 },{0x00,0x00,ABS8 },{0x18,0xff,0 }}},
{42,'h','E','!','E',O_SHAL|O_BYTE,"shal.b",1,{IMM8,0},3,	{{0x04,0xff,0 },{0x00,0x00,IMM8 },{0x18,0xff,0 }}},
{42,'h','E','!','E',O_SHAL|O_BYTE,"shal.b",1,{RNIND_D8,0},3,	{{0xe0,0xf8,RN },{0x00,0x00,DISP8 },{0x18,0xff,0 }}},
{42,'h','E','!','E',O_SHAL|O_BYTE,"shal.b",1,{ABS16,0},4,	{{0x15,0xff,0 },{0x00,0x00,ABS16 },{0x00,0x00,0 },{0x18,0xff,0 }}},
{42,'h','E','!','E',O_SHAL|O_BYTE,"shal.b",1,{RNIND_D16,0},4,	{{0xf0,0xf8,RN },{0x00,0x00,DISP16 },{0x00,0x00,0 },{0x18,0xff,0 }}},
{43,'h','E','!','E',O_SHAL|O_UNSZ,"shal",1,{RN,0},2,	{{0xa8,0xf8,RN },{0x18,0xff,0 }}},
{43,'h','E','!','E',O_SHAL|O_UNSZ,"shal",1,{RNIND,0},2,	{{0xd8,0xf8,RN },{0x18,0xff,0 }}},
{43,'h','E','!','E',O_SHAL|O_UNSZ,"shal",1,{RNDEC,0},2,	{{0xb8,0xf8,RN },{0x18,0xff,0 }}},
{43,'h','E','!','E',O_SHAL|O_UNSZ,"shal",1,{RNINC,0},2,	{{0xc8,0xf8,RN },{0x18,0xff,0 }}},
{43,'h','E','!','E',O_SHAL|O_UNSZ,"shal",1,{ABS8,0},3,	{{0x0d,0xff,0 },{0x00,0x00,ABS8 },{0x18,0xff,0 }}},
{43,'h','E','!','E',O_SHAL|O_UNSZ,"shal",1,{RNIND_D8,0},3,	{{0xe8,0xf8,RN },{0x00,0x00,DISP8 },{0x18,0xff,0 }}},
{43,'h','E','!','E',O_SHAL|O_UNSZ,"shal",1,{ABS16,0},4,	{{0x1d,0xff,0 },{0x00,0x00,ABS16 },{0x00,0x00,0 },{0x18,0xff,0 }}},
{43,'h','E','!','E',O_SHAL|O_UNSZ,"shal",1,{IMM16,0},4,	{{0x0c,0xff,0 },{0x00,0x00,IMM16 },{0x00,0x00,0 },{0x18,0xff,0 }}},
{43,'h','E','!','E',O_SHAL|O_UNSZ,"shal",1,{RNIND_D16,0},4,	{{0xf8,0xf8,RN },{0x00,0x00,DISP16 },{0x00,0x00,0 },{0x18,0xff,0 }}},
{44,'-','B','S','S',O_SCB_NE|O_UNSZ,"scb/ne",2,{RS,PCREL8},3,	{{0x06,0xff,0 },{0xb8,0xf8,RS },{0x00,0x00,PCREL8 }}},
{45,'-','B','S','S',O_SCB_F|O_UNSZ,"scb/f",2,{RS,PCREL8},3,	{{0x01,0xff,0 },{0xb8,0xf8,RS },{0x00,0x00,PCREL8 }}},
{46,'-','B','S','S',O_SCB_EQ|O_UNSZ,"scb/eq",2,{RS,PCREL8},3,	{{0x07,0xff,0 },{0xb8,0xf8,RS },{0x00,0x00,PCREL8 }}},
{47,'-','B','!','!',O_RTS|O_UNSZ,"rts",0,{0,0},1,	{{0x19,0xff,0 }}},
{48,'-','B','!','!',O_RTD|O_UNSZ,"rtd",1,{IMM8,0},2,	{{0x14,0xff,0 },{0x00,0x00,IMM8 }}},
{48,'-','B','!','!',O_RTD|O_UNSZ,"rtd",1,{IMM16,0},3,	{{0x14,0xff,0 },{0x00,0x00,IMM16 },{0x00,0x00,0 }}},
{49,'h','E','!','E',O_ROTXR|O_WORD,"rotxr.w",1,{RN,0},2,	{{0xa8,0xf8,RN },{0x1f,0xff,0 }}},
{49,'h','E','!','E',O_ROTXR|O_WORD,"rotxr.w",1,{RNDEC,0},2,	{{0xb8,0xf8,RN },{0x1f,0xff,0 }}},
{49,'h','E','!','E',O_ROTXR|O_WORD,"rotxr.w",1,{RNINC,0},2,	{{0xc8,0xf8,RN },{0x1f,0xff,0 }}},
{49,'h','E','!','E',O_ROTXR|O_WORD,"rotxr.w",1,{RNIND,0},2,	{{0xd8,0xf8,RN },{0x1f,0xff,0 }}},
{49,'h','E','!','E',O_ROTXR|O_WORD,"rotxr.w",1,{ABS8,0},3,	{{0x0d,0xff,0 },{0x00,0x00,ABS8 },{0x1f,0xff,0 }}},
{49,'h','E','!','E',O_ROTXR|O_WORD,"rotxr.w",1,{RNIND_D8,0},3,	{{0xe8,0xf8,RN },{0x00,0x00,DISP8 },{0x1f,0xff,0 }}},
{49,'h','E','!','E',O_ROTXR|O_WORD,"rotxr.w",1,{ABS16,0},4,	{{0x1d,0xff,0 },{0x00,0x00,ABS16 },{0x00,0x00,0 },{0x1f,0xff,0 }}},
{49,'h','E','!','E',O_ROTXR|O_WORD,"rotxr.w",1,{IMM16,0},4,	{{0x0c,0xff,0 },{0x00,0x00,IMM16 },{0x00,0x00,0 },{0x1f,0xff,0 }}},
{49,'h','E','!','E',O_ROTXR|O_WORD,"rotxr.w",1,{RNIND_D16,0},4,	{{0xf8,0xf8,RN },{0x00,0x00,DISP16 },{0x00,0x00,0 },{0x1f,0xff,0 }}},
{50,'h','E','!','E',O_ROTXR|O_BYTE,"rotxr.b",1,{RN,0},2,	{{0xa0,0xf8,RN },{0x1f,0xff,0 }}},
{50,'h','E','!','E',O_ROTXR|O_BYTE,"rotxr.b",1,{RNDEC,0},2,	{{0xb0,0xf8,RN },{0x1f,0xff,0 }}},
{50,'h','E','!','E',O_ROTXR|O_BYTE,"rotxr.b",1,{RNIND,0},2,	{{0xd0,0xf8,RN },{0x1f,0xff,0 }}},
{50,'h','E','!','E',O_ROTXR|O_BYTE,"rotxr.b",1,{RNINC,0},2,	{{0xc0,0xf8,RN },{0x1f,0xff,0 }}},
{50,'h','E','!','E',O_ROTXR|O_BYTE,"rotxr.b",1,{IMM8,0},3,	{{0x04,0xff,0 },{0x00,0x00,IMM8 },{0x1f,0xff,0 }}},
{50,'h','E','!','E',O_ROTXR|O_BYTE,"rotxr.b",1,{ABS8,0},3,	{{0x05,0xff,0 },{0x00,0x00,ABS8 },{0x1f,0xff,0 }}},
{50,'h','E','!','E',O_ROTXR|O_BYTE,"rotxr.b",1,{RNIND_D8,0},3,	{{0xe0,0xf8,RN },{0x00,0x00,DISP8 },{0x1f,0xff,0 }}},
{50,'h','E','!','E',O_ROTXR|O_BYTE,"rotxr.b",1,{ABS16,0},4,	{{0x15,0xff,0 },{0x00,0x00,ABS16 },{0x00,0x00,0 },{0x1f,0xff,0 }}},
{50,'h','E','!','E',O_ROTXR|O_BYTE,"rotxr.b",1,{RNIND_D16,0},4,	{{0xf0,0xf8,RN },{0x00,0x00,DISP16 },{0x00,0x00,0 },{0x1f,0xff,0 }}},
{51,'h','E','!','E',O_ROTXR|O_UNSZ,"rotxr",1,{RN,0},2,	{{0xa8,0xf8,RN },{0x1f,0xff,0 }}},
{51,'h','E','!','E',O_ROTXR|O_UNSZ,"rotxr",1,{RNDEC,0},2,	{{0xb8,0xf8,RN },{0x1f,0xff,0 }}},
{51,'h','E','!','E',O_ROTXR|O_UNSZ,"rotxr",1,{RNINC,0},2,	{{0xc8,0xf8,RN },{0x1f,0xff,0 }}},
{51,'h','E','!','E',O_ROTXR|O_UNSZ,"rotxr",1,{RNIND,0},2,	{{0xd8,0xf8,RN },{0x1f,0xff,0 }}},
{51,'h','E','!','E',O_ROTXR|O_UNSZ,"rotxr",1,{ABS8,0},3,	{{0x0d,0xff,0 },{0x00,0x00,ABS8 },{0x1f,0xff,0 }}},
{51,'h','E','!','E',O_ROTXR|O_UNSZ,"rotxr",1,{RNIND_D8,0},3,	{{0xe8,0xf8,RN },{0x00,0x00,DISP8 },{0x1f,0xff,0 }}},
{51,'h','E','!','E',O_ROTXR|O_UNSZ,"rotxr",1,{IMM16,0},4,	{{0x0c,0xff,0 },{0x00,0x00,IMM16 },{0x00,0x00,0 },{0x1f,0xff,0 }}},
{51,'h','E','!','E',O_ROTXR|O_UNSZ,"rotxr",1,{ABS16,0},4,	{{0x1d,0xff,0 },{0x00,0x00,ABS16 },{0x00,0x00,0 },{0x1f,0xff,0 }}},
{51,'h','E','!','E',O_ROTXR|O_UNSZ,"rotxr",1,{RNIND_D16,0},4,	{{0xf8,0xf8,RN },{0x00,0x00,DISP16 },{0x00,0x00,0 },{0x1f,0xff,0 }}},
{52,'h','E','!','E',O_ROTXL|O_WORD,"rotxl.w",1,{RN,0},2,	{{0xa8,0xf8,RN },{0x1e,0xff,0 }}},
{52,'h','E','!','E',O_ROTXL|O_WORD,"rotxl.w",1,{RNIND,0},2,	{{0xd8,0xf8,RN },{0x1e,0xff,0 }}},
{52,'h','E','!','E',O_ROTXL|O_WORD,"rotxl.w",1,{RNINC,0},2,	{{0xc8,0xf8,RN },{0x1e,0xff,0 }}},
{52,'h','E','!','E',O_ROTXL|O_WORD,"rotxl.w",1,{RNDEC,0},2,	{{0xb8,0xf8,RN },{0x1e,0xff,0 }}},
{52,'h','E','!','E',O_ROTXL|O_WORD,"rotxl.w",1,{ABS8,0},3,	{{0x0d,0xff,0 },{0x00,0x00,ABS8 },{0x1e,0xff,0 }}},
{52,'h','E','!','E',O_ROTXL|O_WORD,"rotxl.w",1,{RNIND_D8,0},3,	{{0xe8,0xf8,RN },{0x00,0x00,DISP8 },{0x1e,0xff,0 }}},
{52,'h','E','!','E',O_ROTXL|O_WORD,"rotxl.w",1,{ABS16,0},4,	{{0x1d,0xff,0 },{0x00,0x00,ABS16 },{0x00,0x00,0 },{0x1e,0xff,0 }}},
{52,'h','E','!','E',O_ROTXL|O_WORD,"rotxl.w",1,{IMM16,0},4,	{{0x0c,0xff,0 },{0x00,0x00,IMM16 },{0x00,0x00,0 },{0x1e,0xff,0 }}},
{52,'h','E','!','E',O_ROTXL|O_WORD,"rotxl.w",1,{RNIND_D16,0},4,	{{0xf8,0xf8,RN },{0x00,0x00,DISP16 },{0x00,0x00,0 },{0x1e,0xff,0 }}},
{53,'h','E','!','E',O_ROTXL|O_BYTE,"rotxl.b",1,{RN,0},2,	{{0xa0,0xf8,RN },{0x1e,0xff,0 }}},
{53,'h','E','!','E',O_ROTXL|O_BYTE,"rotxl.b",1,{RNINC,0},2,	{{0xc0,0xf8,RN },{0x1e,0xff,0 }}},
{53,'h','E','!','E',O_ROTXL|O_BYTE,"rotxl.b",1,{RNDEC,0},2,	{{0xb0,0xf8,RN },{0x1e,0xff,0 }}},
{53,'h','E','!','E',O_ROTXL|O_BYTE,"rotxl.b",1,{RNIND,0},2,	{{0xd0,0xf8,RN },{0x1e,0xff,0 }}},
{53,'h','E','!','E',O_ROTXL|O_BYTE,"rotxl.b",1,{ABS8,0},3,	{{0x05,0xff,0 },{0x00,0x00,ABS8 },{0x1e,0xff,0 }}},
{53,'h','E','!','E',O_ROTXL|O_BYTE,"rotxl.b",1,{IMM8,0},3,	{{0x04,0xff,0 },{0x00,0x00,IMM8 },{0x1e,0xff,0 }}},
{53,'h','E','!','E',O_ROTXL|O_BYTE,"rotxl.b",1,{RNIND_D8,0},3,	{{0xe0,0xf8,RN },{0x00,0x00,DISP8 },{0x1e,0xff,0 }}},
{53,'h','E','!','E',O_ROTXL|O_BYTE,"rotxl.b",1,{ABS16,0},4,	{{0x15,0xff,0 },{0x00,0x00,ABS16 },{0x00,0x00,0 },{0x1e,0xff,0 }}},
{53,'h','E','!','E',O_ROTXL|O_BYTE,"rotxl.b",1,{RNIND_D16,0},4,	{{0xf0,0xf8,RN },{0x00,0x00,DISP16 },{0x00,0x00,0 },{0x1e,0xff,0 }}},
{54,'h','E','!','E',O_ROTXL|O_UNSZ,"rotxl",1,{RN,0},2,	{{0xa8,0xf8,RN },{0x1e,0xff,0 }}},
{54,'h','E','!','E',O_ROTXL|O_UNSZ,"rotxl",1,{RNIND,0},2,	{{0xd8,0xf8,RN },{0x1e,0xff,0 }}},
{54,'h','E','!','E',O_ROTXL|O_UNSZ,"rotxl",1,{RNINC,0},2,	{{0xc8,0xf8,RN },{0x1e,0xff,0 }}},
{54,'h','E','!','E',O_ROTXL|O_UNSZ,"rotxl",1,{RNDEC,0},2,	{{0xb8,0xf8,RN },{0x1e,0xff,0 }}},
{54,'h','E','!','E',O_ROTXL|O_UNSZ,"rotxl",1,{ABS8,0},3,	{{0x0d,0xff,0 },{0x00,0x00,ABS8 },{0x1e,0xff,0 }}},
{54,'h','E','!','E',O_ROTXL|O_UNSZ,"rotxl",1,{RNIND_D8,0},3,	{{0xe8,0xf8,RN },{0x00,0x00,DISP8 },{0x1e,0xff,0 }}},
{54,'h','E','!','E',O_ROTXL|O_UNSZ,"rotxl",1,{ABS16,0},4,	{{0x1d,0xff,0 },{0x00,0x00,ABS16 },{0x00,0x00,0 },{0x1e,0xff,0 }}},
{54,'h','E','!','E',O_ROTXL|O_UNSZ,"rotxl",1,{IMM16,0},4,	{{0x0c,0xff,0 },{0x00,0x00,IMM16 },{0x00,0x00,0 },{0x1e,0xff,0 }}},
{54,'h','E','!','E',O_ROTXL|O_UNSZ,"rotxl",1,{RNIND_D16,0},4,	{{0xf8,0xf8,RN },{0x00,0x00,DISP16 },{0x00,0x00,0 },{0x1e,0xff,0 }}},
{55,'h','E','!','E',O_ROTR|O_WORD,"rotr.w",1,{RN,0},2,	{{0xa8,0xf8,RN },{0x1d,0xff,0 }}},
{55,'h','E','!','E',O_ROTR|O_WORD,"rotr.w",1,{RNIND,0},2,	{{0xd8,0xf8,RN },{0x1d,0xff,0 }}},
{55,'h','E','!','E',O_ROTR|O_WORD,"rotr.w",1,{RNDEC,0},2,	{{0xb8,0xf8,RN },{0x1d,0xff,0 }}},
{55,'h','E','!','E',O_ROTR|O_WORD,"rotr.w",1,{RNINC,0},2,	{{0xc8,0xf8,RN },{0x1d,0xff,0 }}},
{55,'h','E','!','E',O_ROTR|O_WORD,"rotr.w",1,{ABS8,0},3,	{{0x0d,0xff,0 },{0x00,0x00,ABS8 },{0x1d,0xff,0 }}},
{55,'h','E','!','E',O_ROTR|O_WORD,"rotr.w",1,{RNIND_D8,0},3,	{{0xe8,0xf8,RN },{0x00,0x00,DISP8 },{0x1d,0xff,0 }}},
{55,'h','E','!','E',O_ROTR|O_WORD,"rotr.w",1,{ABS16,0},4,	{{0x1d,0xff,0 },{0x00,0x00,ABS16 },{0x00,0x00,0 },{0x1d,0xff,0 }}},
{55,'h','E','!','E',O_ROTR|O_WORD,"rotr.w",1,{IMM16,0},4,	{{0x0c,0xff,0 },{0x00,0x00,IMM16 },{0x00,0x00,0 },{0x1d,0xff,0 }}},
{55,'h','E','!','E',O_ROTR|O_WORD,"rotr.w",1,{RNIND_D16,0},4,	{{0xf8,0xf8,RN },{0x00,0x00,DISP16 },{0x00,0x00,0 },{0x1d,0xff,0 }}},
{56,'h','E','!','E',O_ROTR|O_BYTE,"rotr.b",1,{RN,0},2,	{{0xa0,0xf8,RN },{0x1d,0xff,0 }}},
{56,'h','E','!','E',O_ROTR|O_BYTE,"rotr.b",1,{RNIND,0},2,	{{0xd0,0xf8,RN },{0x1d,0xff,0 }}},
{56,'h','E','!','E',O_ROTR|O_BYTE,"rotr.b",1,{RNDEC,0},2,	{{0xb0,0xf8,RN },{0x1d,0xff,0 }}},
{56,'h','E','!','E',O_ROTR|O_BYTE,"rotr.b",1,{RNINC,0},2,	{{0xc0,0xf8,RN },{0x1d,0xff,0 }}},
{56,'h','E','!','E',O_ROTR|O_BYTE,"rotr.b",1,{IMM8,0},3,	{{0x04,0xff,0 },{0x00,0x00,IMM8 },{0x1d,0xff,0 }}},
{56,'h','E','!','E',O_ROTR|O_BYTE,"rotr.b",1,{ABS8,0},3,	{{0x05,0xff,0 },{0x00,0x00,ABS8 },{0x1d,0xff,0 }}},
{56,'h','E','!','E',O_ROTR|O_BYTE,"rotr.b",1,{RNIND_D8,0},3,	{{0xe0,0xf8,RN },{0x00,0x00,DISP8 },{0x1d,0xff,0 }}},
{56,'h','E','!','E',O_ROTR|O_BYTE,"rotr.b",1,{ABS16,0},4,	{{0x15,0xff,0 },{0x00,0x00,ABS16 },{0x00,0x00,0 },{0x1d,0xff,0 }}},
{56,'h','E','!','E',O_ROTR|O_BYTE,"rotr.b",1,{RNIND_D16,0},4,	{{0xf0,0xf8,RN },{0x00,0x00,DISP16 },{0x00,0x00,0 },{0x1d,0xff,0 }}},
{57,'h','E','!','E',O_ROTR|O_UNSZ,"rotr",1,{RN,0},2,	{{0xa8,0xf8,RN },{0x1d,0xff,0 }}},
{57,'h','E','!','E',O_ROTR|O_UNSZ,"rotr",1,{RNDEC,0},2,	{{0xb8,0xf8,RN },{0x1d,0xff,0 }}},
{57,'h','E','!','E',O_ROTR|O_UNSZ,"rotr",1,{RNIND,0},2,	{{0xd8,0xf8,RN },{0x1d,0xff,0 }}},
{57,'h','E','!','E',O_ROTR|O_UNSZ,"rotr",1,{RNINC,0},2,	{{0xc8,0xf8,RN },{0x1d,0xff,0 }}},
{57,'h','E','!','E',O_ROTR|O_UNSZ,"rotr",1,{ABS8,0},3,	{{0x0d,0xff,0 },{0x00,0x00,ABS8 },{0x1d,0xff,0 }}},
{57,'h','E','!','E',O_ROTR|O_UNSZ,"rotr",1,{RNIND_D8,0},3,	{{0xe8,0xf8,RN },{0x00,0x00,DISP8 },{0x1d,0xff,0 }}},
{57,'h','E','!','E',O_ROTR|O_UNSZ,"rotr",1,{ABS16,0},4,	{{0x1d,0xff,0 },{0x00,0x00,ABS16 },{0x00,0x00,0 },{0x1d,0xff,0 }}},
{57,'h','E','!','E',O_ROTR|O_UNSZ,"rotr",1,{IMM16,0},4,	{{0x0c,0xff,0 },{0x00,0x00,IMM16 },{0x00,0x00,0 },{0x1d,0xff,0 }}},
{57,'h','E','!','E',O_ROTR|O_UNSZ,"rotr",1,{RNIND_D16,0},4,	{{0xf8,0xf8,RN },{0x00,0x00,DISP16 },{0x00,0x00,0 },{0x1d,0xff,0 }}},
{58,'h','E','!','E',O_ROTL|O_WORD,"rotl.w",1,{RN,0},2,	{{0xa8,0xf8,RN },{0x1c,0xff,0 }}},
{58,'h','E','!','E',O_ROTL|O_WORD,"rotl.w",1,{RNIND,0},2,	{{0xd8,0xf8,RN },{0x1c,0xff,0 }}},
{58,'h','E','!','E',O_ROTL|O_WORD,"rotl.w",1,{RNINC,0},2,	{{0xc8,0xf8,RN },{0x1c,0xff,0 }}},
{58,'h','E','!','E',O_ROTL|O_WORD,"rotl.w",1,{RNDEC,0},2,	{{0xb8,0xf8,RN },{0x1c,0xff,0 }}},
{58,'h','E','!','E',O_ROTL|O_WORD,"rotl.w",1,{ABS8,0},3,	{{0x0d,0xff,0 },{0x00,0x00,ABS8 },{0x1c,0xff,0 }}},
{58,'h','E','!','E',O_ROTL|O_WORD,"rotl.w",1,{RNIND_D8,0},3,	{{0xe8,0xf8,RN },{0x00,0x00,DISP8 },{0x1c,0xff,0 }}},
{58,'h','E','!','E',O_ROTL|O_WORD,"rotl.w",1,{IMM16,0},4,	{{0x0c,0xff,0 },{0x00,0x00,IMM16 },{0x00,0x00,0 },{0x1c,0xff,0 }}},
{58,'h','E','!','E',O_ROTL|O_WORD,"rotl.w",1,{ABS16,0},4,	{{0x1d,0xff,0 },{0x00,0x00,ABS16 },{0x00,0x00,0 },{0x1c,0xff,0 }}},
{58,'h','E','!','E',O_ROTL|O_WORD,"rotl.w",1,{RNIND_D16,0},4,	{{0xf8,0xf8,RN },{0x00,0x00,DISP16 },{0x00,0x00,0 },{0x1c,0xff,0 }}},
{59,'h','E','!','E',O_ROTL|O_BYTE,"rotl.b",1,{RN,0},2,	{{0xa0,0xf8,RN },{0x1c,0xff,0 }}},
{59,'h','E','!','E',O_ROTL|O_BYTE,"rotl.b",1,{RNDEC,0},2,	{{0xb0,0xf8,RN },{0x1c,0xff,0 }}},
{59,'h','E','!','E',O_ROTL|O_BYTE,"rotl.b",1,{RNIND,0},2,	{{0xd0,0xf8,RN },{0x1c,0xff,0 }}},
{59,'h','E','!','E',O_ROTL|O_BYTE,"rotl.b",1,{RNINC,0},2,	{{0xc0,0xf8,RN },{0x1c,0xff,0 }}},
{59,'h','E','!','E',O_ROTL|O_BYTE,"rotl.b",1,{IMM8,0},3,	{{0x04,0xff,0 },{0x00,0x00,IMM8 },{0x1c,0xff,0 }}},
{59,'h','E','!','E',O_ROTL|O_BYTE,"rotl.b",1,{ABS8,0},3,	{{0x05,0xff,0 },{0x00,0x00,ABS8 },{0x1c,0xff,0 }}},
{59,'h','E','!','E',O_ROTL|O_BYTE,"rotl.b",1,{RNIND_D8,0},3,	{{0xe0,0xf8,RN },{0x00,0x00,DISP8 },{0x1c,0xff,0 }}},
{59,'h','E','!','E',O_ROTL|O_BYTE,"rotl.b",1,{ABS16,0},4,	{{0x15,0xff,0 },{0x00,0x00,ABS16 },{0x00,0x00,0 },{0x1c,0xff,0 }}},
{59,'h','E','!','E',O_ROTL|O_BYTE,"rotl.b",1,{RNIND_D16,0},4,	{{0xf0,0xf8,RN },{0x00,0x00,DISP16 },{0x00,0x00,0 },{0x1c,0xff,0 }}},
{60,'h','E','!','E',O_ROTL|O_UNSZ,"rotl",1,{RN,0},2,	{{0xa8,0xf8,RN },{0x1c,0xff,0 }}},
{60,'h','E','!','E',O_ROTL|O_UNSZ,"rotl",1,{RNDEC,0},2,	{{0xb8,0xf8,RN },{0x1c,0xff,0 }}},
{60,'h','E','!','E',O_ROTL|O_UNSZ,"rotl",1,{RNIND,0},2,	{{0xd8,0xf8,RN },{0x1c,0xff,0 }}},
{60,'h','E','!','E',O_ROTL|O_UNSZ,"rotl",1,{RNINC,0},2,	{{0xc8,0xf8,RN },{0x1c,0xff,0 }}},
{60,'h','E','!','E',O_ROTL|O_UNSZ,"rotl",1,{RNIND_D8,0},3,	{{0xe8,0xf8,RN },{0x00,0x00,DISP8 },{0x1c,0xff,0 }}},
{60,'h','E','!','E',O_ROTL|O_UNSZ,"rotl",1,{ABS8,0},3,	{{0x0d,0xff,0 },{0x00,0x00,ABS8 },{0x1c,0xff,0 }}},
{60,'h','E','!','E',O_ROTL|O_UNSZ,"rotl",1,{IMM16,0},4,	{{0x0c,0xff,0 },{0x00,0x00,IMM16 },{0x00,0x00,0 },{0x1c,0xff,0 }}},
{60,'h','E','!','E',O_ROTL|O_UNSZ,"rotl",1,{ABS16,0},4,	{{0x1d,0xff,0 },{0x00,0x00,ABS16 },{0x00,0x00,0 },{0x1c,0xff,0 }}},
{60,'h','E','!','E',O_ROTL|O_UNSZ,"rotl",1,{RNIND_D16,0},4,	{{0xf8,0xf8,RN },{0x00,0x00,DISP16 },{0x00,0x00,0 },{0x1c,0xff,0 }}},
{61,'-','B','!','!',O_PRTS|O_UNSZ,"prts",0,{0,0},2,	{{0x11,0xff,0 },{0x19,0xff,0 }}},
{62,'-','B','!','!',O_PRTD|O_UNSZ,"prtd",1,{IMM8,0},3,	{{0x11,0xff,0 },{0x14,0xff,0 },{0x00,0x00,IMM8 }}},
{62,'-','B','!','!',O_PRTD|O_UNSZ,"prtd",1,{IMM16,0},4,	{{0x11,0xff,0 },{0x1c,0xff,0 },{0x00,0x00,IMM16 },{0x00,0x00,0 }}},
{63,'-','J','!','!',O_PJSR|O_UNSZ,"pjsr",1,{RDIND,0},2,	{{0x11,0xff,0 },{0xc8,0xf8,RDIND }}},
{63,'-','J','!','!',O_PJSR|O_UNSZ,"pjsr",1,{ABS24,0},4,	{{0x03,0xff,0 },{0x00,0x00,ABS24 },{0x00,0x00,0 },{0x00,0x00,0 }}},
{64,'-','J','!','!',O_PJMP|O_UNSZ,"pjmp",1,{RDIND,0},2,	{{0x11,0xff,0 },{0xc0,0xf8,RDIND }}},
{64,'-','J','!','!',O_PJMP|O_UNSZ,"pjmp",1,{ABS24,0},4,	{{0x13,0xff,0 },{0x00,0x00,ABS24 },{0x00,0x00,0 },{0x00,0x00,0 }}},
{65,'s','I','C','C',O_ORC|O_WORD,"orc.w",2,{IMM16,CRW},4,	{{0x0c,0xff,0 },{0x00,0x00,IMM16 },{0x00,0x00,0 },{0x48,0xf8,CRW }}},
{66,'s','I','C','C',O_ORC|O_BYTE,"orc.b",2,{IMM8,CRB},3,	{{0x04,0xff,0 },{0x00,0x00,IMM8 },{0x48,0xf8,CRB }}},
{67,'s','I','C','C',O_ORC|O_UNSZ,"orc",2,{IMM8,CRB},3,	{{0x04,0xff,0 },{0x00,0x00,IMM8 },{0x48,0xf8,CRB }}},
{67,'s','I','C','C',O_ORC|O_UNSZ,"orc",2,{IMM16,CRW},4,	{{0x0c,0xff,0 },{0x00,0x00,IMM16 },{0x00,0x00,0 },{0x48,0xf8,CRW }}},
{68,'m','E','D','D',O_OR|O_WORD,"or.w",2,{RN,RD},2,	{{0xa8,0xf8,RN },{0x40,0xf8,RD }}},
{68,'m','E','D','D',O_OR|O_WORD,"or.w",2,{RNIND,RD},2,	{{0xd8,0xf8,RN },{0x40,0xf8,RD }}},
{68,'m','E','D','D',O_OR|O_WORD,"or.w",2,{RNDEC,RD},2,	{{0xb8,0xf8,RN },{0x40,0xf8,RD }}},
{68,'m','E','D','D',O_OR|O_WORD,"or.w",2,{RNINC,RD},2,	{{0xc8,0xf8,RN },{0x40,0xf8,RD }}},
{68,'m','E','D','D',O_OR|O_WORD,"or.w",2,{ABS8,RD},3,	{{0x0d,0xff,0 },{0x00,0x00,ABS8 },{0x40,0xf8,RD }}},
{68,'m','E','D','D',O_OR|O_WORD,"or.w",2,{RNIND_D8,RD},3,	{{0xe8,0xf8,RN },{0x00,0x00,DISP8 },{0x40,0xf8,RD }}},
{68,'m','E','D','D',O_OR|O_WORD,"or.w",2,{ABS16,RD},4,	{{0x1d,0xff,0 },{0x00,0x00,ABS16 },{0x00,0x00,0 },{0x40,0xf8,RD }}},
{68,'m','E','D','D',O_OR|O_WORD,"or.w",2,{IMM16,RD},4,	{{0x0c,0xff,0 },{0x00,0x00,IMM16 },{0x00,0x00,0 },{0x40,0xf8,RD }}},
{68,'m','E','D','D',O_OR|O_WORD,"or.w",2,{RNIND_D16,RD},4,	{{0xf8,0xf8,RN },{0x00,0x00,DISP16 },{0x00,0x00,0 },{0x40,0xf8,RD }}},
{69,'m','E','D','D',O_OR|O_BYTE,"or.b",2,{RN,RD},2,	{{0xa0,0xf8,RN },{0x40,0xf8,RD }}},
{69,'m','E','D','D',O_OR|O_BYTE,"or.b",2,{RNINC,RD},2,	{{0xc0,0xf8,RN },{0x40,0xf8,RD }}},
{69,'m','E','D','D',O_OR|O_BYTE,"or.b",2,{RNDEC,RD},2,	{{0xb0,0xf8,RN },{0x40,0xf8,RD }}},
{69,'m','E','D','D',O_OR|O_BYTE,"or.b",2,{RNIND,RD},2,	{{0xd0,0xf8,RN },{0x40,0xf8,RD }}},
{69,'m','E','D','D',O_OR|O_BYTE,"or.b",2,{ABS8,RD},3,	{{0x05,0xff,0 },{0x00,0x00,ABS8 },{0x40,0xf8,RD }}},
{69,'m','E','D','D',O_OR|O_BYTE,"or.b",2,{IMM8,RD},3,	{{0x04,0xff,0 },{0x00,0x00,IMM8 },{0x40,0xf8,RD }}},
{69,'m','E','D','D',O_OR|O_BYTE,"or.b",2,{RNIND_D8,RD},3,	{{0xe0,0xf8,RN },{0x00,0x00,DISP8 },{0x40,0xf8,RD }}},
{69,'m','E','D','D',O_OR|O_BYTE,"or.b",2,{ABS16,RD},4,	{{0x15,0xff,0 },{0x00,0x00,ABS16 },{0x00,0x00,0 },{0x40,0xf8,RD }}},
{69,'m','E','D','D',O_OR|O_BYTE,"or.b",2,{RNIND_D16,RD},4,	{{0xf0,0xf8,RN },{0x00,0x00,DISP16 },{0x00,0x00,0 },{0x40,0xf8,RD }}},
{70,'m','E','D','D',O_OR|O_UNSZ,"or",2,{RN,RD},2,	{{0xa8,0xf8,RN },{0x40,0xf8,RD }}},
{70,'m','E','D','D',O_OR|O_UNSZ,"or",2,{RNIND,RD},2,	{{0xd8,0xf8,RN },{0x40,0xf8,RD }}},
{70,'m','E','D','D',O_OR|O_UNSZ,"or",2,{RNDEC,RD},2,	{{0xb8,0xf8,RN },{0x40,0xf8,RD }}},
{70,'m','E','D','D',O_OR|O_UNSZ,"or",2,{RNINC,RD},2,	{{0xc8,0xf8,RN },{0x40,0xf8,RD }}},
{70,'m','E','D','D',O_OR|O_UNSZ,"or",2,{ABS8,RD},3,	{{0x0d,0xff,0 },{0x00,0x00,ABS8 },{0x40,0xf8,RD }}},
{70,'m','E','D','D',O_OR|O_UNSZ,"or",2,{RNIND_D8,RD},3,	{{0xe8,0xf8,RN },{0x00,0x00,DISP8 },{0x40,0xf8,RD }}},
{70,'m','E','D','D',O_OR|O_UNSZ,"or",2,{ABS16,RD},4,	{{0x1d,0xff,0 },{0x00,0x00,ABS16 },{0x00,0x00,0 },{0x40,0xf8,RD }}},
{70,'m','E','D','D',O_OR|O_UNSZ,"or",2,{IMM16,RD},4,	{{0x0c,0xff,0 },{0x00,0x00,IMM16 },{0x00,0x00,0 },{0x40,0xf8,RD }}},
{70,'m','E','D','D',O_OR|O_UNSZ,"or",2,{RNIND_D16,RD},4,	{{0xf8,0xf8,RN },{0x00,0x00,DISP16 },{0x00,0x00,0 },{0x40,0xf8,RD }}},
{71,'m','E','!','E',O_NOT|O_WORD,"not.w",1,{RN,0},2,	{{0xa8,0xf8,RN },{0x15,0xff,0 }}},
{71,'m','E','!','E',O_NOT|O_WORD,"not.w",1,{RNINC,0},2,	{{0xc8,0xf8,RN },{0x15,0xff,0 }}},
{71,'m','E','!','E',O_NOT|O_WORD,"not.w",1,{RNIND,0},2,	{{0xd8,0xf8,RN },{0x15,0xff,0 }}},
{71,'m','E','!','E',O_NOT|O_WORD,"not.w",1,{RNDEC,0},2,	{{0xb8,0xf8,RN },{0x15,0xff,0 }}},
{71,'m','E','!','E',O_NOT|O_WORD,"not.w",1,{ABS8,0},3,	{{0x0d,0xff,0 },{0x00,0x00,ABS8 },{0x15,0xff,0 }}},
{71,'m','E','!','E',O_NOT|O_WORD,"not.w",1,{RNIND_D8,0},3,	{{0xe8,0xf8,RN },{0x00,0x00,DISP8 },{0x15,0xff,0 }}},
{71,'m','E','!','E',O_NOT|O_WORD,"not.w",1,{ABS16,0},4,	{{0x1d,0xff,0 },{0x00,0x00,ABS16 },{0x00,0x00,0 },{0x15,0xff,0 }}},
{71,'m','E','!','E',O_NOT|O_WORD,"not.w",1,{IMM16,0},4,	{{0x0c,0xff,0 },{0x00,0x00,IMM16 },{0x00,0x00,0 },{0x15,0xff,0 }}},
{71,'m','E','!','E',O_NOT|O_WORD,"not.w",1,{RNIND_D16,0},4,	{{0xf8,0xf8,RN },{0x00,0x00,DISP16 },{0x00,0x00,0 },{0x15,0xff,0 }}},
{72,'m','E','!','E',O_NOT|O_BYTE,"not.b",1,{RN,0},2,	{{0xa0,0xf8,RN },{0x15,0xff,0 }}},
{72,'m','E','!','E',O_NOT|O_BYTE,"not.b",1,{RNIND,0},2,	{{0xd0,0xf8,RN },{0x15,0xff,0 }}},
{72,'m','E','!','E',O_NOT|O_BYTE,"not.b",1,{RNINC,0},2,	{{0xc0,0xf8,RN },{0x15,0xff,0 }}},
{72,'m','E','!','E',O_NOT|O_BYTE,"not.b",1,{RNDEC,0},2,	{{0xb0,0xf8,RN },{0x15,0xff,0 }}},
{72,'m','E','!','E',O_NOT|O_BYTE,"not.b",1,{IMM8,0},3,	{{0x04,0xff,0 },{0x00,0x00,IMM8 },{0x15,0xff,0 }}},
{72,'m','E','!','E',O_NOT|O_BYTE,"not.b",1,{ABS8,0},3,	{{0x05,0xff,0 },{0x00,0x00,ABS8 },{0x15,0xff,0 }}},
{72,'m','E','!','E',O_NOT|O_BYTE,"not.b",1,{RNIND_D8,0},3,	{{0xe0,0xf8,RN },{0x00,0x00,DISP8 },{0x15,0xff,0 }}},
{72,'m','E','!','E',O_NOT|O_BYTE,"not.b",1,{ABS16,0},4,	{{0x15,0xff,0 },{0x00,0x00,ABS16 },{0x00,0x00,0 },{0x15,0xff,0 }}},
{72,'m','E','!','E',O_NOT|O_BYTE,"not.b",1,{RNIND_D16,0},4,	{{0xf0,0xf8,RN },{0x00,0x00,DISP16 },{0x00,0x00,0 },{0x15,0xff,0 }}},
{73,'m','E','!','E',O_NOT|O_UNSZ,"not",1,{RN,0},2,	{{0xa8,0xf8,RN },{0x15,0xff,0 }}},
{73,'m','E','!','E',O_NOT|O_UNSZ,"not",1,{RNIND,0},2,	{{0xd8,0xf8,RN },{0x15,0xff,0 }}},
{73,'m','E','!','E',O_NOT|O_UNSZ,"not",1,{RNDEC,0},2,	{{0xb8,0xf8,RN },{0x15,0xff,0 }}},
{73,'m','E','!','E',O_NOT|O_UNSZ,"not",1,{RNINC,0},2,	{{0xc8,0xf8,RN },{0x15,0xff,0 }}},
{73,'m','E','!','E',O_NOT|O_UNSZ,"not",1,{ABS8,0},3,	{{0x0d,0xff,0 },{0x00,0x00,ABS8 },{0x15,0xff,0 }}},
{73,'m','E','!','E',O_NOT|O_UNSZ,"not",1,{RNIND_D8,0},3,	{{0xe8,0xf8,RN },{0x00,0x00,DISP8 },{0x15,0xff,0 }}},
{73,'m','E','!','E',O_NOT|O_UNSZ,"not",1,{ABS16,0},4,	{{0x1d,0xff,0 },{0x00,0x00,ABS16 },{0x00,0x00,0 },{0x15,0xff,0 }}},
{73,'m','E','!','E',O_NOT|O_UNSZ,"not",1,{IMM16,0},4,	{{0x0c,0xff,0 },{0x00,0x00,IMM16 },{0x00,0x00,0 },{0x15,0xff,0 }}},
{73,'m','E','!','E',O_NOT|O_UNSZ,"not",1,{RNIND_D16,0},4,	{{0xf8,0xf8,RN },{0x00,0x00,DISP16 },{0x00,0x00,0 },{0x15,0xff,0 }}},
{74,'-','!','!','!',O_NOP|O_UNSZ,"nop",0,{0,0},1,	{{0x00,0xff,0 }}},
{75,'a','E','!','E',O_NEG|O_WORD,"neg.w",1,{RN,0},2,	{{0xa8,0xf8,RN },{0x14,0xff,0 }}},
{75,'a','E','!','E',O_NEG|O_WORD,"neg.w",1,{RNDEC,0},2,	{{0xb8,0xf8,RN },{0x14,0xff,0 }}},
{75,'a','E','!','E',O_NEG|O_WORD,"neg.w",1,{RNINC,0},2,	{{0xc8,0xf8,RN },{0x14,0xff,0 }}},
{75,'a','E','!','E',O_NEG|O_WORD,"neg.w",1,{RNIND,0},2,	{{0xd8,0xf8,RN },{0x14,0xff,0 }}},
{75,'a','E','!','E',O_NEG|O_WORD,"neg.w",1,{ABS8,0},3,	{{0x0d,0xff,0 },{0x00,0x00,ABS8 },{0x14,0xff,0 }}},
{75,'a','E','!','E',O_NEG|O_WORD,"neg.w",1,{RNIND_D8,0},3,	{{0xe8,0xf8,RN },{0x00,0x00,DISP8 },{0x14,0xff,0 }}},
{75,'a','E','!','E',O_NEG|O_WORD,"neg.w",1,{ABS16,0},4,	{{0x1d,0xff,0 },{0x00,0x00,ABS16 },{0x00,0x00,0 },{0x14,0xff,0 }}},
{75,'a','E','!','E',O_NEG|O_WORD,"neg.w",1,{IMM16,0},4,	{{0x0c,0xff,0 },{0x00,0x00,IMM16 },{0x00,0x00,0 },{0x14,0xff,0 }}},
{75,'a','E','!','E',O_NEG|O_WORD,"neg.w",1,{RNIND_D16,0},4,	{{0xf8,0xf8,RN },{0x00,0x00,DISP16 },{0x00,0x00,0 },{0x14,0xff,0 }}},
{76,'a','E','!','E',O_NEG|O_BYTE,"neg.b",1,{RN,0},2,	{{0xa0,0xf8,RN },{0x14,0xff,0 }}},
{76,'a','E','!','E',O_NEG|O_BYTE,"neg.b",1,{RNDEC,0},2,	{{0xb0,0xf8,RN },{0x14,0xff,0 }}},
{76,'a','E','!','E',O_NEG|O_BYTE,"neg.b",1,{RNIND,0},2,	{{0xd0,0xf8,RN },{0x14,0xff,0 }}},
{76,'a','E','!','E',O_NEG|O_BYTE,"neg.b",1,{RNINC,0},2,	{{0xc0,0xf8,RN },{0x14,0xff,0 }}},
{76,'a','E','!','E',O_NEG|O_BYTE,"neg.b",1,{IMM8,0},3,	{{0x04,0xff,0 },{0x00,0x00,IMM8 },{0x14,0xff,0 }}},
{76,'a','E','!','E',O_NEG|O_BYTE,"neg.b",1,{ABS8,0},3,	{{0x05,0xff,0 },{0x00,0x00,ABS8 },{0x14,0xff,0 }}},
{76,'a','E','!','E',O_NEG|O_BYTE,"neg.b",1,{RNIND_D8,0},3,	{{0xe0,0xf8,RN },{0x00,0x00,DISP8 },{0x14,0xff,0 }}},
{76,'a','E','!','E',O_NEG|O_BYTE,"neg.b",1,{ABS16,0},4,	{{0x15,0xff,0 },{0x00,0x00,ABS16 },{0x00,0x00,0 },{0x14,0xff,0 }}},
{76,'a','E','!','E',O_NEG|O_BYTE,"neg.b",1,{RNIND_D16,0},4,	{{0xf0,0xf8,RN },{0x00,0x00,DISP16 },{0x00,0x00,0 },{0x14,0xff,0 }}},
{77,'a','E','!','E',O_NEG|O_UNSZ,"neg",1,{RN,0},2,	{{0xa8,0xf8,RN },{0x14,0xff,0 }}},
{77,'a','E','!','E',O_NEG|O_UNSZ,"neg",1,{RNDEC,0},2,	{{0xb8,0xf8,RN },{0x14,0xff,0 }}},
{77,'a','E','!','E',O_NEG|O_UNSZ,"neg",1,{RNINC,0},2,	{{0xc8,0xf8,RN },{0x14,0xff,0 }}},
{77,'a','E','!','E',O_NEG|O_UNSZ,"neg",1,{RNIND,0},2,	{{0xd8,0xf8,RN },{0x14,0xff,0 }}},
{77,'a','E','!','E',O_NEG|O_UNSZ,"neg",1,{ABS8,0},3,	{{0x0d,0xff,0 },{0x00,0x00,ABS8 },{0x14,0xff,0 }}},
{77,'a','E','!','E',O_NEG|O_UNSZ,"neg",1,{RNIND_D8,0},3,	{{0xe8,0xf8,RN },{0x00,0x00,DISP8 },{0x14,0xff,0 }}},
{77,'a','E','!','E',O_NEG|O_UNSZ,"neg",1,{ABS16,0},4,	{{0x1d,0xff,0 },{0x00,0x00,ABS16 },{0x00,0x00,0 },{0x14,0xff,0 }}},
{77,'a','E','!','E',O_NEG|O_UNSZ,"neg",1,{IMM16,0},4,	{{0x0c,0xff,0 },{0x00,0x00,IMM16 },{0x00,0x00,0 },{0x14,0xff,0 }}},
{77,'a','E','!','E',O_NEG|O_UNSZ,"neg",1,{RNIND_D16,0},4,	{{0xf8,0xf8,RN },{0x00,0x00,DISP16 },{0x00,0x00,0 },{0x14,0xff,0 }}},
{78,'p','E','D','D',O_MULXU|O_WORD,"mulxu.w",2,{RN,RD},2,	{{0xa8,0xf8,RN },{0xa8,0xf8,RD }}},
{78,'p','E','D','D',O_MULXU|O_WORD,"mulxu.w",2,{RNDEC,RD},2,	{{0xb8,0xf8,RN },{0xa8,0xf8,RD }}},
{78,'p','E','D','D',O_MULXU|O_WORD,"mulxu.w",2,{RNINC,RD},2,	{{0xc8,0xf8,RN },{0xa8,0xf8,RD }}},
{78,'p','E','D','D',O_MULXU|O_WORD,"mulxu.w",2,{RNIND,RD},2,	{{0xd8,0xf8,RN },{0xa8,0xf8,RD }}},
{78,'p','E','D','D',O_MULXU|O_WORD,"mulxu.w",2,{ABS8,RD},3,	{{0x0d,0xff,0 },{0x00,0x00,ABS8 },{0xa8,0xf8,RD }}},
{78,'p','E','D','D',O_MULXU|O_WORD,"mulxu.w",2,{RNIND_D8,RD},3,	{{0xe8,0xf8,RN },{0x00,0x00,DISP8 },{0xa8,0xf8,RD }}},
{78,'p','E','D','D',O_MULXU|O_WORD,"mulxu.w",2,{ABS16,RD},4,	{{0x1d,0xff,0 },{0x00,0x00,ABS16 },{0x00,0x00,0 },{0xa8,0xf8,RD }}},
{78,'p','E','D','D',O_MULXU|O_WORD,"mulxu.w",2,{IMM16,RD},4,	{{0x0c,0xff,0 },{0x00,0x00,IMM16 },{0x00,0x00,0 },{0xa8,0xf8,RD }}},
{78,'p','E','D','D',O_MULXU|O_WORD,"mulxu.w",2,{RNIND_D16,RD},4,	{{0xf8,0xf8,RN },{0x00,0x00,DISP16 },{0x00,0x00,0 },{0xa8,0xf8,RD }}},
{79,'p','E','D','D',O_MULXU|O_BYTE,"mulxu.b",2,{RN,RD},2,	{{0xa0,0xf8,RN },{0xa8,0xf8,RD }}},
{79,'p','E','D','D',O_MULXU|O_BYTE,"mulxu.b",2,{RNIND,RD},2,	{{0xd0,0xf8,RN },{0xa8,0xf8,RD }}},
{79,'p','E','D','D',O_MULXU|O_BYTE,"mulxu.b",2,{RNINC,RD},2,	{{0xc0,0xf8,RN },{0xa8,0xf8,RD }}},
{79,'p','E','D','D',O_MULXU|O_BYTE,"mulxu.b",2,{RNDEC,RD},2,	{{0xb0,0xf8,RN },{0xa8,0xf8,RD }}},
{79,'p','E','D','D',O_MULXU|O_BYTE,"mulxu.b",2,{IMM8,RD},3,	{{0x04,0xff,0 },{0x00,0x00,IMM8 },{0xa8,0xf8,RD }}},
{79,'p','E','D','D',O_MULXU|O_BYTE,"mulxu.b",2,{ABS8,RD},3,	{{0x05,0xff,0 },{0x00,0x00,ABS8 },{0xa8,0xf8,RD }}},
{79,'p','E','D','D',O_MULXU|O_BYTE,"mulxu.b",2,{RNIND_D8,RD},3,	{{0xe0,0xf8,RN },{0x00,0x00,DISP8 },{0xa8,0xf8,RD }}},
{79,'p','E','D','D',O_MULXU|O_BYTE,"mulxu.b",2,{ABS16,RD},4,	{{0x15,0xff,0 },{0x00,0x00,ABS16 },{0x00,0x00,0 },{0xa8,0xf8,RD }}},
{79,'p','E','D','D',O_MULXU|O_BYTE,"mulxu.b",2,{RNIND_D16,RD},4,	{{0xf0,0xf8,RN },{0x00,0x00,DISP16 },{0x00,0x00,0 },{0xa8,0xf8,RD }}},
{80,'p','E','D','D',O_MULXU|O_UNSZ,"mulxu",2,{RN,RD},2,	{{0xa8,0xf8,RN },{0xa8,0xf8,RD }}},
{80,'p','E','D','D',O_MULXU|O_UNSZ,"mulxu",2,{RNIND,RD},2,	{{0xd8,0xf8,RN },{0xa8,0xf8,RD }}},
{80,'p','E','D','D',O_MULXU|O_UNSZ,"mulxu",2,{RNDEC,RD},2,	{{0xb8,0xf8,RN },{0xa8,0xf8,RD }}},
{80,'p','E','D','D',O_MULXU|O_UNSZ,"mulxu",2,{RNINC,RD},2,	{{0xc8,0xf8,RN },{0xa8,0xf8,RD }}},
{80,'p','E','D','D',O_MULXU|O_UNSZ,"mulxu",2,{RNIND_D8,RD},3,	{{0xe8,0xf8,RN },{0x00,0x00,DISP8 },{0xa8,0xf8,RD }}},
{80,'p','E','D','D',O_MULXU|O_UNSZ,"mulxu",2,{ABS8,RD},3,	{{0x0d,0xff,0 },{0x00,0x00,ABS8 },{0xa8,0xf8,RD }}},
{80,'p','E','D','D',O_MULXU|O_UNSZ,"mulxu",2,{IMM16,RD},4,	{{0x0c,0xff,0 },{0x00,0x00,IMM16 },{0x00,0x00,0 },{0xa8,0xf8,RD }}},
{80,'p','E','D','D',O_MULXU|O_UNSZ,"mulxu",2,{ABS16,RD},4,	{{0x1d,0xff,0 },{0x00,0x00,ABS16 },{0x00,0x00,0 },{0xa8,0xf8,RD }}},
{80,'p','E','D','D',O_MULXU|O_UNSZ,"mulxu",2,{RNIND_D16,RD},4,	{{0xf8,0xf8,RN },{0x00,0x00,DISP16 },{0x00,0x00,0 },{0xa8,0xf8,RD }}},
{81,'-','S','!','E',O_MOVTPE|O_BYTE,"movtpe.b",2,{RS,RN},3,	{{0xa0,0xf8,RN },{0x00,0xff,0 },{0x90,0xf8,RS }}},
{81,'-','S','!','E',O_MOVTPE|O_BYTE,"movtpe.b",2,{RS,RNDEC},3,	{{0xb0,0xf8,RN },{0x00,0xff,0 },{0x90,0xf8,RS }}},
{81,'-','S','!','E',O_MOVTPE|O_BYTE,"movtpe.b",2,{RS,RNINC},3,	{{0xc0,0xf8,RN },{0x00,0xff,0 },{0x90,0xf8,RS }}},
{81,'-','S','!','E',O_MOVTPE|O_BYTE,"movtpe.b",2,{RS,RNIND},3,	{{0xd0,0xf8,RN },{0x00,0xff,0 },{0x90,0xf8,RS }}},
{81,'-','S','!','E',O_MOVTPE|O_BYTE,"movtpe.b",2,{RS,ABS8},4,	{{0x05,0xff,0 },{0x00,0x00,ABS8 },{0x00,0xff,0 },{0x90,0xf8,RS }}},
{81,'-','S','!','E',O_MOVTPE|O_BYTE,"movtpe.b",2,{RS,RNIND_D8},4,	{{0xe0,0xf8,RN },{0x00,0x00,DISP8 },{0x00,0xff,0 },{0x90,0xf8,RS }}},
{81,'-','S','!','E',O_MOVTPE|O_BYTE,"movtpe.b",2,{RS,ABS16},5,	{{0x15,0xff,0 },{0x00,0x00,ABS16 },{0x00,0x00,0 },{0x00,0xff,0 },{0x90,0xf8,RS }}},
{81,'-','S','!','E',O_MOVTPE|O_BYTE,"movtpe.b",2,{RS,RNIND_D16},5,	{{0xf0,0xf8,RN },{0x00,0x00,DISP16 },{0x00,0x00,0 },{0x00,0xff,0 },{0x90,0xf8,RS }}},
{82,'-','S','!','E',O_MOVTPE|O_UNSZ,"movtpe",2,{RS,RN},3,	{{0xa0,0xf8,RN },{0x00,0xff,0 },{0x90,0xf8,RS }}},
{82,'-','S','!','E',O_MOVTPE|O_UNSZ,"movtpe",2,{RS,RNDEC},3,	{{0xb0,0xf8,RN },{0x00,0xff,0 },{0x90,0xf8,RS }}},
{82,'-','S','!','E',O_MOVTPE|O_UNSZ,"movtpe",2,{RS,RNIND},3,	{{0xd0,0xf8,RN },{0x00,0xff,0 },{0x90,0xf8,RS }}},
{82,'-','S','!','E',O_MOVTPE|O_UNSZ,"movtpe",2,{RS,RNINC},3,	{{0xc0,0xf8,RN },{0x00,0xff,0 },{0x90,0xf8,RS }}},
{82,'-','S','!','E',O_MOVTPE|O_UNSZ,"movtpe",2,{RS,ABS8},4,	{{0x05,0xff,0 },{0x00,0x00,ABS8 },{0x00,0xff,0 },{0x90,0xf8,RS }}},
{82,'-','S','!','E',O_MOVTPE|O_UNSZ,"movtpe",2,{RS,RNIND_D8},4,	{{0xe0,0xf8,RN },{0x00,0x00,DISP8 },{0x00,0xff,0 },{0x90,0xf8,RS }}},
{82,'-','S','!','E',O_MOVTPE|O_UNSZ,"movtpe",2,{RS,ABS16},5,	{{0x15,0xff,0 },{0x00,0x00,ABS16 },{0x00,0x00,0 },{0x00,0xff,0 },{0x90,0xf8,RS }}},
{82,'-','S','!','E',O_MOVTPE|O_UNSZ,"movtpe",2,{RS,RNIND_D16},5,	{{0xf0,0xf8,RN },{0x00,0x00,DISP16 },{0x00,0x00,0 },{0x00,0xff,0 },{0x90,0xf8,RS }}},
{83,'-','E','!','D',O_MOVFPE|O_BYTE,"movfpe.b",2,{RN,RD},3,	{{0xa0,0xf8,RN },{0x00,0xff,0 },{0x80,0xf8,RD }}},
{83,'-','E','!','D',O_MOVFPE|O_BYTE,"movfpe.b",2,{RNINC,RD},3,	{{0xc0,0xf8,RN },{0x00,0xff,0 },{0x80,0xf8,RD }}},
{83,'-','E','!','D',O_MOVFPE|O_BYTE,"movfpe.b",2,{RNIND,RD},3,	{{0xd0,0xf8,RN },{0x00,0xff,0 },{0x80,0xf8,RD }}},
{83,'-','E','!','D',O_MOVFPE|O_BYTE,"movfpe.b",2,{RNDEC,RD},3,	{{0xb0,0xf8,RN },{0x00,0xff,0 },{0x80,0xf8,RD }}},
{83,'-','E','!','D',O_MOVFPE|O_BYTE,"movfpe.b",2,{IMM8,RD},4,	{{0x04,0xff,0 },{0x00,0x00,IMM8 },{0x00,0xff,0 },{0x80,0xf8,RD }}},
{83,'-','E','!','D',O_MOVFPE|O_BYTE,"movfpe.b",2,{ABS8,RD},4,	{{0x05,0xff,0 },{0x00,0x00,ABS8 },{0x00,0xff,0 },{0x80,0xf8,RD }}},
{83,'-','E','!','D',O_MOVFPE|O_BYTE,"movfpe.b",2,{RNIND_D8,RD},4,	{{0xe0,0xf8,RN },{0x00,0x00,DISP8 },{0x00,0xff,0 },{0x80,0xf8,RD }}},
{83,'-','E','!','D',O_MOVFPE|O_BYTE,"movfpe.b",2,{ABS16,RD},5,	{{0x15,0xff,0 },{0x00,0x00,ABS16 },{0x00,0x00,0 },{0x00,0xff,0 },{0x80,0xf8,RD }}},
{83,'-','E','!','D',O_MOVFPE|O_BYTE,"movfpe.b",2,{RNIND_D16,RD},5,	{{0xf0,0xf8,RN },{0x00,0x00,DISP16 },{0x00,0x00,0 },{0x00,0xff,0 },{0x80,0xf8,RD }}},
{84,'-','E','!','D',O_MOVFPE|O_UNSZ,"movfpe",2,{RN,RD},3,	{{0xa0,0xf8,RN },{0x00,0xff,0 },{0x80,0xf8,RD }}},
{84,'-','E','!','D',O_MOVFPE|O_UNSZ,"movfpe",2,{RNINC,RD},3,	{{0xc0,0xf8,RN },{0x00,0xff,0 },{0x80,0xf8,RD }}},
{84,'-','E','!','D',O_MOVFPE|O_UNSZ,"movfpe",2,{RNIND,RD},3,	{{0xd0,0xf8,RN },{0x00,0xff,0 },{0x80,0xf8,RD }}},
{84,'-','E','!','D',O_MOVFPE|O_UNSZ,"movfpe",2,{RNDEC,RD},3,	{{0xb0,0xf8,RN },{0x00,0xff,0 },{0x80,0xf8,RD }}},
{84,'-','E','!','D',O_MOVFPE|O_UNSZ,"movfpe",2,{IMM8,RD},4,	{{0x04,0xff,0 },{0x00,0x00,IMM8 },{0x00,0xff,0 },{0x80,0xf8,RD }}},
{84,'-','E','!','D',O_MOVFPE|O_UNSZ,"movfpe",2,{ABS8,RD},4,	{{0x05,0xff,0 },{0x00,0x00,ABS8 },{0x00,0xff,0 },{0x80,0xf8,RD }}},
{84,'-','E','!','D',O_MOVFPE|O_UNSZ,"movfpe",2,{RNIND_D8,RD},4,	{{0xe0,0xf8,RN },{0x00,0x00,DISP8 },{0x00,0xff,0 },{0x80,0xf8,RD }}},
{84,'-','E','!','D',O_MOVFPE|O_UNSZ,"movfpe",2,{ABS16,RD},5,	{{0x15,0xff,0 },{0x00,0x00,ABS16 },{0x00,0x00,0 },{0x00,0xff,0 },{0x80,0xf8,RD }}},
{84,'-','E','!','D',O_MOVFPE|O_UNSZ,"movfpe",2,{RNIND_D16,RD},5,	{{0xf0,0xf8,RN },{0x00,0x00,DISP16 },{0x00,0x00,0 },{0x00,0xff,0 },{0x80,0xf8,RD }}},
{85,'m','S','!','E',O_MOV|O_WORD,"mov:s.w",2,{RS,ABS8},2,	{{0x78,0xf8,RS },{0x00,0x00,ABS8 }}},
{86,'m','S','!','E',O_MOV|O_BYTE,"mov:s.b",2,{RS,ABS8},2,	{{0x70,0xf8,RS },{0x00,0x00,ABS8 }}},
{87,'m','S','!','E',O_MOV|O_UNSZ,"mov:s",2,{RS,ABS8},2,	{{0x78,0xf8,RS },{0x00,0x00,ABS8 }}},
{88,'m','E','!','D',O_MOV|O_WORD,"mov:l.w",2,{ABS8,RD},2,	{{0x68,0xf8,RD },{0x00,0x00,ABS8 }}},
{89,'m','E','!','D',O_MOV|O_BYTE,"mov:l.b",2,{ABS8,RD},2,	{{0x60,0xf8,RD },{0x00,0x00,ABS8 }}},
{90,'m','E','!','D',O_MOV|O_UNSZ,"mov:l",2,{ABS8,RD},2,	{{0x68,0xf8,RD },{0x00,0x00,ABS8 }}},
{91,'m','I','!','D',O_MOV|O_WORD,"mov:i.w",2,{IMM16,RD},3,	{{0x58,0xf8,RD },{0x00,0x00,IMM16 },{0x00,0x00,0 }}},
{92,'m','I','!','D',O_MOV|O_UNSZ,"mov:i",  2,{IMM16,RD},3,	{{0x58,0xf8,RD },{0x00,0x00,IMM16 },{0x00,0x00,0 }}},
{93,'m','S','!','E',O_MOV|O_WORD,"mov:g.w",2,{RS,RNIND},2,	{{0xd8,0xf8,RN },{0x90,0xf8,RS }}},
{93,'m','E','!','D',O_MOV|O_WORD,"mov:g.w",2,{RNIND,RD},2,	{{0xd8,0xf8,RN },{0x80,0xf8,RD }}},
{93,'m','S','!','E',O_MOV|O_WORD,"mov:g.w",2,{RS,RNDEC},2,	{{0xb8,0xf8,RN },{0x90,0xf8,RS }}},
{93,'m','S','!','E',O_MOV|O_WORD,"mov:g.w",2,{RS,RNINC},2,	{{0xc8,0xf8,RN },{0x90,0xf8,RS }}},
{93,'m','E','!','D',O_MOV|O_WORD,"mov:g.w",2,{RNDEC,RD},2,	{{0xb8,0xf8,RN },{0x80,0xf8,RD }}},
{93,'m','E','!','D',O_MOV|O_WORD,"mov:g.w",2,{RNINC,RD},2,	{{0xc8,0xf8,RN },{0x80,0xf8,RD }}},
{93,'m','E','!','D',O_MOV|O_WORD,"mov:g.w",2,{RN,RD},2,		{{0xa8,0xf8,RN },{0x80,0xf8,RD }}},
{93,'m','S','!','E',O_MOV|O_WORD,"mov:g.w",2,{RS,ABS8},3,	{{0x0d,0xff,0 },{0x00,0x00,ABS8 },{0x90,0xf8,RS }}},
{93,'m','S','!','E',O_MOV|O_WORD,"mov:g.w",2,{RS,RNIND_D8},3,	{{0xe8,0xf8,RN },{0x00,0x00,DISP8 },{0x90,0xf8,RS }}},
{93,'m','E','!','D',O_MOV|O_WORD,"mov:g.w",2,{RNIND_D8,RD},3,	{{0xe8,0xf8,RN },{0x00,0x00,DISP8 },{0x80,0xf8,RD }}},
{93,'m','E','!','D',O_MOV|O_WORD,"mov:g.w",2,{ABS8,RD},3,	{{0x0d,0xff,0 },{0x00,0x00,ABS8 },{0x80,0xf8,RD }}},
{93,'m','I','!','E',O_MOV|O_WORD,"mov:g.w",2,{IMM16,RNIND},4,	{{0xd8,0xf8,RN },{0x07,0xff,0 },{0x00,0x00,IMM16 },{0x00,0x00,0 }}},
{93,'m','S','!','E',O_MOV|O_WORD,"mov:g.w",2,{RS,RNIND_D16},4,	{{0xf8,0xf8,RN },{0x00,0x00,DISP16 },{0x00,0x00,0 },{0x90,0xf8,RS }}},
{93,'m','I','!','E',O_MOV|O_WORD,"mov:g.w",2,{IMM16,RNDEC},4,	{{0xb8,0xf8,RN },{0x07,0xff,0 },{0x00,0x00,IMM16 },{0x00,0x00,0 }}},
{93,'m','E','!','D',O_MOV|O_WORD,"mov:g.w",2,{IMM16,RD},4,	{{0x0c,0xff,0 },{0x00,0x00,IMM16 },{0x00,0x00,0 },{0x80,0xf8,RD }}},
{93,'m','S','!','E',O_MOV|O_WORD,"mov:g.w",2,{RS,ABS16},4,	{{0x1d,0xff,0 },{0x00,0x00,ABS16 },{0x00,0x00,0 },{0x90,0xf8,RS }}},
{93,'m','I','!','E',O_MOV|O_WORD,"mov:g.w",2,{IMM16,RNINC},4,	{{0xc8,0xf8,RN },{0x07,0xff,0 },{0x00,0x00,IMM16 },{0x00,0x00,0 }}},
{93,'m','E','!','D',O_MOV|O_WORD,"mov:g.w",2,{ABS16,RD},4,	{{0x1d,0xff,0 },{0x00,0x00,ABS16 },{0x00,0x00,0 },{0x80,0xf8,RD }}},
{93,'m','E','!','D',O_MOV|O_WORD,"mov:g.w",2,{RNIND_D16,RD},4,	{{0xf8,0xf8,RN },{0x00,0x00,DISP16 },{0x00,0x00,0 },{0x80,0xf8,RD }}},
{93,'m','I','!','E',O_MOV|O_WORD,"mov:g.w",2,{IMM16,RNIND_D8},5,{{0xe8,0xf8,RN },{0x00,0x00,DISP8 },{0x07,0xff,0 },{0x00,0x00,IMM16 },{0x00,0x00,0 }}},
{93,'m','I','!','E',O_MOV|O_WORD,"mov:g.w",2,{IMM16,ABS8},5,	{{0x0d,0xff,0 },{0x00,0x00,ABS8 },{0x07,0xff,0 },{0x00,0x00,IMM16 },{0x00,0x00,0 }}},
{93,'m','I','!','E',O_MOV|O_WORD,"mov:g.w",2,{IMM16,RNIND_D16},6,{{0xf8,0xf8,RN },{0x00,0x00,DISP16 },{0x00,0x00,0 },{0x07,0xff,0 },{0x00,0x00,IMM16 },{0x00,0x00,0 }}},

{93,'m','I','!','E',O_MOV|O_WORD,"mov:g.w",2,{IMM16,ABS16},6,	{{0x1d,0xff,0 },{0x00,0x00,ABS16 },{0x00,0x00,0 },{0x07,0xff,0 },{0x00,0x00,IMM16 },{0x00,0x00,0 }}},

{94,'m','S','!','E',O_MOV|O_BYTE,"mov:g.b",2,{RS,RNINC},2,	{{0xc0,0xf8,RN },{0x90,0xf8,RS }}},
{94,'m','E','!','D',O_MOV|O_BYTE,"mov:g.b",2,{RNIND,RD},2,	{{0xd0,0xf8,RN },{0x80,0xf8,RD }}},
{94,'m','S','!','E',O_MOV|O_BYTE,"mov:g.b",2,{RS,RNIND},2,	{{0xd0,0xf8,RN },{0x90,0xf8,RS }}},
{94,'m','E','!','D',O_MOV|O_BYTE,"mov:g.b",2,{RNDEC,RD},2,	{{0xb0,0xf8,RN },{0x80,0xf8,RD }}},
{94,'m','S','!','E',O_MOV|O_BYTE,"mov:g.b",2,{RS,RNDEC},2,	{{0xb0,0xf8,RN },{0x90,0xf8,RS }}},
{94,'m','E','!','D',O_MOV|O_BYTE,"mov:g.b",2,{RN,RD},2,	{{0xa0,0xf8,RN },{0x80,0xf8,RD }}},
{94,'m','E','!','D',O_MOV|O_BYTE,"mov:g.b",2,{RNINC,RD},2,	{{0xc0,0xf8,RN },{0x80,0xf8,RD }}},
{94,'m','S','!','E',O_MOV|O_BYTE,"mov:g.b",2,{RS,ABS8},3,	{{0x05,0xff,0 },{0x00,0x00,ABS8 },{0x90,0xf8,RS }}},
{94,'m','S','!','E',O_MOV|O_BYTE,"mov:g.b",2,{RS,RNIND_D8},3,	{{0xe0,0xf8,RN },{0x00,0x00,DISP8 },{0x90,0xf8,RS }}},
{94,'m','E','!','D',O_MOV|O_BYTE,"mov:g.b",2,{IMM8,RD},3,	{{0x04,0xff,0 },{0x00,0x00,IMM8 },{0x80,0xf8,RD }}},
{94,'m','E','!','D',O_MOV|O_BYTE,"mov:g.b",2,{RNIND_D8,RD},3,	{{0xe0,0xf8,RN },{0x00,0x00,DISP8 },{0x80,0xf8,RD }}},
{94,'m','I','!','E',O_MOV|O_BYTE,"mov:g.b",2,{IMM8,RNIND},3,	{{0xd0,0xf8,RN },{0x06,0xff,0 },{0x00,0x00,IMM8 }}},
{94,'m','I','!','E',O_MOV|O_BYTE,"mov:g.b",2,{IMM8,RNINC},3,	{{0xc0,0xf8,RN },{0x06,0xff,0 },{0x00,0x00,IMM8 }}},
{94,'m','I','!','E',O_MOV|O_BYTE,"mov:g.b",2,{IMM8,RNDEC},3,	{{0xb0,0xf8,RN },{0x06,0xff,0 },{0x00,0x00,IMM8 }}},
{94,'m','E','!','D',O_MOV|O_BYTE,"mov:g.b",2,{ABS8,RD},3,	{{0x05,0xff,0 },{0x00,0x00,ABS8 },{0x80,0xf8,RD }}},
{94,'m','I','!','E',O_MOV|O_BYTE,"mov:g.b",2,{IMM8,ABS8},4,	{{0x05,0xff,0 },{0x00,0x00,ABS8 },{0x06,0xff,0 },{0x00,0x00,IMM8 }}},
{94,'m','I','!','E',O_MOV|O_BYTE,"mov:g.b",2,{IMM8,RNIND_D8},4,	{{0xe0,0xf8,RN },{0x00,0x00,DISP8 },{0x06,0xff,0 },{0x00,0x00,IMM8 }}},
{94,'m','E','!','D',O_MOV|O_BYTE,"mov:g.b",2,{ABS16,RD},4,	{{0x15,0xff,0 },{0x00,0x00,ABS16 },{0x00,0x00,0 },{0x80,0xf8,RD }}},
{94,'m','S','!','E',O_MOV|O_BYTE,"mov:g.b",2,{RS,RNIND_D16},4,	{{0xf0,0xf8,RN },{0x00,0x00,DISP16 },{0x00,0x00,0 },{0x90,0xf8,RS }}},
{94,'m','S','!','E',O_MOV|O_BYTE,"mov:g.b",2,{RS,ABS16},4,	{{0x15,0xff,0 },{0x00,0x00,ABS16 },{0x00,0x00,0 },{0x90,0xf8,RS }}},
{94,'m','E','!','D',O_MOV|O_BYTE,"mov:g.b",2,{RNIND_D16,RD},4,	{{0xf0,0xf8,RN },{0x00,0x00,DISP16 },{0x00,0x00,0 },{0x80,0xf8,RD }}},
{94,'m','I','!','E',O_MOV|O_BYTE,"mov:g.b",2,{IMM8,RNIND_D16},5,	{{0xf0,0xf8,RN },{0x00,0x00,DISP16 },{0x00,0x00,0 },{0x06,0xff,0 },{0x00,0x00,IMM8 }}},
{94,'m','I','!','E',O_MOV|O_BYTE,"mov:g.b",2,{IMM8,ABS16},5,	{{0x15,0xff,0 },{0x00,0x00,ABS16 },{0x00,0x00,0 },{0x06,0xff,0 },{0x00,0x00,IMM8 }}},
{95,'m','S','!','E',O_MOV|O_UNSZ,"mov:g",2,{RS,RNINC},2,	{{0xc8,0xf8,RN },{0x90,0xf8,RS }}},
{95,'m','E','!','D',O_MOV|O_UNSZ,"mov:g",2,{RN,RD},2,	{{0xa8,0xf8,RN },{0x80,0xf8,RD }}},
{95,'m','S','!','E',O_MOV|O_UNSZ,"mov:g",2,{RS,RNIND},2,	{{0xd8,0xf8,RN },{0x90,0xf8,RS }}},
{95,'m','E','!','D',O_MOV|O_UNSZ,"mov:g",2,{RNIND,RD},2,	{{0xd8,0xf8,RN },{0x80,0xf8,RD }}},
{95,'m','S','!','E',O_MOV|O_UNSZ,"mov:g",2,{RS,RNDEC},2,	{{0xb8,0xf8,RN },{0x90,0xf8,RS }}},
{95,'m','E','!','D',O_MOV|O_UNSZ,"mov:g",2,{RNINC,RD},2,	{{0xc8,0xf8,RN },{0x80,0xf8,RD }}},
{95,'m','E','!','D',O_MOV|O_UNSZ,"mov:g",2,{RNDEC,RD},2,	{{0xb8,0xf8,RN },{0x80,0xf8,RD }}},
{95,'m','S','!','E',O_MOV|O_UNSZ,"mov:g",2,{RS,RNIND_D8},3,	{{0xe8,0xf8,RN },{0x00,0x00,DISP8 },{0x90,0xf8,RS }}},
{95,'m','S','!','E',O_MOV|O_UNSZ,"mov:g",2,{RS,ABS8},3,	{{0x0d,0xff,0 },{0x00,0x00,ABS8 },{0x90,0xf8,RS }}},
{95,'m','E','!','D',O_MOV|O_UNSZ,"mov:g",2,{RNIND_D8,RD},3,	{{0xe8,0xf8,RN },{0x00,0x00,DISP8 },{0x80,0xf8,RD }}},
{95,'m','I','!','E',O_MOV|O_UNSZ,"mov:g",2,{IMM8,RNIND},3,	{{0xd8,0xf8,RN },{0x06,0xff,0 },{0x00,0x00,IMM8 }}},
{95,'m','E','!','D',O_MOV|O_UNSZ,"mov:g",2,{ABS8,RD},3,	{{0x0d,0xff,0 },{0x00,0x00,ABS8 },{0x80,0xf8,RD }}},
{95,'m','I','!','E',O_MOV|O_UNSZ,"mov:g",2,{IMM8,RNDEC},3,	{{0xb8,0xf8,RN },{0x06,0xff,0 },{0x00,0x00,IMM8 }}},
{95,'m','I','!','E',O_MOV|O_UNSZ,"mov:g",2,{IMM8,RNINC},3,	{{0xc8,0xf8,RN },{0x06,0xff,0 },{0x00,0x00,IMM8 }}},
{95,'m','I','!','E',O_MOV|O_UNSZ,"mov:g",2,{IMM8,RNIND_D8},4,	{{0xe8,0xf8,RN },{0x00,0x00,DISP8 },{0x06,0xff,0 },{0x00,0x00,IMM8 }}},
{95,'m','I','!','E',O_MOV|O_UNSZ,"mov:g",2,{IMM8,ABS8},4,	{{0x0d,0xff,0 },{0x00,0x00,ABS8 },{0x06,0xff,0 },{0x00,0x00,IMM8 }}},
{95,'m','S','!','E',O_MOV|O_UNSZ,"mov:g",2,{RS,RNIND_D16},4,	{{0xf8,0xf8,RN },{0x00,0x00,DISP16 },{0x00,0x00,0 },{0x90,0xf8,RS }}},
{95,'m','E','!','D',O_MOV|O_UNSZ,"mov:g",2,{IMM16,RD},4,	{{0x0c,0xff,0 },{0x00,0x00,IMM16 },{0x00,0x00,0 },{0x80,0xf8,RD }}},
{95,'m','I','!','E',O_MOV|O_UNSZ,"mov:g",2,{IMM16,RNIND},4,	{{0xd8,0xf8,RN },{0x07,0xff,0 },{0x00,0x00,IMM16 },{0x00,0x00,0 }}},
{95,'m','I','!','E',O_MOV|O_UNSZ,"mov:g",2,{IMM16,RNINC},4,	{{0xc8,0xf8,RN },{0x07,0xff,0 },{0x00,0x00,IMM16 },{0x00,0x00,0 }}},
{95,'m','I','!','E',O_MOV|O_UNSZ,"mov:g",2,{IMM16,RNDEC},4,	{{0xb8,0xf8,RN },{0x07,0xff,0 },{0x00,0x00,IMM16 },{0x00,0x00,0 }}},
{95,'m','S','!','E',O_MOV|O_UNSZ,"mov:g",2,{RS,ABS16},4,	{{0x1d,0xff,0 },{0x00,0x00,ABS16 },{0x00,0x00,0 },{0x90,0xf8,RS }}},
{95,'m','E','!','D',O_MOV|O_UNSZ,"mov:g",2,{RNIND_D16,RD},4,	{{0xf8,0xf8,RN },{0x00,0x00,DISP16 },{0x00,0x00,0 },{0x80,0xf8,RD }}},
{95,'m','E','!','D',O_MOV|O_UNSZ,"mov:g",2,{ABS16,RD},4,	{{0x1d,0xff,0 },{0x00,0x00,ABS16 },{0x00,0x00,0 },{0x80,0xf8,RD }}},
{95,'m','I','!','E',O_MOV|O_UNSZ,"mov:g",2,{IMM16,RNIND_D8},5,	{{0xe8,0xf8,RN },{0x00,0x00,DISP8 },{0x07,0xff,0 },{0x00,0x00,IMM16 },{0x00,0x00,0 }}},
{95,'m','I','!','E',O_MOV|O_UNSZ,"mov:g",2,{IMM8,ABS16},5,	{{0x1d,0xff,0 },{0x00,0x00,ABS16 },{0x00,0x00,0 },{0x06,0xff,0 },{0x00,0x00,IMM8 }}},
{95,'m','I','!','E',O_MOV|O_UNSZ,"mov:g",2,{IMM16,ABS8},5,	{{0x0d,0xff,0 },{0x00,0x00,ABS8 },{0x07,0xff,0 },{0x00,0x00,IMM16 },{0x00,0x00,0 }}},
{95,'m','I','!','E',O_MOV|O_UNSZ,"mov:g",2,{IMM8,RNIND_D16},5,	{{0xf8,0xf8,RN },{0x00,0x00,DISP16 },{0x00,0x00,0 },{0x06,0xff,0 },{0x00,0x00,IMM8 }}},
{95,'m','I','!','E',O_MOV|O_UNSZ,"mov:g",2,{IMM16,ABS16},6,	{{0x1d,0xff,0 },{0x00,0x00,ABS16 },{0x00,0x00,0 },{0x07,0xff,0 },{0x00,0x00,IMM16 },{0x00,0x00,0 }}},
{95,'m','I','!','E',O_MOV|O_UNSZ,"mov:g",2,{IMM16,RNIND_D16},6,	{{0xf8,0xf8,RN },{0x00,0x00,DISP16 },{0x00,0x00,0 },{0x07,0xff,0 },{0x00,0x00,IMM16 },{0x00,0x00,0 }}},
{96,'m','S','!','E',O_MOV|O_WORD,"mov:f.w",2,{RS,FPIND_D8},2,	{{0x98,0xf8,RS },{0x00,0x00,FPIND_D8 }}},
{96,'m','E','!','D',O_MOV|O_WORD,"mov:f.w",2,{FPIND_D8,RD},2,	{{0x88,0xf8,RD },{0x00,0x00,FPIND_D8 }}},
{97,'m','S','!','E',O_MOV|O_BYTE,"mov:f.b",2,{RS,FPIND_D8},2,	{{0x90,0xf8,RS },{0x00,0x00,FPIND_D8 }}},
{97,'m','E','!','D',O_MOV|O_BYTE,"mov:f.b",2,{FPIND_D8,RD},2,	{{0x80,0xf8,RD },{0x00,0x00,FPIND_D8 }}},
{98,'m','S','!','E',O_MOV|O_UNSZ,"mov:f",2,{RS,FPIND_D8},2,	{{0x98,0xf8,RS },{0x00,0x00,FPIND_D8 }}},
{98,'m','E','!','D',O_MOV|O_UNSZ,"mov:f",2,{FPIND_D8,RD},2,	{{0x88,0xf8,RD },{0x00,0x00,FPIND_D8 }}},
{99,'m','I','!','D',O_MOV|O_BYTE,"mov:e.b",2,{IMM8,RD},2,	{{0x50,0xf8,RD },{0x00,0x00,IMM8 }}},
{100,'m','I','!','D',O_MOV|O_UNSZ,"mov:e",2,{IMM8,RD},2,	{{0x50,0xf8,RD },{0x00,0x00,IMM8 }}},
{101,'m','S','!','E',O_MOV|O_WORD,"mov.w",2,{RS,FPIND_D8},2,	{{0x98,0xf8,RS },{0x00,0x00,FPIND_D8 }}},
{101,'m','S','!','E',O_MOV|O_WORD,"mov.w",2,{RS,ABS8},2,	{{0x78,0xf8,RS },{0x00,0x00,ABS8 }}},
{101,'m','E','!','D',O_MOV|O_WORD,"mov.w",2,{ABS8,RD},2,	{{0x68,0xf8,RD },{0x00,0x00,ABS8 }}},
{101,'m','S','!','E',O_MOV|O_WORD,"mov.w",2,{RS,RNIND},2,	{{0xd8,0xf8,RN },{0x90,0xf8,RS }}},
{101,'m','S','!','E',O_MOV|O_WORD,"mov.w",2,{RS,RNINC},2,	{{0xc8,0xf8,RN },{0x90,0xf8,RS }}},
{101,'m','S','!','E',O_MOV|O_WORD,"mov.w",2,{RS,RNDEC},2,	{{0xb8,0xf8,RN },{0x90,0xf8,RS }}},
{101,'m','E','!','D',O_MOV|O_WORD,"mov.w",2,{RNIND,RD},2,	{{0xd8,0xf8,RN },{0x80,0xf8,RD }}},
{101,'m','E','!','D',O_MOV|O_WORD,"mov.w",2,{FPIND_D8,RD},2,	{{0x88,0xf8,RD },{0x00,0x00,FPIND_D8 }}},
{101,'m','E','!','D',O_MOV|O_WORD,"mov.w",2,{RNINC,RD},2,	{{0xc8,0xf8,RN },{0x80,0xf8,RD }}},
{101,'m','E','!','D',O_MOV|O_WORD,"mov.w",2,{RN,RD},2,	{{0xa8,0xf8,RN },{0x80,0xf8,RD }}},
{101,'m','E','!','D',O_MOV|O_WORD,"mov.w",2,{RNDEC,RD},2,	{{0xb8,0xf8,RN },{0x80,0xf8,RD }}},
{101,'m','S','!','E',O_MOV|O_WORD,"mov.w",2,{RS,ABS8},3,	{{0x0d,0xff,0 },{0x00,0x00,ABS8 },{0x90,0xf8,RS }}},
{101,'m','E','!','D',O_MOV|O_WORD,"mov.w",2,{RNIND_D8,RD},3,	{{0xe8,0xf8,RN },{0x00,0x00,DISP8 },{0x80,0xf8,RD }}},
{101,'m','S','!','E',O_MOV|O_WORD,"mov.w",2,{RS,RNIND_D8},3,	{{0xe8,0xf8,RN },{0x00,0x00,DISP8 },{0x90,0xf8,RS }}},
{101,'m','I','!','D',O_MOV|O_WORD,"mov.w",2,{IMM16,RD},3,	{{0x58,0xf8,RD },{0x00,0x00,IMM16 },{0x00,0x00,0 }}},
{101,'m','E','!','D',O_MOV|O_WORD,"mov.w",2,{ABS8,RD},3,	{{0x0d,0xff,0 },{0x00,0x00,ABS8 },{0x80,0xf8,RD }}},
{101,'m','I','!','E',O_MOV|O_WORD,"mov.w",2,{IMM16,RNINC},4,	{{0xc8,0xf8,RN },{0x07,0xff,0 },{0x00,0x00,IMM16 },{0x00,0x00,0 }}},
{101,'m','I','!','E',O_MOV|O_WORD,"mov.w",2,{IMM16,RNDEC},4,	{{0xb8,0xf8,RN },{0x07,0xff,0 },{0x00,0x00,IMM16 },{0x00,0x00,0 }}},
{101,'m','I','!','E',O_MOV|O_WORD,"mov.w",2,{IMM16,RNIND},4,	{{0xd8,0xf8,RN },{0x07,0xff,0 },{0x00,0x00,IMM16 },{0x00,0x00,0 }}},
{101,'m','S','!','E',O_MOV|O_WORD,"mov.w",2,{RS,RNIND_D16},4,	{{0xf8,0xf8,RN },{0x00,0x00,DISP16 },{0x00,0x00,0 },{0x90,0xf8,RS }}},
{101,'m','S','!','E',O_MOV|O_WORD,"mov.w",2,{RS,ABS16},4,	{{0x1d,0xff,0 },{0x00,0x00,ABS16 },{0x00,0x00,0 },{0x90,0xf8,RS }}},
{101,'m','E','!','D',O_MOV|O_WORD,"mov.w",2,{ABS16,RD},4,	{{0x1d,0xff,0 },{0x00,0x00,ABS16 },{0x00,0x00,0 },{0x80,0xf8,RD }}},
{101,'m','E','!','D',O_MOV|O_WORD,"mov.w",2,{IMM16,RD},4,	{{0x0c,0xff,0 },{0x00,0x00,IMM16 },{0x00,0x00,0 },{0x80,0xf8,RD }}},
{101,'m','E','!','D',O_MOV|O_WORD,"mov.w",2,{RNIND_D16,RD},4,	{{0xf8,0xf8,RN },{0x00,0x00,DISP16 },{0x00,0x00,0 },{0x80,0xf8,RD }}},
{101,'m','I','!','E',O_MOV|O_WORD,"mov.w",2,{IMM16,RNIND_D8},5,	{{0xe8,0xf8,RN },{0x00,0x00,DISP8 },{0x07,0xff,0 },{0x00,0x00,IMM16 },{0x00,0x00,0 }}},
{101,'m','I','!','E',O_MOV|O_WORD,"mov.w",2,{IMM16,ABS8},5,	{{0x0d,0xff,0 },{0x00,0x00,ABS8 },{0x07,0xff,0 },{0x00,0x00,IMM16 },{0x00,0x00,0 }}},
{101,'m','I','!','E',O_MOV|O_WORD,"mov.w",2,{IMM16,RNIND_D16},6,	{{0xf8,0xf8,RN },{0x00,0x00,DISP16 },{0x00,0x00,0 },{0x07,0xff,0 },{0x00,0x00,IMM16 },{0x00,0x00,0 }}},
{101,'m','I','!','E',O_MOV|O_WORD,"mov.w",2,{IMM16,ABS16},6,	{{0x1d,0xff,0 },{0x00,0x00,ABS16 },{0x00,0x00,0 },{0x07,0xff,0 },{0x00,0x00,IMM16 },{0x00,0x00,0 }}},

{102,'m','E','!','D',O_MOV|O_BYTE,"mov.b",2,{FPIND_D8,RD},2,	{{0x80,0xf8,RD },{0x00,0x00,FPIND_D8 }}},
{102,'m','S','!','E',O_MOV|O_BYTE,"mov.b",2,{RS,ABS8},2,	{{0x70,0xf8,RS },{0x00,0x00,ABS8 }}},
{102,'m','E','!','D',O_MOV|O_BYTE,"mov.b",2,{RNINC,RD},2,	{{0xc0,0xf8,RN },{0x80,0xf8,RD }}},
{102,'m','S','!','E',O_MOV|O_BYTE,"mov.b",2,{RS,RNIND},2,	{{0xd0,0xf8,RN },{0x90,0xf8,RS }}},
{102,'m','S','!','E',O_MOV|O_BYTE,"mov.b",2,{RS,RNINC},2,	{{0xc0,0xf8,RN },{0x90,0xf8,RS }}},
{102,'m','S','!','E',O_MOV|O_BYTE,"mov.b",2,{RS,RNDEC},2,	{{0xb0,0xf8,RN },{0x90,0xf8,RS }}},
{102,'m','E','!','D',O_MOV|O_BYTE,"mov.b",2,{RNDEC,RD},2,	{{0xb0,0xf8,RN },{0x80,0xf8,RD }}},
{102,'m','S','!','E',O_MOV|O_BYTE,"mov.b",2,{RS,FPIND_D8},2,	{{0x90,0xf8,RS },{0x00,0x00,FPIND_D8 }}},
{102,'m','E','!','D',O_MOV|O_BYTE,"mov.b",2,{RNIND,RD},2,	{{0xd0,0xf8,RN },{0x80,0xf8,RD }}},
{102,'m','E','!','D',O_MOV|O_BYTE,"mov.b",2,{RN,RD},2,	{{0xa0,0xf8,RN },{0x80,0xf8,RD }}},
{102,'m','E','!','D',O_MOV|O_BYTE,"mov.b",2,{ABS8,RD},2,	{{0x60,0xf8,RD },{0x00,0x00,ABS8 }}},
{102,'m','I','!','D',O_MOV|O_BYTE,"mov.b",2,{IMM8,RD},2,	{{0x50,0xf8,RD },{0x00,0x00,IMM8 }}},
{102,'m','E','!','D',O_MOV|O_BYTE,"mov.b",2,{IMM8,RD},3,	{{0x04,0xff,0 },{0x00,0x00,IMM8 },{0x80,0xf8,RD }}},
{102,'m','S','!','E',O_MOV|O_BYTE,"mov.b",2,{RS,ABS8},3,	{{0x05,0xff,0 },{0x00,0x00,ABS8 },{0x90,0xf8,RS }}},
{102,'m','I','!','E',O_MOV|O_BYTE,"mov.b",2,{IMM8,RNIND},3,	{{0xd0,0xf8,RN },{0x06,0xff,0 },{0x00,0x00,IMM8 }}},
{102,'m','I','!','E',O_MOV|O_BYTE,"mov.b",2,{IMM8,RNINC},3,	{{0xc0,0xf8,RN },{0x06,0xff,0 },{0x00,0x00,IMM8 }}},
{102,'m','I','!','E',O_MOV|O_BYTE,"mov.b",2,{IMM8,RNDEC},3,	{{0xb0,0xf8,RN },{0x06,0xff,0 },{0x00,0x00,IMM8 }}},
{102,'m','E','!','D',O_MOV|O_BYTE,"mov.b",2,{RNIND_D8,RD},3,	{{0xe0,0xf8,RN },{0x00,0x00,DISP8 },{0x80,0xf8,RD }}},
{102,'m','E','!','D',O_MOV|O_BYTE,"mov.b",2,{ABS8,RD},3,	{{0x05,0xff,0 },{0x00,0x00,ABS8 },{0x80,0xf8,RD }}},
{102,'m','S','!','E',O_MOV|O_BYTE,"mov.b",2,{RS,RNIND_D8},3,	{{0xe0,0xf8,RN },{0x00,0x00,DISP8 },{0x90,0xf8,RS }}},
{102,'m','I','!','E',O_MOV|O_BYTE,"mov.b",2,{IMM8,RNIND_D8},4,	{{0xe0,0xf8,RN },{0x00,0x00,DISP8 },{0x06,0xff,0 },{0x00,0x00,IMM8 }}},
{102,'m','E','!','D',O_MOV|O_BYTE,"mov.b",2,{ABS16,RD},4,	{{0x15,0xff,0 },{0x00,0x00,ABS16 },{0x00,0x00,0 },{0x80,0xf8,RD }}},
{102,'m','I','!','E',O_MOV|O_BYTE,"mov.b",2,{IMM8,ABS8},4,	{{0x05,0xff,0 },{0x00,0x00,ABS8 },{0x06,0xff,0 },{0x00,0x00,IMM8 }}},
{102,'m','S','!','E',O_MOV|O_BYTE,"mov.b",2,{RS,ABS16},4,	{{0x15,0xff,0 },{0x00,0x00,ABS16 },{0x00,0x00,0 },{0x90,0xf8,RS }}},
{102,'m','S','!','E',O_MOV|O_BYTE,"mov.b",2,{RS,RNIND_D16},4,	{{0xf0,0xf8,RN },{0x00,0x00,DISP16 },{0x00,0x00,0 },{0x90,0xf8,RS }}},
{102,'m','E','!','D',O_MOV|O_BYTE,"mov.b",2,{RNIND_D16,RD},4,	{{0xf0,0xf8,RN },{0x00,0x00,DISP16 },{0x00,0x00,0 },{0x80,0xf8,RD }}},
{102,'m','I','!','E',O_MOV|O_BYTE,"mov.b",2,{IMM8,ABS16},5,	{{0x15,0xff,0 },{0x00,0x00,ABS16 },{0x00,0x00,0 },{0x06,0xff,0 },{0x00,0x00,IMM8 }}},
{102,'m','I','!','E',O_MOV|O_BYTE,"mov.b",2,{IMM8,RNIND_D16},5,	{{0xf0,0xf8,RN },{0x00,0x00,DISP16 },{0x00,0x00,0 },{0x06,0xff,0 },{0x00,0x00,IMM8 }}},
{103,'m','E','!','D',O_MOV|O_UNSZ,"mov",2,{ABS8,RD},2,	{{0x68,0xf8,RD },{0x00,0x00,ABS8 }}},
{103,'m','S','!','E',O_MOV|O_UNSZ,"mov",2,{RS,ABS8},2,	{{0x78,0xf8,RS },{0x00,0x00,ABS8 }}},
{103,'m','E','!','D',O_MOV|O_UNSZ,"mov",2,{RNIND,RD},2,	{{0xd8,0xf8,RN },{0x80,0xf8,RD }}},
{103,'m','S','!','E',O_MOV|O_UNSZ,"mov",2,{RS,RNIND},2,	{{0xd8,0xf8,RN },{0x90,0xf8,RS }}},
{103,'m','S','!','E',O_MOV|O_UNSZ,"mov",2,{RS,RNINC},2,	{{0xc8,0xf8,RN },{0x90,0xf8,RS }}},
{103,'m','S','!','E',O_MOV|O_UNSZ,"mov",2,{RS,RNDEC},2,	{{0xb8,0xf8,RN },{0x90,0xf8,RS }}},
{103,'m','E','!','D',O_MOV|O_UNSZ,"mov",2,{RN,RD},2,	{{0xa8,0xf8,RN },{0x80,0xf8,RD }}},
{103,'m','S','!','E',O_MOV|O_UNSZ,"mov",2,{RS,FPIND_D8},2,	{{0x98,0xf8,RS },{0x00,0x00,FPIND_D8 }}},
{103,'m','E','!','D',O_MOV|O_UNSZ,"mov",2,{RNINC,RD},2,	{{0xc8,0xf8,RN },{0x80,0xf8,RD }}},
{103,'m','E','!','D',O_MOV|O_UNSZ,"mov",2,{FPIND_D8,RD},2,	{{0x88,0xf8,RD },{0x00,0x00,FPIND_D8 }}},
/*{103,'m','I','!','D',O_MOV|O_UNSZ,"mov",2,{IMM8,RD},2,	{{0x58,0xf8,RD },{0x00,0x00,IMM8 }}},*/
{103,'m','E','!','D',O_MOV|O_UNSZ,"mov",2,{RNDEC,RD},2,	{{0xb8,0xf8,RN },{0x80,0xf8,RD }}},
{103,'m','S','!','E',O_MOV|O_UNSZ,"mov",2,{RS,RNIND_D8},3,	{{0xe8,0xf8,RN },{0x00,0x00,DISP8 },{0x90,0xf8,RS }}},
{103,'m','I','!','E',O_MOV|O_UNSZ,"mov",2,{IMM8,RNIND},3,	{{0xd8,0xf8,RN },{0x06,0xff,0 },{0x00,0x00,IMM8 }}},
{103,'m','I','!','E',O_MOV|O_UNSZ,"mov",2,{IMM8,RNINC},3,	{{0xc8,0xf8,RN },{0x06,0xff,0 },{0x00,0x00,IMM8 }}},
{103,'m','I','!','E',O_MOV|O_UNSZ,"mov",2,{IMM8,RNDEC},3,	{{0xb8,0xf8,RN },{0x06,0xff,0 },{0x00,0x00,IMM8 }}},
{103,'m','E','!','D',O_MOV|O_UNSZ,"mov",2,{RNIND_D8,RD},3,	{{0xe8,0xf8,RN },{0x00,0x00,DISP8 },{0x80,0xf8,RD }}},
{103,'m','S','!','E',O_MOV|O_UNSZ,"mov",2,{RS,ABS8},3,	{{0x0d,0xff,0 },{0x00,0x00,ABS8 },{0x90,0xf8,RS }}},
{103,'m','E','!','D',O_MOV|O_UNSZ,"mov",2,{ABS8,RD},3,	{{0x0d,0xff,0 },{0x00,0x00,ABS8 },{0x80,0xf8,RD }}},
{103,'m','I','!','D',O_MOV|O_UNSZ,"mov",2,{IMM16,RD},3,	{{0x58,0xf8,RD },{0x00,0x00,IMM16 },{0x00,0x00,0 }}},
{103,'m','I','!','E',O_MOV|O_UNSZ,"mov",2,{IMM8,ABS8},4,	{{0x0d,0xff,0 },{0x00,0x00,ABS8 },{0x06,0xff,0 },{0x00,0x00,IMM8 }}},
{103,'m','S','!','E',O_MOV|O_UNSZ,"mov",2,{RS,RNIND_D16},4,	{{0xf8,0xf8,RN },{0x00,0x00,DISP16 },{0x00,0x00,0 },{0x90,0xf8,RS }}},
{103,'m','I','!','E',O_MOV|O_UNSZ,"mov",2,{IMM16,RNIND},4,	{{0xd8,0xf8,RN },{0x07,0xff,0 },{0x00,0x00,IMM16 },{0x00,0x00,0 }}},
{103,'m','I','!','E',O_MOV|O_UNSZ,"mov",2,{IMM16,RNINC},4,	{{0xc8,0xf8,RN },{0x07,0xff,0 },{0x00,0x00,IMM16 },{0x00,0x00,0 }}},
{103,'m','I','!','E',O_MOV|O_UNSZ,"mov",2,{IMM16,RNDEC},4,	{{0xb8,0xf8,RN },{0x07,0xff,0 },{0x00,0x00,IMM16 },{0x00,0x00,0 }}},
{103,'m','S','!','E',O_MOV|O_UNSZ,"mov",2,{RS,ABS16},4,	{{0x1d,0xff,0 },{0x00,0x00,ABS16 },{0x00,0x00,0 },{0x90,0xf8,RS }}},
{103,'m','E','!','D',O_MOV|O_UNSZ,"mov",2,{IMM16,RD},4,	{{0x0c,0xff,0 },{0x00,0x00,IMM16 },{0x00,0x00,0 },{0x80,0xf8,RD }}},
{103,'m','I','!','E',O_MOV|O_UNSZ,"mov",2,{IMM8,RNIND_D8},4,	{{0xe8,0xf8,RN },{0x00,0x00,DISP8 },{0x06,0xff,0 },{0x00,0x00,IMM8 }}},
{103,'m','E','!','D',O_MOV|O_UNSZ,"mov",2,{RNIND_D16,RD},4,	{{0xf8,0xf8,RN },{0x00,0x00,DISP16 },{0x00,0x00,0 },{0x80,0xf8,RD }}},
{103,'m','E','!','D',O_MOV|O_UNSZ,"mov",2,{ABS16,RD},4,	{{0x1d,0xff,0 },{0x00,0x00,ABS16 },{0x00,0x00,0 },{0x80,0xf8,RD }}},
{103,'m','I','!','E',O_MOV|O_UNSZ,"mov",2,{IMM16,RNIND_D8},5,	{{0xe8,0xf8,RN },{0x00,0x00,DISP8 },{0x07,0xff,0 },{0x00,0x00,IMM16 },{0x00,0x00,0 }}},
{103,'m','I','!','E',O_MOV|O_UNSZ,"mov",2,{IMM8,ABS16},5,	{{0x1d,0xff,0 },{0x00,0x00,ABS16 },{0x00,0x00,0 },{0x06,0xff,0 },{0x00,0x00,IMM8 }}},
{103,'m','I','!','E',O_MOV|O_UNSZ,"mov",2,{IMM16,ABS8},5,	{{0x0d,0xff,0 },{0x00,0x00,ABS8 },{0x07,0xff,0 },{0x00,0x00,IMM16 },{0x00,0x00,0 }}},
{103,'m','I','!','E',O_MOV|O_UNSZ,"mov",2,{IMM8,RNIND_D16},5,	{{0xf8,0xf8,RN },{0x00,0x00,DISP16 },{0x00,0x00,0 },{0x06,0xff,0 },{0x00,0x00,IMM8 }}},
{103,'m','I','!','E',O_MOV|O_UNSZ,"mov",2,{IMM16,ABS16},6,	{{0x1d,0xff,0 },{0x00,0x00,ABS16 },{0x00,0x00,0 },{0x07,0xff,0 },{0x00,0x00,IMM16 },{0x00,0x00,0 }}},
{103,'m','I','!','E',O_MOV|O_UNSZ,"mov",2,{IMM16,RNIND_D16},6,	{{0xf8,0xf8,RN },{0x00,0x00,DISP16 },{0x00,0x00,0 },{0x07,0xff,0 },{0x00,0x00,IMM16 },{0x00,0x00,0 }}},
{104,'-','S','I','!',O_LINK|O_UNSZ,"link",2,{FP,IMM8},2,	{{0x17,0xff,0 },{0x00,0x00,IMM8 }}},
{104,'-','S','I','!',O_LINK|O_UNSZ,"link",2,{FP,IMM16},3,	{{0x1f,0xff,0 },{0x00,0x00,IMM16 },{0x00,0x00,0 }}},
{105,'-','E','!','C',O_LDM|O_UNSZ,"ldm",2,{SPINC,RLIST},2,	{{0x02,0xff,0 },{0x00,0x00,RLIST }}},
{106,'s','E','!','C',O_LDC|O_WORD,"ldc.w",2,{RN,CRW},2,		{{0xa8,0xf8,RN },{0x88,0xf8,CRW }}},
{106,'s','E','!','C',O_LDC|O_WORD,"ldc.w",2,{RNIND,CRW},2,	{{0xd8,0xf8,RN },{0x88,0xf8,CRW }}},
{106,'s','E','!','C',O_LDC|O_WORD,"ldc.w",2,{RNINC,CRW},2,	{{0xc8,0xf8,RN },{0x88,0xf8,CRW }}},
{106,'s','E','!','C',O_LDC|O_WORD,"ldc.w",2,{RNDEC,CRW},2,	{{0xb8,0xf8,RN },{0x88,0xf8,CRW }}},
{106,'s','E','!','C',O_LDC|O_WORD,"ldc.w",2,{ABS8,CRW},3,	{{0x0d,0xff,0 },{0x00,0x00,ABS8 },{0x88,0xf8,CRW }}},
{106,'s','E','!','C',O_LDC|O_WORD,"ldc.w",2,{RNIND_D8,CRW},3,	{{0xe8,0xf8,RN },{0x00,0x00,DISP8 },{0x88,0xf8,CRW }}},
{106,'s','E','!','C',O_LDC|O_WORD,"ldc.w",2,{IMM16,CRW},4,	{{0x0c,0xff,0 },{0x00,0x00,IMM16 },{0x00,0x00,0 },{0x88,0xf8,CRW }}},
{106,'s','E','!','C',O_LDC|O_WORD,"ldc.w",2,{ABS16,CRW},4,	{{0x1d,0xff,0 },{0x00,0x00,ABS16 },{0x00,0x00,0 },{0x88,0xf8,CRW }}},
{106,'s','E','!','C',O_LDC|O_WORD,"ldc.w",2,{RNIND_D16,CRW},4,	{{0xf8,0xf8,RN },{0x00,0x00,DISP16 },{0x00,0x00,0 },{0x88,0xf8,CRW }}},
{107,'s','E','!','C',O_LDC|O_BYTE,"ldc.b",2,{RN,CRB},2,		{{0xa0,0xf8,RN },{0x88,0xf8,CRB }}},
{107,'s','E','!','C',O_LDC|O_BYTE,"ldc.b",2,{RNDEC,CRB},2,	{{0xb0,0xf8,RN },{0x88,0xf8,CRB }}},
{107,'s','E','!','C',O_LDC|O_BYTE,"ldc.b",2,{RNINC,CRB},2,	{{0xc0,0xf8,RN },{0x88,0xf8,CRB }}},
{107,'s','E','!','C',O_LDC|O_BYTE,"ldc.b",2,{RNIND,CRB},2,	{{0xd0,0xf8,RN },{0x88,0xf8,CRB }}},
{107,'s','E','!','C',O_LDC|O_BYTE,"ldc.b",2,{IMM8,CRB},3,	{{0x04,0xff,0 },{0x00,0x00,IMM8 },{0x88,0xf8,CRB }}},
{107,'s','E','!','C',O_LDC|O_BYTE,"ldc.b",2,{ABS8,CRB},3,	{{0x05,0xff,0 },{0x00,0x00,ABS8 },{0x88,0xf8,CRB }}},
{107,'s','E','!','C',O_LDC|O_BYTE,"ldc.b",2,{RNIND_D8,CRB},3,	{{0xe0,0xf8,RN },{0x00,0x00,DISP8 },{0x88,0xf8,CRB }}},
{107,'s','E','!','C',O_LDC|O_BYTE,"ldc.b",2,{ABS16,CRB},4,	{{0x15,0xff,0 },{0x00,0x00,ABS16 },{0x00,0x00,0 },{0x88,0xf8,CRB }}},
{107,'s','E','!','C',O_LDC|O_BYTE,"ldc.b",2,{RNIND_D16,CRB},4,	{{0xf0,0xf8,RN },{0x00,0x00,DISP16 },{0x00,0x00,0 },{0x88,0xf8,CRB }}},
{108,'s','E','!','C',O_LDC|O_UNSZ,"ldc",2,{RN,CRW},2,	{{0xa8,0xf8,RN },{0x88,0xf8,CRW }}},
{108,'s','E','!','C',O_LDC|O_UNSZ,"ldc",2,{RN,CRB},2,	{{0xa0,0xf8,RN },{0x88,0xf8,CRB }}},
{108,'s','E','!','C',O_LDC|O_UNSZ,"ldc",2,{RNINC,CRW},2,	{{0xc8,0xf8,RN },{0x88,0xf8,CRW }}},
{108,'s','E','!','C',O_LDC|O_UNSZ,"ldc",2,{RNIND,CRB},2,	{{0xd0,0xf8,RN },{0x88,0xf8,CRB }}},
{108,'s','E','!','C',O_LDC|O_UNSZ,"ldc",2,{RNDEC,CRW},2,	{{0xb8,0xf8,RN },{0x88,0xf8,CRW }}},
{108,'s','E','!','C',O_LDC|O_UNSZ,"ldc",2,{RNIND,CRW},2,	{{0xd8,0xf8,RN },{0x88,0xf8,CRW }}},
{108,'s','E','!','C',O_LDC|O_UNSZ,"ldc",2,{RNDEC,CRB},2,	{{0xb0,0xf8,RN },{0x88,0xf8,CRB }}},
{108,'s','E','!','C',O_LDC|O_UNSZ,"ldc",2,{RNINC,CRB},2,	{{0xc0,0xf8,RN },{0x88,0xf8,CRB }}},
{108,'s','E','!','C',O_LDC|O_UNSZ,"ldc",2,{ABS8,CRW},3,		{{0x05,0xff,0 },{0x00,0x00,ABS8 },{0x88,0xf8,CRW }}},
{108,'s','E','!','C',O_LDC|O_UNSZ,"ldc",2,{ABS8,CRB},3,		{{0x05,0xff,0 },{0x00,0x00,ABS8 },{0x88,0xf8,CRB }}},
{108,'s','E','!','C',O_LDC|O_UNSZ,"ldc",2,{IMM8,CRB},3,		{{0x04,0xff,0 },{0x00,0x00,IMM8 },{0x88,0xf8,CRB }}},
{108,'s','E','!','C',O_LDC|O_UNSZ,"ldc",2,{RNIND_D8,CRW},3,	{{0xe0,0xf8,RN },{0x00,0x00,DISP8 },{0x88,0xf8,CRW }}},
{108,'s','E','!','C',O_LDC|O_UNSZ,"ldc",2,{RNIND_D8,CRB},3,	{{0xe0,0xf8,RN },{0x00,0x00,DISP8 },{0x88,0xf8,CRB }}},
{108,'s','E','!','C',O_LDC|O_UNSZ,"ldc",2,{ABS16,CRB},4,	{{0x15,0xff,0 },{0x00,0x00,ABS16 },{0x00,0x00,0 },{0x88,0xf8,CRB }}},
{108,'s','E','!','C',O_LDC|O_UNSZ,"ldc",2,{ABS16,CRW},4,	{{0x1d,0xff,0 },{0x00,0x00,ABS16 },{0x00,0x00,0 },{0x88,0xf8,CRW }}},
{108,'s','E','!','C',O_LDC|O_UNSZ,"ldc",2,{IMM16,CRW},4,	{{0x0c,0xff,0 },{0x00,0x00,IMM16 },{0x00,0x00,0 },{0x88,0xf8,CRW }}},
{108,'s','E','!','C',O_LDC|O_UNSZ,"ldc",2,{RNIND_D16,CRW},4,	{{0xf8,0xf8,RN },{0x00,0x00,DISP16 },{0x00,0x00,0 },{0x88,0xf8,CRW }}},
{108,'s','E','!','C',O_LDC|O_UNSZ,"ldc",2,{RNIND_D16,CRB},4,	{{0xf0,0xf8,RN },{0x00,0x00,DISP16 },{0x00,0x00,0 },{0x88,0xf8,CRB }}},
{109,'-','B','!','!',O_JSR|O_UNSZ,"jsr",1,{RDIND,0},2,	{{0x11,0xff,0 },{0xd8,0xf8,RD }}},
{109,'-','B','!','!',O_JSR|O_UNSZ,"jsr",1,{ABS16,0},3,	{{0x18,0xff,0 },{0x00,0x00,ABS16 },{0x00,0x00,0 }}},
{109,'-','B','!','!',O_JSR|O_UNSZ,"jsr",1,{RDIND_D8,0},3,	{{0x11,0xff,0 },{0xe8,0xf8,RDIND_D8 },{0x00,0x00,0 }}},
{109,'-','B','!','!',O_JSR|O_UNSZ,"jsr",1,{RDIND_D16,0},4,	{{0x11,0xff,0 },{0xf8,0xf8,RDIND_D16 },{0x00,0x00,0 },{0x00,0x00,0 }}},
{110,'-','B','!','!',O_JMP|O_UNSZ,"jmp",1,{RDIND,0},2,	{{0x11,0xff,0 },{0xd0,0xf8,RD }}},
{110,'-','B','!','!',O_JMP|O_UNSZ,"jmp",1,{ABS16,0},3,	{{0x10,0xff,0 },{0x00,0x00,ABS16 },{0x00,0x00,0 }}},
{110,'-','B','!','!',O_JMP|O_UNSZ,"jmp",1,{RDIND_D8,0},3,	{{0x11,0xff,0 },{0xe0,0xf8,RDIND_D8 },{0x00,0x00,0 }}},
{110,'-','B','!','!',O_JMP|O_UNSZ,"jmp",1,{RDIND_D16,0},4,	{{0x11,0xff,0 },{0xf0,0xf8,RDIND_D16 },{0x00,0x00,0 },{0x00,0x00,0 }}},
{111,'s','D','!','D',O_EXTU|O_BYTE,"extu.b",1,{RD,0},2,	{{0xa0,0xf8,RD },{0x12,0xff,0 }}},
{112,'s','D','!','D',O_EXTU|O_UNSZ,"extu",1,{RD,0},2,	{{0xa0,0xf8,RD },{0x12,0xff,0 }}},
{113,'s','D','!','D',O_EXTS|O_BYTE,"exts.b",1,{RD,0},2,	{{0xa0,0xf8,RD },{0x11,0xff,0 }}},
{114,'s','D','!','D',O_EXTS|O_UNSZ,"exts",1,{RD,0},2,	{{0xa0,0xf8,RD },{0x11,0xff,0 }}},
{115,'s','D','!','!',O_DSUB|O_UNSZ,"dsub",2,{RS,RD},3,	{{0xa0,0xf8,RS },{0x00,0xff,0 },{0xb0,0xf8,RD }}},
{116,'s','E','D','D',O_DIVXU|O_WORD,"divxu.w",2,{RN,RD},2,	{{0xa8,0xf8,RN },{0xb8,0xf8,RD }}},
{116,'s','E','D','D',O_DIVXU|O_WORD,"divxu.w",2,{RNDEC,RD},2,	{{0xb8,0xf8,RN },{0xb8,0xf8,RD }}},
{116,'s','E','D','D',O_DIVXU|O_WORD,"divxu.w",2,{RNINC,RD},2,	{{0xc8,0xf8,RN },{0xb8,0xf8,RD }}},
{116,'s','E','D','D',O_DIVXU|O_WORD,"divxu.w",2,{RNIND,RD},2,	{{0xd8,0xf8,RN },{0xb8,0xf8,RD }}},
{116,'s','E','D','D',O_DIVXU|O_WORD,"divxu.w",2,{ABS8,RD},3,	{{0x0d,0xff,0 },{0x00,0x00,ABS8 },{0xb8,0xf8,RD }}},
{116,'s','E','D','D',O_DIVXU|O_WORD,"divxu.w",2,{RNIND_D8,RD},3,	{{0xe8,0xf8,RN },{0x00,0x00,DISP8 },{0xb8,0xf8,RD }}},
{116,'s','E','D','D',O_DIVXU|O_WORD,"divxu.w",2,{ABS16,RD},4,	{{0x1d,0xff,0 },{0x00,0x00,ABS16 },{0x00,0x00,0 },{0xb8,0xf8,RD }}},
{116,'s','E','D','D',O_DIVXU|O_WORD,"divxu.w",2,{IMM16,RD},4,	{{0x0c,0xff,0 },{0x00,0x00,IMM16 },{0x00,0x00,0 },{0xb8,0xf8,RD }}},
{116,'s','E','D','D',O_DIVXU|O_WORD,"divxu.w",2,{RNIND_D16,RD},4,	{{0xf8,0xf8,RN },{0x00,0x00,DISP16 },{0x00,0x00,0 },{0xb8,0xf8,RD }}},
{117,'s','E','D','D',O_DIVXU|O_BYTE,"divxu.b",2,{RN,RD},2,	{{0xa0,0xf8,RN },{0xb8,0xf8,RD }}},
{117,'s','E','D','D',O_DIVXU|O_BYTE,"divxu.b",2,{RNDEC,RD},2,	{{0xb0,0xf8,RN },{0xb8,0xf8,RD }}},
{117,'s','E','D','D',O_DIVXU|O_BYTE,"divxu.b",2,{RNIND,RD},2,	{{0xd0,0xf8,RN },{0xb8,0xf8,RD }}},
{117,'s','E','D','D',O_DIVXU|O_BYTE,"divxu.b",2,{RNINC,RD},2,	{{0xc0,0xf8,RN },{0xb8,0xf8,RD }}},
{117,'s','E','D','D',O_DIVXU|O_BYTE,"divxu.b",2,{IMM8,RD},3,	{{0x04,0xff,0 },{0x00,0x00,IMM8 },{0xb8,0xf8,RD }}},
{117,'s','E','D','D',O_DIVXU|O_BYTE,"divxu.b",2,{RNIND_D8,RD},3,	{{0xe0,0xf8,RN },{0x00,0x00,DISP8 },{0xb8,0xf8,RD }}},
{117,'s','E','D','D',O_DIVXU|O_BYTE,"divxu.b",2,{ABS8,RD},3,	{{0x05,0xff,0 },{0x00,0x00,ABS8 },{0xb8,0xf8,RD }}},
{117,'s','E','D','D',O_DIVXU|O_BYTE,"divxu.b",2,{ABS16,RD},4,	{{0x15,0xff,0 },{0x00,0x00,ABS16 },{0x00,0x00,0 },{0xb8,0xf8,RD }}},
{117,'s','E','D','D',O_DIVXU|O_BYTE,"divxu.b",2,{RNIND_D16,RD},4,	{{0xf0,0xf8,RN },{0x00,0x00,DISP16 },{0x00,0x00,0 },{0xb8,0xf8,RD }}},
{118,'s','E','D','D',O_DIVXU|O_UNSZ,"divxu",2,{RN,RD},2,	{{0xa8,0xf8,RN },{0xb8,0xf8,RD }}},
{118,'s','E','D','D',O_DIVXU|O_UNSZ,"divxu",2,{RNINC,RD},2,	{{0xc8,0xf8,RN },{0xb8,0xf8,RD }}},
{118,'s','E','D','D',O_DIVXU|O_UNSZ,"divxu",2,{RNDEC,RD},2,	{{0xb8,0xf8,RN },{0xb8,0xf8,RD }}},
{118,'s','E','D','D',O_DIVXU|O_UNSZ,"divxu",2,{RNIND,RD},2,	{{0xd8,0xf8,RN },{0xb8,0xf8,RD }}},
{118,'s','E','D','D',O_DIVXU|O_UNSZ,"divxu",2,{ABS8,RD},3,	{{0x0d,0xff,0 },{0x00,0x00,ABS8 },{0xb8,0xf8,RD }}},
{118,'s','E','D','D',O_DIVXU|O_UNSZ,"divxu",2,{RNIND_D8,RD},3,	{{0xe8,0xf8,RN },{0x00,0x00,DISP8 },{0xb8,0xf8,RD }}},
{118,'s','E','D','D',O_DIVXU|O_UNSZ,"divxu",2,{IMM16,RD},4,	{{0x0c,0xff,0 },{0x00,0x00,IMM16 },{0x00,0x00,0 },{0xb8,0xf8,RD }}},
{118,'s','E','D','D',O_DIVXU|O_UNSZ,"divxu",2,{ABS16,RD},4,	{{0x1d,0xff,0 },{0x00,0x00,ABS16 },{0x00,0x00,0 },{0xb8,0xf8,RD }}},
{118,'s','E','D','D',O_DIVXU|O_UNSZ,"divxu",2,{RNIND_D16,RD},4,	{{0xf8,0xf8,RN },{0x00,0x00,DISP16 },{0x00,0x00,0 },{0xb8,0xf8,RD }}},
{119,'s','D','!','!',O_DADD|O_UNSZ,"dadd",2,{RS,RD},3,	{{0xa0,0xf8,RS },{0x00,0xff,0 },{0xa0,0xf8,RD }}},
{120,'a','D','I','!',O_CMP|O_WORD,"cmp:i.w",2,{IMM16,RD},3,	{{0x48,0xf8,RD },{0x00,0x00,IMM16 },{0x00,0x00,0 }}},
{121,'a','D','I','!',O_CMP|O_UNSZ,"cmp:i",2,{IMM16,RD},3,	{{0x48,0xf8,RD },{0x00,0x00,IMM16 },{0x00,0x00,0 }}},
{122,'a','D','E','!',O_CMP|O_WORD,"cmp:g.w",2,{RN,RD},2,	{{0xa8,0xf8,RN },{0x70,0xf8,RD }}},
{122,'a','D','E','!',O_CMP|O_WORD,"cmp:g.w",2,{RNIND,RD},2,	{{0xd8,0xf8,RN },{0x70,0xf8,RD }}},
{122,'a','D','E','!',O_CMP|O_WORD,"cmp:g.w",2,{RNINC,RD},2,	{{0xc8,0xf8,RN },{0x70,0xf8,RD }}},
{122,'a','D','E','!',O_CMP|O_WORD,"cmp:g.w",2,{RNDEC,RD},2,	{{0xb8,0xf8,RN },{0x70,0xf8,RD }}},
{122,'a','D','E','!',O_CMP|O_WORD,"cmp:g.w",2,{RNIND_D8,RD},3,	{{0xe8,0xf8,RN },{0x00,0x00,DISP8 },{0x70,0xf8,RD }}},
{122,'a','D','E','!',O_CMP|O_WORD,"cmp:g.w",2,{ABS8,RD},3,	{{0x0d,0xff,0 },{0x00,0x00,ABS8 },{0x70,0xf8,RD }}},
{122,'a','E','I','!',O_CMP|O_WORD,"cmp:g.w",2,{IMM16,RNINC},4,	{{0xc8,0xf8,RN },{0x05,0xff,0 },{0x00,0x00,IMM16 },{0x00,0x00,0 }}},
{122,'a','E','I','!',O_CMP|O_WORD,"cmp:g.w",2,{IMM16,RNDEC},4,	{{0xb8,0xf8,RN },{0x05,0xff,0 },{0x00,0x00,IMM16 },{0x00,0x00,0 }}},
{122,'a','E','I','!',O_CMP|O_WORD,"cmp:g.w",2,{IMM16,RN},4,	{{0xa8,0xf8,RN },{0x05,0xff,0 },{0x00,0x00,IMM16 },{0x00,0x00,0 }}},
{122,'a','E','I','!',O_CMP|O_WORD,"cmp:g.w",2,{IMM16,RNIND},4,	{{0xd8,0xf8,RN },{0x05,0xff,0 },{0x00,0x00,IMM16 },{0x00,0x00,0 }}},
{122,'a','D','E','!',O_CMP|O_WORD,"cmp:g.w",2,{IMM16,RD},4,	{{0x0c,0xff,0 },{0x00,0x00,IMM16 },{0x00,0x00,0 },{0x70,0xf8,RD }}},
{122,'a','D','E','!',O_CMP|O_WORD,"cmp:g.w",2,{ABS16,RD},4,	{{0x1d,0xff,0 },{0x00,0x00,ABS16 },{0x00,0x00,0 },{0x70,0xf8,RD }}},
{122,'a','D','E','!',O_CMP|O_WORD,"cmp:g.w",2,{RNIND_D16,RD},4,	{{0xf8,0xf8,RN },{0x00,0x00,DISP16 },{0x00,0x00,0 },{0x70,0xf8,RD }}},
{122,'a','E','I','!',O_CMP|O_WORD,"cmp:g.w",2,{IMM16,RNIND_D8},5,{{0xe8,0xf8,RN },{0x00,0x00,DISP8 },{0x05,0xff,0 },{0x00,0x00,IMM16 },{0x00,0x00,0 }}},
{122,'a','E','I','!',O_CMP|O_WORD,"cmp:g.w",2,{IMM16,ABS8},5,	{{0x0d,0xff,0 },{0x00,0x00,ABS8 },{0x05,0xff,0 },{0x00,0x00,IMM16 },{0x00,0x00,0 }}},
{122,'a','E','I','!',O_CMP|O_WORD,"cmp:g.w",2,{IMM16,RNIND_D16},6,	{{0xf8,0xf8,RN },{0x00,0x00,DISP16 },{0x00,0x00,0 },{0x05,0xff,0 },{0x00,0x00,IMM16 },{0x00,0x00,0 }}},
{122,'a','E','I','!',O_CMP|O_WORD,"cmp:g.w",2,{IMM16,ABS16},6,	{{0x1d,0xff,0 },{0x00,0x00,ABS16 },{0x00,0x00,0 },{0x05,0xff,0 },{0x00,0x00,IMM16 },{0x00,0x00,0 }}},
{123,'a','D','E','!',O_CMP|O_BYTE,"cmp:g.b",2,{RN,RD},2,	{{0xa0,0xf8,RN },{0x70,0xf8,RD }}},
{123,'a','D','E','!',O_CMP|O_BYTE,"cmp:g.b",2,{RNDEC,RD},2,	{{0xb0,0xf8,RN },{0x70,0xf8,RD }}},
{123,'a','D','E','!',O_CMP|O_BYTE,"cmp:g.b",2,{RNIND,RD},2,	{{0xd0,0xf8,RN },{0x70,0xf8,RD }}},
{123,'a','D','E','!',O_CMP|O_BYTE,"cmp:g.b",2,{RNINC,RD},2,	{{0xc0,0xf8,RN },{0x70,0xf8,RD }}},
{123,'a','E','I','!',O_CMP|O_BYTE,"cmp:g.b",2,{IMM8,RN},3,	{{0xa0,0xf8,RN },{0x04,0xff,0 },{0x00,0x00,IMM8 }}},
{123,'a','E','I','!',O_CMP|O_BYTE,"cmp:g.b",2,{IMM8,RNIND},3,	{{0xd0,0xf8,RN },{0x04,0xff,0 },{0x00,0x00,IMM8 }}},
{123,'a','E','I','!',O_CMP|O_BYTE,"cmp:g.b",2,{IMM8,RNINC},3,	{{0xc0,0xf8,RN },{0x04,0xff,0 },{0x00,0x00,IMM8 }}},
{123,'a','E','I','!',O_CMP|O_BYTE,"cmp:g.b",2,{IMM8,RNDEC},3,	{{0xb0,0xf8,RN },{0x04,0xff,0 },{0x00,0x00,IMM8 }}},
{123,'a','D','E','!',O_CMP|O_BYTE,"cmp:g.b",2,{IMM8,RD},3,	{{0x04,0xff,0 },{0x00,0x00,IMM8 },{0x70,0xf8,RD }}},
{123,'a','D','E','!',O_CMP|O_BYTE,"cmp:g.b",2,{RNIND_D8,RD},3,	{{0xe0,0xf8,RN },{0x00,0x00,DISP8 },{0x70,0xf8,RD }}},
{123,'a','D','E','!',O_CMP|O_BYTE,"cmp:g.b",2,{ABS8,RD},3,	{{0x05,0xff,0 },{0x00,0x00,ABS8 },{0x70,0xf8,RD }}},
{123,'a','D','E','!',O_CMP|O_BYTE,"cmp:g.b",2,{ABS16,RD},4,	{{0x15,0xff,0 },{0x00,0x00,ABS16 },{0x00,0x00,0 },{0x70,0xf8,RD }}},
{123,'a','E','I','!',O_CMP|O_BYTE,"cmp:g.b",2,{IMM8,RNIND_D8},4,	{{0xe0,0xf8,RN },{0x00,0x00,DISP8 },{0x04,0xff,0 },{0x00,0x00,IMM8 }}},
{123,'a','E','I','!',O_CMP|O_BYTE,"cmp:g.b",2,{IMM8,ABS8},4,	{{0x05,0xff,0 },{0x00,0x00,ABS8 },{0x04,0xff,0 },{0x00,0x00,IMM8 }}},
{123,'a','D','E','!',O_CMP|O_BYTE,"cmp:g.b",2,{RNIND_D16,RD},4,	{{0xf0,0xf8,RN },{0x00,0x00,DISP16 },{0x00,0x00,0 },{0x70,0xf8,RD }}},
{123,'a','E','I','!',O_CMP|O_BYTE,"cmp:g.b",2,{IMM8,ABS16},5,	{{0x15,0xff,0 },{0x00,0x00,ABS16 },{0x00,0x00,0 },{0x04,0xff,0 },{0x00,0x00,IMM8 }}},
{123,'a','E','I','!',O_CMP|O_BYTE,"cmp:g.b",2,{IMM8,RNIND_D16},5,	{{0xf0,0xf8,RN },{0x00,0x00,DISP16 },{0x00,0x00,0 },{0x04,0xff,0 },{0x00,0x00,IMM8 }}},
{124,'a','D','E','!',O_CMP|O_UNSZ,"cmp:g",2,{RN,RD},2,		{{0xa8,0xf8,RN },{0x70,0xf8,RD }}},
{124,'a','D','E','!',O_CMP|O_UNSZ,"cmp:g",2,{RNIND,RD},2,	{{0xd8,0xf8,RN },{0x70,0xf8,RD }}},
{124,'a','D','E','!',O_CMP|O_UNSZ,"cmp:g",2,{RNINC,RD},2,	{{0xc8,0xf8,RN },{0x70,0xf8,RD }}},
{124,'a','D','E','!',O_CMP|O_UNSZ,"cmp:g",2,{RNDEC,RD},2,	{{0xb8,0xf8,RN },{0x70,0xf8,RD }}},
{124,'a','D','E','!',O_CMP|O_UNSZ,"cmp:g",2,{RNIND_D8,RD},3,	{{0xe8,0xf8,RN },{0x00,0x00,DISP8 },{0x70,0xf8,RD }}},
{124,'a','D','E','!',O_CMP|O_UNSZ,"cmp:g",2,{ABS8,RD},3,	{{0x0d,0xff,0 },{0x00,0x00,ABS8 },{0x70,0xf8,RD }}},
{124,'a','E','I','!',O_CMP|O_UNSZ,"cmp:g",2,{IMM16,RNINC},4,	{{0xc8,0xf8,RN },{0x05,0xff,0 },{0x00,0x00,IMM16 },{0x00,0x00,0 }}},
{124,'a','E','I','!',O_CMP|O_UNSZ,"cmp:g",2,{IMM16,RNIND},4,	{{0xd8,0xf8,RN },{0x05,0xff,0 },{0x00,0x00,IMM16 },{0x00,0x00,0 }}},
{124,'a','E','I','!',O_CMP|O_UNSZ,"cmp:g",2,{IMM16,RN},4,	{{0xa8,0xf8,RN },{0x05,0xff,0 },{0x00,0x00,IMM16 },{0x00,0x00,0 }}},
{124,'a','E','I','!',O_CMP|O_UNSZ,"cmp:g",2,{IMM16,RNDEC},4,	{{0xb8,0xf8,RN },{0x05,0xff,0 },{0x00,0x00,IMM16 },{0x00,0x00,0 }}},
{124,'a','D','E','!',O_CMP|O_UNSZ,"cmp:g",2,{IMM16,RD},4,	{{0x0c,0xff,0 },{0x00,0x00,IMM16 },{0x00,0x00,0 },{0x70,0xf8,RD }}},
{124,'a','D','E','!',O_CMP|O_UNSZ,"cmp:g",2,{ABS16,RD},4,	{{0x1d,0xff,0 },{0x00,0x00,ABS16 },{0x00,0x00,0 },{0x70,0xf8,RD }}},
{124,'a','D','E','!',O_CMP|O_UNSZ,"cmp:g",2,{RNIND_D16,RD},4,	{{0xf8,0xf8,RN },{0x00,0x00,DISP16 },{0x00,0x00,0 },{0x70,0xf8,RD }}},
{124,'a','E','I','!',O_CMP|O_UNSZ,"cmp:g",2,{IMM16,RNIND_D8},5,	{{0xe8,0xf8,RN },{0x00,0x00,DISP8 },{0x05,0xff,0 },{0x00,0x00,IMM16 },{0x00,0x00,0 }}},
{124,'a','E','I','!',O_CMP|O_UNSZ,"cmp:g",2,{IMM16,ABS8},5,	{{0x0d,0xff,0 },{0x00,0x00,ABS8 },{0x05,0xff,0 },{0x00,0x00,IMM16 },{0x00,0x00,0 }}},
{124,'a','E','I','!',O_CMP|O_UNSZ,"cmp:g",2,{IMM16,RNIND_D16},6,{{0xf8,0xf8,RN },{0x00,0x00,DISP16 },{0x00,0x00,0 },{0x05,0xff,0 },{0x00,0x00,IMM16 },{0x00,0x00,0 }}},
{124,'a','E','I','!',O_CMP|O_UNSZ,"cmp:g",2,{IMM16,ABS16},6,	{{0x1d,0xff,0 },{0x00,0x00,ABS16 },{0x00,0x00,0 },{0x05,0xff,0 },{0x00,0x00,IMM16 },{0x00,0x00,0 }}},
{125,'a','D','I','!',O_CMP|O_BYTE,"cmp:e.b",2,{IMM8,RD},2,	{{0x40,0xf8,RD },{0x00,0x00,IMM8 }}},
{126,'a','D','I','!',O_CMP|O_UNSZ,"cmp:e",2,{IMM8,RD},2,	{{0x48,0xf8,RD },{0x00,0x00,IMM8 }}},
{127,'a','D','E','!',O_CMP|O_WORD,"cmp.w",2,{RN,RD},2,		{{0xa8,0xf8,RN },{0x70,0xf8,RD }}},
{127,'a','D','E','!',O_CMP|O_WORD,"cmp.w",2,{RNDEC,RD},2,	{{0xb8,0xf8,RN },{0x70,0xf8,RD }}},
{127,'a','D','E','!',O_CMP|O_WORD,"cmp.w",2,{RNINC,RD},2,	{{0xc8,0xf8,RN },{0x70,0xf8,RD }}},
{127,'a','D','E','!',O_CMP|O_WORD,"cmp.w",2,{RNIND,RD},2,	{{0xd8,0xf8,RN },{0x70,0xf8,RD }}},
{127,'a','D','I','!',O_CMP|O_WORD,"cmp.w",2,{IMM16,RD},3,	{{0x48,0xf8,RD },{0x00,0x00,IMM16 },{0x00,0x00,0 }}},
{127,'a','D','E','!',O_CMP|O_WORD,"cmp.w",2,{RNIND_D8,RD},3,	{{0xe8,0xf8,RN },{0x00,0x00,DISP8 },{0x70,0xf8,RD }}},
{127,'a','D','E','!',O_CMP|O_WORD,"cmp.w",2,{ABS8,RD},3,	{{0x0d,0xff,0 },{0x00,0x00,ABS8 },{0x70,0xf8,RD }}},
{127,'a','E','I','!',O_CMP|O_WORD,"cmp.w",2,{IMM16,RNINC},4,	{{0xc8,0xf8,RN },{0x05,0xff,0 },{0x00,0x00,IMM16 },{0x00,0x00,0 }}},
{127,'a','E','I','!',O_CMP|O_WORD,"cmp.w",2,{IMM16,RNDEC},4,	{{0xb8,0xf8,RN },{0x05,0xff,0 },{0x00,0x00,IMM16 },{0x00,0x00,0 }}},
{127,'a','E','I','!',O_CMP|O_WORD,"cmp.w",2,{IMM16,RNIND},4,	{{0xd8,0xf8,RN },{0x05,0xff,0 },{0x00,0x00,IMM16 },{0x00,0x00,0 }}},
{127,'a','D','E','!',O_CMP|O_WORD,"cmp.w",2,{RNIND_D16,RD},4,	{{0xf8,0xf8,RN },{0x00,0x00,DISP16 },{0x00,0x00,0 },{0x70,0xf8,RD }}},
{127,'a','D','E','!',O_CMP|O_WORD,"cmp.w",2,{ABS16,RD},4,	{{0x1d,0xff,0 },{0x00,0x00,ABS16 },{0x00,0x00,0 },{0x70,0xf8,RD }}},
{127,'a','D','E','!',O_CMP|O_WORD,"cmp.w",2,{IMM16,RD},4,	{{0x0c,0xff,0 },{0x00,0x00,IMM16 },{0x00,0x00,0 },{0x70,0xf8,RD }}},
{127,'a','E','I','!',O_CMP|O_WORD,"cmp.w",2,{IMM16,RN},4,	{{0xa8,0xf8,RN },{0x05,0xff,0 },{0x00,0x00,IMM16 },{0x00,0x00,0 }}},
{127,'a','E','I','!',O_CMP|O_WORD,"cmp.w",2,{IMM16,RNIND_D8},5,	{{0xe8,0xf8,RN },{0x00,0x00,DISP8 },{0x05,0xff,0 },{0x00,0x00,IMM16 },{0x00,0x00,0 }}},
{127,'a','E','I','!',O_CMP|O_WORD,"cmp.w",2,{IMM16,ABS8},5,	{{0x0d,0xff,0 },{0x00,0x00,ABS8 },{0x05,0xff,0 },{0x00,0x00,IMM16 },{0x00,0x00,0 }}},
{127,'a','E','I','!',O_CMP|O_WORD,"cmp.w",2,{IMM16,RNIND_D16},6,{{0xf8,0xf8,RN },{0x00,0x00,DISP16 },{0x00,0x00,0 },{0x05,0xff,0 },{0x00,0x00,IMM16 },{0x00,0x00,0 }}},
{127,'a','E','I','!',O_CMP|O_WORD,"cmp.w",2,{IMM16,ABS16},6,	{{0x1d,0xff,0 },{0x00,0x00,ABS16 },{0x00,0x00,0 },{0x05,0xff,0 },{0x00,0x00,IMM16 },{0x00,0x00,0 }}},
{128,'a','D','E','!',O_CMP|O_BYTE,"cmp.b",2,{RN,RD},2,		{{0xa0,0xf8,RN },{0x70,0xf8,RD }}},
{128,'a','D','E','!',O_CMP|O_BYTE,"cmp.b",2,{RNDEC,RD},2,	{{0xb0,0xf8,RN },{0x70,0xf8,RD }}},
{128,'a','D','E','!',O_CMP|O_BYTE,"cmp.b",2,{RNINC,RD},2,	{{0xc0,0xf8,RN },{0x70,0xf8,RD }}},
{128,'a','D','I','!',O_CMP|O_BYTE,"cmp.b",2,{IMM8,RD},2,	{{0x40,0xf8,RD },{0x00,0x00,IMM8 }}},
{128,'a','D','E','!',O_CMP|O_BYTE,"cmp.b",2,{RNIND,RD},2,	{{0xd0,0xf8,RN },{0x70,0xf8,RD }}},
{128,'a','E','I','!',O_CMP|O_BYTE,"cmp.b",2,{IMM8,RN},3,	{{0xa0,0xf8,RN },{0x04,0xff,0 },{0x00,0x00,IMM8 }}},
{128,'a','E','I','!',O_CMP|O_BYTE,"cmp.b",2,{IMM8,RNIND},3,	{{0xd0,0xf8,RN },{0x04,0xff,0 },{0x00,0x00,IMM8 }}},
{128,'a','E','I','!',O_CMP|O_BYTE,"cmp.b",2,{IMM8,RNINC},3,	{{0xc0,0xf8,RN },{0x04,0xff,0 },{0x00,0x00,IMM8 }}},
{128,'a','E','I','!',O_CMP|O_BYTE,"cmp.b",2,{IMM8,RNDEC},3,	{{0xb0,0xf8,RN },{0x04,0xff,0 },{0x00,0x00,IMM8 }}},
{128,'a','D','E','!',O_CMP|O_BYTE,"cmp.b",2,{ABS8,RD},3,	{{0x05,0xff,0 },{0x00,0x00,ABS8 },{0x70,0xf8,RD }}},
{128,'a','D','E','!',O_CMP|O_BYTE,"cmp.b",2,{RNIND_D8,RD},3,	{{0xe0,0xf8,RN },{0x00,0x00,DISP8 },{0x70,0xf8,RD }}},
{128,'a','D','E','!',O_CMP|O_BYTE,"cmp.b",2,{IMM8,RD},3,	{{0x04,0xff,0 },{0x00,0x00,IMM8 },{0x70,0xf8,RD }}},
{128,'a','E','I','!',O_CMP|O_BYTE,"cmp.b",2,{IMM8,ABS8},4,	{{0x05,0xff,0 },{0x00,0x00,ABS8 },{0x04,0xff,0 },{0x00,0x00,IMM8 }}},
{128,'a','D','E','!',O_CMP|O_BYTE,"cmp.b",2,{ABS16,RD},4,	{{0x15,0xff,0 },{0x00,0x00,ABS16 },{0x00,0x00,0 },{0x70,0xf8,RD }}},
{128,'a','E','I','!',O_CMP|O_BYTE,"cmp.b",2,{IMM8,RNIND_D8},4,	{{0xe0,0xf8,RN },{0x00,0x00,DISP8 },{0x04,0xff,0 },{0x00,0x00,IMM8 }}},
{128,'a','D','E','!',O_CMP|O_BYTE,"cmp.b",2,{RNIND_D16,RD},4,	{{0xf0,0xf8,RN },{0x00,0x00,DISP16 },{0x00,0x00,0 },{0x70,0xf8,RD }}},
{128,'a','E','I','!',O_CMP|O_BYTE,"cmp.b",2,{IMM8,ABS16},5,	{{0x15,0xff,0 },{0x00,0x00,ABS16 },{0x00,0x00,0 },{0x04,0xff,0 },{0x00,0x00,IMM8 }}},
{128,'a','E','I','!',O_CMP|O_BYTE,"cmp.b",2,{IMM8,RNIND_D16},5,	{{0xf0,0xf8,RN },{0x00,0x00,DISP16 },{0x00,0x00,0 },{0x04,0xff,0 },{0x00,0x00,IMM8 }}},
{129,'a','D','E','!',O_CMP|O_UNSZ,"cmp",2,{RN,RD},2,	{{0xa8,0xf8,RN },{0x70,0xf8,RD }}},
{129,'a','D','I','!',O_CMP|O_UNSZ,"cmp",2,{IMM8,RD},2,	{{0x48,0xf8,RD },{0x00,0x00,IMM8 }}},
{129,'a','D','E','!',O_CMP|O_UNSZ,"cmp",2,{RNINC,RD},2,	{{0xc8,0xf8,RN },{0x70,0xf8,RD }}},
{129,'a','D','E','!',O_CMP|O_UNSZ,"cmp",2,{RNIND,RD},2,	{{0xd8,0xf8,RN },{0x70,0xf8,RD }}},
{129,'a','D','E','!',O_CMP|O_UNSZ,"cmp",2,{RNDEC,RD},2,	{{0xb8,0xf8,RN },{0x70,0xf8,RD }}},
{129,'a','D','I','!',O_CMP|O_UNSZ,"cmp",2,{IMM16,RD},3,	{{0x48,0xf8,RD },{0x00,0x00,IMM16 },{0x00,0x00,0 }}},
{129,'a','D','E','!',O_CMP|O_UNSZ,"cmp",2,{RNIND_D8,RD},3,	{{0xe8,0xf8,RN },{0x00,0x00,DISP8 },{0x70,0xf8,RD }}},
{129,'a','D','E','!',O_CMP|O_UNSZ,"cmp",2,{ABS8,RD},3,		{{0x0d,0xff,0 },{0x00,0x00,ABS8 },{0x70,0xf8,RD }}},
{129,'a','E','I','!',O_CMP|O_UNSZ,"cmp",2,{IMM16,RN},4,		{{0xa8,0xf8,RN },{0x05,0xff,0 },{0x00,0x00,IMM16 },{0x00,0x00,0 }}},
{129,'a','E','I','!',O_CMP|O_UNSZ,"cmp",2,{IMM16,RNDEC},4,	{{0xb8,0xf8,RN },{0x05,0xff,0 },{0x00,0x00,IMM16 },{0x00,0x00,0 }}},
{129,'a','E','I','!',O_CMP|O_UNSZ,"cmp",2,{IMM16,RNIND},4,	{{0xd8,0xf8,RN },{0x05,0xff,0 },{0x00,0x00,IMM16 },{0x00,0x00,0 }}},
{129,'a','D','E','!',O_CMP|O_UNSZ,"cmp",2,{RNIND_D16,RD},4,	{{0xf8,0xf8,RN },{0x00,0x00,DISP16 },{0x00,0x00,0 },{0x70,0xf8,RD }}},
{129,'a','E','I','!',O_CMP|O_UNSZ,"cmp",2,{IMM16,RNINC},4,	{{0xc8,0xf8,RN },{0x05,0xff,0 },{0x00,0x00,IMM16 },{0x00,0x00,0 }}},
{129,'a','D','E','!',O_CMP|O_UNSZ,"cmp",2,{ABS16,RD},4,	{{0x1d,0xff,0 },{0x00,0x00,ABS16 },{0x00,0x00,0 },{0x70,0xf8,RD }}},
{129,'a','D','E','!',O_CMP|O_UNSZ,"cmp",2,{IMM16,RD},4,	{{0x0c,0xff,0 },{0x00,0x00,IMM16 },{0x00,0x00,0 },{0x70,0xf8,RD }}},
{129,'a','E','I','!',O_CMP|O_UNSZ,"cmp",2,{IMM16,ABS8},5,	{{0x0d,0xff,0 },{0x00,0x00,ABS8 },{0x05,0xff,0 },{0x00,0x00,IMM16 },{0x00,0x00,0 }}},
{129,'a','E','I','!',O_CMP|O_UNSZ,"cmp",2,{IMM16,RNIND_D8},5,	{{0xe8,0xf8,RN },{0x00,0x00,DISP8 },{0x05,0xff,0 },{0x00,0x00,IMM16 },{0x00,0x00,0 }}},
{129,'a','E','I','!',O_CMP|O_UNSZ,"cmp",2,{IMM16,ABS16},6,	{{0x1d,0xff,0 },{0x00,0x00,ABS16 },{0x00,0x00,0 },{0x05,0xff,0 },{0x00,0x00,IMM16 },{0x00,0x00,0 }}},
{129,'a','E','I','!',O_CMP|O_UNSZ,"cmp",2,{IMM16,RNIND_D16},6,	{{0xf8,0xf8,RN },{0x00,0x00,DISP16 },{0x00,0x00,0 },{0x05,0xff,0 },{0x00,0x00,IMM16 },{0x00,0x00,0 }}},
{130,'c','!','!','E',O_CLR|O_WORD,"clr.w",1,{RN,0},2,	{{0xa8,0xf8,RN },{0x13,0xff,0 }}},
{130,'c','!','!','E',O_CLR|O_WORD,"clr.w",1,{RNIND,0},2,	{{0xd8,0xf8,RN },{0x13,0xff,0 }}},
{130,'c','!','!','E',O_CLR|O_WORD,"clr.w",1,{RNINC,0},2,	{{0xc8,0xf8,RN },{0x13,0xff,0 }}},
{130,'c','!','!','E',O_CLR|O_WORD,"clr.w",1,{RNDEC,0},2,	{{0xb8,0xf8,RN },{0x13,0xff,0 }}},
{130,'c','!','!','E',O_CLR|O_WORD,"clr.w",1,{ABS8,0},3,	{{0x0d,0xff,0 },{0x00,0x00,ABS8 },{0x13,0xff,0 }}},
{130,'c','!','!','E',O_CLR|O_WORD,"clr.w",1,{RNIND_D8,0},3,	{{0xe8,0xf8,RN },{0x00,0x00,DISP8 },{0x13,0xff,0 }}},
{130,'c','!','!','E',O_CLR|O_WORD,"clr.w",1,{ABS16,0},4,	{{0x1d,0xff,0 },{0x00,0x00,ABS16 },{0x00,0x00,0 },{0x13,0xff,0 }}},
{130,'c','!','!','E',O_CLR|O_WORD,"clr.w",1,{IMM16,0},4,	{{0x0c,0xff,0 },{0x00,0x00,IMM16 },{0x00,0x00,0 },{0x13,0xff,0 }}},
{130,'c','!','!','E',O_CLR|O_WORD,"clr.w",1,{RNIND_D16,0},4,	{{0xf8,0xf8,RN },{0x00,0x00,DISP16 },{0x00,0x00,0 },{0x13,0xff,0 }}},
{131,'c','!','!','E',O_CLR|O_BYTE,"clr.b",1,{RN,0},2,	{{0xa0,0xf8,RN },{0x13,0xff,0 }}},
{131,'c','!','!','E',O_CLR|O_BYTE,"clr.b",1,{RNINC,0},2,	{{0xc0,0xf8,RN },{0x13,0xff,0 }}},
{131,'c','!','!','E',O_CLR|O_BYTE,"clr.b",1,{RNDEC,0},2,	{{0xb0,0xf8,RN },{0x13,0xff,0 }}},
{131,'c','!','!','E',O_CLR|O_BYTE,"clr.b",1,{RNIND,0},2,	{{0xd0,0xf8,RN },{0x13,0xff,0 }}},
{131,'c','!','!','E',O_CLR|O_BYTE,"clr.b",1,{ABS8,0},3,	{{0x05,0xff,0 },{0x00,0x00,ABS8 },{0x13,0xff,0 }}},
{131,'c','!','!','E',O_CLR|O_BYTE,"clr.b",1,{IMM8,0},3,	{{0x04,0xff,0 },{0x00,0x00,IMM8 },{0x13,0xff,0 }}},
{131,'c','!','!','E',O_CLR|O_BYTE,"clr.b",1,{RNIND_D8,0},3,	{{0xe0,0xf8,RN },{0x00,0x00,DISP8 },{0x13,0xff,0 }}},
{131,'c','!','!','E',O_CLR|O_BYTE,"clr.b",1,{ABS16,0},4,	{{0x15,0xff,0 },{0x00,0x00,ABS16 },{0x00,0x00,0 },{0x13,0xff,0 }}},
{131,'c','!','!','E',O_CLR|O_BYTE,"clr.b",1,{RNIND_D16,0},4,	{{0xf0,0xf8,RN },{0x00,0x00,DISP16 },{0x00,0x00,0 },{0x13,0xff,0 }}},
{132,'c','!','!','E',O_CLR|O_UNSZ,"clr",1,{RN,0},2,	{{0xa8,0xf8,RN },{0x13,0xff,0 }}},
{132,'c','!','!','E',O_CLR|O_UNSZ,"clr",1,{RNIND,0},2,	{{0xd8,0xf8,RN },{0x13,0xff,0 }}},
{132,'c','!','!','E',O_CLR|O_UNSZ,"clr",1,{RNINC,0},2,	{{0xc8,0xf8,RN },{0x13,0xff,0 }}},
{132,'c','!','!','E',O_CLR|O_UNSZ,"clr",1,{RNDEC,0},2,	{{0xb8,0xf8,RN },{0x13,0xff,0 }}},
{132,'c','!','!','E',O_CLR|O_UNSZ,"clr",1,{ABS8,0},3,	{{0x0d,0xff,0 },{0x00,0x00,ABS8 },{0x13,0xff,0 }}},
{132,'c','!','!','E',O_CLR|O_UNSZ,"clr",1,{RNIND_D8,0},3,	{{0xe8,0xf8,RN },{0x00,0x00,DISP8 },{0x13,0xff,0 }}},
{132,'c','!','!','E',O_CLR|O_UNSZ,"clr",1,{IMM16,0},4,	{{0x0c,0xff,0 },{0x00,0x00,IMM16 },{0x00,0x00,0 },{0x13,0xff,0 }}},
{132,'c','!','!','E',O_CLR|O_UNSZ,"clr",1,{ABS16,0},4,	{{0x1d,0xff,0 },{0x00,0x00,ABS16 },{0x00,0x00,0 },{0x13,0xff,0 }}},
{132,'c','!','!','E',O_CLR|O_UNSZ,"clr",1,{RNIND_D16,0},4,	{{0xf8,0xf8,RN },{0x00,0x00,DISP16 },{0x00,0x00,0 },{0x13,0xff,0 }}},
{133,'-','B','!','!',O_BVS|O_WORD,"bvs.w",1,{PCREL16,0},3,	{{0x39,0xff,0 },{0x00,0x00,PCREL16 },{0x00,0x00,0 }}},
{134,'-','B','!','!',O_BVS|O_BYTE,"bvs.b",1,{PCREL8,0},2,	{{0x29,0xff,0 },{0x00,0x00,PCREL8 }}},
{135,'-','B','!','!',O_BVS|O_UNSZ,"bvs",1,{PCREL8,0},2,	{{0x29,0xff,0 },{0x00,0x00,PCREL8 }}},
{135,'-','B','!','!',O_BVS|O_UNSZ,"bvs",1,{PCREL16,0},3,	{{0x39,0xff,0 },{0x00,0x00,PCREL16 },{0x00,0x00,0 }}},
{136,'-','B','!','!',O_BVC|O_WORD,"bvc.w",1,{PCREL16,0},3,	{{0x38,0xff,0 },{0x00,0x00,PCREL16 },{0x00,0x00,0 }}},
{137,'-','B','!','!',O_BVC|O_BYTE,"bvc.b",1,{PCREL8,0},2,	{{0x28,0xff,0 },{0x00,0x00,PCREL8 }}},
{138,'-','B','!','!',O_BVC|O_UNSZ,"bvc",1,{PCREL8,0},2,	{{0x28,0xff,0 },{0x00,0x00,PCREL8 }}},
{138,'-','B','!','!',O_BVC|O_UNSZ,"bvc",1,{PCREL16,0},3,	{{0x38,0xff,0 },{0x00,0x00,PCREL16 },{0x00,0x00,0 }}},
{139,'b','E','S','E',O_BTST|O_WORD,"btst.w",2,{RS,RN},2,	{{0xa8,0xf8,RN },{0x78,0xf8,RS }}},
{139,'b','E','I','E',O_BTST|O_WORD,"btst.w",2,{IMM4,RNDEC},2,	{{0xb8,0xf8,RN },{0xf0,0xf0,IMM4 }}},
{139,'b','E','I','E',O_BTST|O_WORD,"btst.w",2,{IMM4,RNINC},2,	{{0xc8,0xf8,RN },{0xf0,0xf0,IMM4 }}},
{139,'b','E','S','E',O_BTST|O_WORD,"btst.w",2,{RS,RNIND},2,	{{0xd8,0xf8,RN },{0x78,0xf8,RS }}},
{139,'b','E','S','E',O_BTST|O_WORD,"btst.w",2,{RS,RNINC},2,	{{0xc8,0xf8,RN },{0x78,0xf8,RS }}},
{139,'b','E','S','E',O_BTST|O_WORD,"btst.w",2,{RS,RNDEC},2,	{{0xb8,0xf8,RN },{0x78,0xf8,RS }}},
{139,'b','E','I','E',O_BTST|O_WORD,"btst.w",2,{IMM4,RNIND},2,	{{0xd8,0xf8,RN },{0xf0,0xf0,IMM4 }}},
{139,'b','E','I','E',O_BTST|O_WORD,"btst.w",2,{IMM4,RN},2,	{{0xa8,0xf8,RN },{0xf0,0xf0,IMM4 }}},
{139,'b','E','S','E',O_BTST|O_WORD,"btst.w",2,{RS,RNIND_D8},3,	{{0xe8,0xf8,RN },{0x00,0x00,DISP8 },{0x78,0xf8,RS }}},
{139,'b','E','I','E',O_BTST|O_WORD,"btst.w",2,{IMM4,ABS8},3,	{{0x0d,0xff,0 },{0x00,0x00,ABS8 },{0xf0,0xf0,IMM4 }}},
{139,'b','E','S','E',O_BTST|O_WORD,"btst.w",2,{RS,ABS8},3,	{{0x0d,0xff,0 },{0x00,0x00,ABS8 },{0x78,0xf8,RS }}},
{139,'b','E','I','E',O_BTST|O_WORD,"btst.w",2,{IMM4,RNIND_D8},3,	{{0xe8,0xf8,RN },{0x00,0x00,DISP8 },{0xf0,0xf0,IMM4 }}},
{139,'b','E','S','E',O_BTST|O_WORD,"btst.w",2,{RS,RNIND_D16},4,	{{0xf8,0xf8,RN },{0x00,0x00,DISP16 },{0x00,0x00,0 },{0x78,0xf8,RS }}},
{139,'b','E','S','E',O_BTST|O_WORD,"btst.w",2,{RS,ABS16},4,	{{0x1d,0xff,0 },{0x00,0x00,ABS16 },{0x00,0x00,0 },{0x78,0xf8,RS }}},
{139,'b','E','I','E',O_BTST|O_WORD,"btst.w",2,{IMM4,ABS16},4,	{{0x1d,0xff,0 },{0x00,0x00,ABS16 },{0x00,0x00,0 },{0xf0,0xf0,IMM4 }}},
{139,'b','E','I','E',O_BTST|O_WORD,"btst.w",2,{IMM4,RNIND_D16},4,	{{0xf8,0xf8,RN },{0x00,0x00,DISP16 },{0x00,0x00,0 },{0xf0,0xf0,IMM4 }}},
{140,'b','E','S','E',O_BTST|O_BYTE,"btst.b",2,{RS,RN},2,	{{0xa0,0xf8,RN },{0x78,0xf8,RS }}},
{140,'b','E','I','E',O_BTST|O_BYTE,"btst.b",2,{IMM4,RNDEC},2,	{{0xb0,0xf8,RN },{0xf0,0xf0,IMM4 }}},
{140,'b','E','I','E',O_BTST|O_BYTE,"btst.b",2,{IMM4,RN},2,	{{0xa0,0xf8,RN },{0xf0,0xf0,IMM4 }}},
{140,'b','E','S','E',O_BTST|O_BYTE,"btst.b",2,{RS,RNIND},2,	{{0xd0,0xf8,RN },{0x78,0xf8,RS }}},
{140,'b','E','S','E',O_BTST|O_BYTE,"btst.b",2,{RS,RNINC},2,	{{0xc0,0xf8,RN },{0x78,0xf8,RS }}},
{140,'b','E','S','E',O_BTST|O_BYTE,"btst.b",2,{RS,RNDEC},2,	{{0xb0,0xf8,RN },{0x78,0xf8,RS }}},
{140,'b','E','I','E',O_BTST|O_BYTE,"btst.b",2,{IMM4,RNIND},2,	{{0xd0,0xf8,RN },{0xf0,0xf0,IMM4 }}},
{140,'b','E','I','E',O_BTST|O_BYTE,"btst.b",2,{IMM4,RNINC},2,	{{0xc0,0xf8,RN },{0xf0,0xf0,IMM4 }}},
{140,'b','E','S','E',O_BTST|O_BYTE,"btst.b",2,{RS,RNIND_D8},3,	{{0xe0,0xf8,RN },{0x00,0x00,DISP8 },{0x78,0xf8,RS }}},
{140,'b','E','I','E',O_BTST|O_BYTE,"btst.b",2,{IMM4,RNIND_D8},3,	{{0xe0,0xf8,RN },{0x00,0x00,DISP8 },{0xf0,0xf0,IMM4 }}},
{140,'b','E','I','E',O_BTST|O_BYTE,"btst.b",2,{IMM4,ABS8},3,	{{0x05,0xff,0 },{0x00,0x00,ABS8 },{0xf0,0xf0,IMM4 }}},
{140,'b','E','S','E',O_BTST|O_BYTE,"btst.b",2,{RS,ABS8},3,	{{0x05,0xff,0 },{0x00,0x00,ABS8 },{0x78,0xf8,RS }}},
{140,'b','E','I','E',O_BTST|O_BYTE,"btst.b",2,{IMM4,ABS16},4,	{{0x15,0xff,0 },{0x00,0x00,ABS16 },{0x00,0x00,0 },{0xf0,0xf0,IMM4 }}},
{140,'b','E','S','E',O_BTST|O_BYTE,"btst.b",2,{RS,RNIND_D16},4,	{{0xf0,0xf8,RN },{0x00,0x00,DISP16 },{0x00,0x00,0 },{0x78,0xf8,RS }}},
{140,'b','E','S','E',O_BTST|O_BYTE,"btst.b",2,{RS,ABS16},4,	{{0x15,0xff,0 },{0x00,0x00,ABS16 },{0x00,0x00,0 },{0x78,0xf8,RS }}},
{140,'b','E','I','E',O_BTST|O_BYTE,"btst.b",2,{IMM4,RNIND_D16},4,	{{0xf0,0xf8,RN },{0x00,0x00,DISP16 },{0x00,0x00,0 },{0xf0,0xf0,IMM4 }}},
{141,'b','E','S','E',O_BTST|O_UNSZ,"btst",2,{RS,RN},2,	{{0xa8,0xf8,RN },{0x78,0xf8,RS }}},
{141,'b','E','I','E',O_BTST|O_UNSZ,"btst",2,{IMM4,RNDEC},2,	{{0xb8,0xf8,RN },{0xf0,0xf0,IMM4 }}},
{141,'b','E','I','E',O_BTST|O_UNSZ,"btst",2,{IMM4,RNIND},2,	{{0xd8,0xf8,RN },{0xf0,0xf0,IMM4 }}},
{141,'b','E','S','E',O_BTST|O_UNSZ,"btst",2,{RS,RNIND},2,	{{0xd8,0xf8,RN },{0x78,0xf8,RS }}},
{141,'b','E','S','E',O_BTST|O_UNSZ,"btst",2,{RS,RNINC},2,	{{0xc8,0xf8,RN },{0x78,0xf8,RS }}},
{141,'b','E','S','E',O_BTST|O_UNSZ,"btst",2,{RS,RNDEC},2,	{{0xb8,0xf8,RN },{0x78,0xf8,RS }}},
{141,'b','E','I','E',O_BTST|O_UNSZ,"btst",2,{IMM4,RN},2,	{{0xa8,0xf8,RN },{0xf0,0xf0,IMM4 }}},
{141,'b','E','I','E',O_BTST|O_UNSZ,"btst",2,{IMM4,RNINC},2,	{{0xc8,0xf8,RN },{0xf0,0xf0,IMM4 }}},
{141,'b','E','S','E',O_BTST|O_UNSZ,"btst",2,{RS,RNIND_D8},3,	{{0xe8,0xf8,RN },{0x00,0x00,DISP8 },{0x78,0xf8,RS }}},
{141,'b','E','I','E',O_BTST|O_UNSZ,"btst",2,{IMM4,ABS8},3,	{{0x0d,0xff,0 },{0x00,0x00,ABS8 },{0xf0,0xf0,IMM4 }}},
{141,'b','E','S','E',O_BTST|O_UNSZ,"btst",2,{RS,ABS8},3,	{{0x0d,0xff,0 },{0x00,0x00,ABS8 },{0x78,0xf8,RS }}},
{141,'b','E','I','E',O_BTST|O_UNSZ,"btst",2,{IMM4,RNIND_D8},3,	{{0xe8,0xf8,RN },{0x00,0x00,DISP8 },{0xf0,0xf0,IMM4 }}},
{141,'b','E','I','E',O_BTST|O_UNSZ,"btst",2,{IMM4,ABS16},4,	{{0x1d,0xff,0 },{0x00,0x00,ABS16 },{0x00,0x00,0 },{0xf0,0xf0,IMM4 }}},
{141,'b','E','S','E',O_BTST|O_UNSZ,"btst",2,{RS,RNIND_D16},4,	{{0xf8,0xf8,RN },{0x00,0x00,DISP16 },{0x00,0x00,0 },{0x78,0xf8,RS }}},
{141,'b','E','S','E',O_BTST|O_UNSZ,"btst",2,{RS,ABS16},4,	{{0x1d,0xff,0 },{0x00,0x00,ABS16 },{0x00,0x00,0 },{0x78,0xf8,RS }}},
{141,'b','E','I','E',O_BTST|O_UNSZ,"btst",2,{IMM4,RNIND_D16},4,	{{0xf8,0xf8,RN },{0x00,0x00,DISP16 },{0x00,0x00,0 },{0xf0,0xf0,IMM4 }}},
{142,'-','B','!','!',O_BT|O_WORD,"bt.w",1,{PCREL16,0},3,	{{0x30,0xff,0 },{0x00,0x00,PCREL16 },{0x00,0x00,0 }}},
{143,'-','B','!','!',O_BT|O_BYTE,"bt.b",1,{PCREL8,0},2,	{{0x20,0xff,0 },{0x00,0x00,PCREL8 }}},
{144,'-','B','!','!',O_BT|O_UNSZ,"bt",1,{PCREL8,0},2,	{{0x20,0xff,0 },{0x00,0x00,PCREL8 }}},
{144,'-','B','!','!',O_BT|O_UNSZ,"bt",1,{PCREL16,0},3,	{{0x30,0xff,0 },{0x00,0x00,PCREL16 },{0x00,0x00,0 }}},
{145,'-','B','!','!',O_BSR|O_WORD,"bsr.w",1,{PCREL16,0},3,	{{0x1e,0xff,0 },{0x00,0x00,PCREL16 },{0x00,0x00,0 }}},
{146,'-','B','!','!',O_BSR|O_BYTE,"bsr.b",1,{PCREL8,0},2,	{{0x0e,0xff,0 },{0x00,0x00,PCREL8 }}},
{147,'-','B','!','!',O_BSR|O_UNSZ,"bsr",1,{PCREL8,0},2,	{{0x0e,0xff,0 },{0x00,0x00,PCREL8 }}},
{147,'-','B','!','!',O_BSR|O_UNSZ,"bsr",1,{PCREL16,0},3,	{{0x1e,0xff,0 },{0x00,0x00,PCREL16 },{0x00,0x00,0 }}},
{148,'b','E','S','E',O_BSET|O_WORD,"bset.w",2,{RS,RN},2,	{{0xa8,0xf8,RN },{0x48,0xf8,RS }}},
{148,'b','E','I','E',O_BSET|O_WORD,"bset.w",2,{IMM4,RNDEC},2,	{{0xb8,0xf8,RN },{0xc0,0xf0,IMM4 }}},
{148,'b','E','I','E',O_BSET|O_WORD,"bset.w",2,{IMM4,RN},2,	{{0xa8,0xf8,RN },{0xc0,0xf0,IMM4 }}},
{148,'b','E','S','E',O_BSET|O_WORD,"bset.w",2,{RS,RNIND},2,	{{0xd8,0xf8,RN },{0x48,0xf8,RS }}},
{148,'b','E','S','E',O_BSET|O_WORD,"bset.w",2,{RS,RNINC},2,	{{0xc8,0xf8,RN },{0x48,0xf8,RS }}},
{148,'b','E','S','E',O_BSET|O_WORD,"bset.w",2,{RS,RNDEC},2,	{{0xb8,0xf8,RN },{0x48,0xf8,RS }}},
{148,'b','E','I','E',O_BSET|O_WORD,"bset.w",2,{IMM4,RNINC},2,	{{0xc8,0xf8,RN },{0xc0,0xf0,IMM4 }}},
{148,'b','E','I','E',O_BSET|O_WORD,"bset.w",2,{IMM4,RNIND},2,	{{0xd8,0xf8,RN },{0xc0,0xf0,IMM4 }}},
{148,'b','E','I','E',O_BSET|O_WORD,"bset.w",2,{IMM4,RNIND_D8},3,	{{0xe8,0xf8,RN },{0x00,0x00,DISP8 },{0xc0,0xf0,IMM4 }}},
{148,'b','E','S','E',O_BSET|O_WORD,"bset.w",2,{RS,ABS8},3,	{{0x0d,0xff,0 },{0x00,0x00,ABS8 },{0x48,0xf8,RS }}},
{148,'b','E','I','E',O_BSET|O_WORD,"bset.w",2,{IMM4,ABS8},3,	{{0x0d,0xff,0 },{0x00,0x00,ABS8 },{0xc0,0xf0,IMM4 }}},
{148,'b','E','S','E',O_BSET|O_WORD,"bset.w",2,{RS,RNIND_D8},3,	{{0xe8,0xf8,RN },{0x00,0x00,DISP8 },{0x48,0xf8,RS }}},
{148,'b','E','I','E',O_BSET|O_WORD,"bset.w",2,{IMM4,ABS16},4,	{{0x1d,0xff,0 },{0x00,0x00,ABS16 },{0x00,0x00,0 },{0xc0,0xf0,IMM4 }}},
{148,'b','E','S','E',O_BSET|O_WORD,"bset.w",2,{RS,ABS16},4,	{{0x1d,0xff,0 },{0x00,0x00,ABS16 },{0x00,0x00,0 },{0x48,0xf8,RS }}},
{148,'b','E','S','E',O_BSET|O_WORD,"bset.w",2,{RS,RNIND_D16},4,	{{0xf8,0xf8,RN },{0x00,0x00,DISP16 },{0x00,0x00,0 },{0x48,0xf8,RS }}},
{148,'b','E','I','E',O_BSET|O_WORD,"bset.w",2,{IMM4,RNIND_D16},4,	{{0xf8,0xf8,RN },{0x00,0x00,DISP16 },{0x00,0x00,0 },{0xc0,0xf0,IMM4 }}},
{149,'b','E','S','E',O_BSET|O_BYTE,"bset.b",2,{RS,RN},2,	{{0xa0,0xf8,RN },{0x48,0xf8,RS }}},
{149,'b','E','I','E',O_BSET|O_BYTE,"bset.b",2,{IMM4,RNINC},2,	{{0xc0,0xf8,RN },{0xc0,0xf0,IMM4 }}},
{149,'b','E','I','E',O_BSET|O_BYTE,"bset.b",2,{IMM4,RN},2,	{{0xa0,0xf8,RN },{0xc0,0xf0,IMM4 }}},
{149,'b','E','S','E',O_BSET|O_BYTE,"bset.b",2,{RS,RNIND},2,	{{0xd0,0xf8,RN },{0x48,0xf8,RS }}},
{149,'b','E','S','E',O_BSET|O_BYTE,"bset.b",2,{RS,RNINC},2,	{{0xc0,0xf8,RN },{0x48,0xf8,RS }}},
{149,'b','E','S','E',O_BSET|O_BYTE,"bset.b",2,{RS,RNDEC},2,	{{0xb0,0xf8,RN },{0x48,0xf8,RS }}},
{149,'b','E','I','E',O_BSET|O_BYTE,"bset.b",2,{IMM4,RNDEC},2,	{{0xb0,0xf8,RN },{0xc0,0xf0,IMM4 }}},
{149,'b','E','I','E',O_BSET|O_BYTE,"bset.b",2,{IMM4,RNIND},2,	{{0xd0,0xf8,RN },{0xc0,0xf0,IMM4 }}},
{149,'b','E','I','E',O_BSET|O_BYTE,"bset.b",2,{IMM4,ABS8},3,	{{0x05,0xff,0 },{0x00,0x00,ABS8 },{0xc0,0xf0,IMM4 }}},
{149,'b','E','I','E',O_BSET|O_BYTE,"bset.b",2,{IMM4,RNIND_D8},3,	{{0xe0,0xf8,RN },{0x00,0x00,DISP8 },{0xc0,0xf0,IMM4 }}},
{149,'b','E','S','E',O_BSET|O_BYTE,"bset.b",2,{RS,RNIND_D8},3,	{{0xe0,0xf8,RN },{0x00,0x00,DISP8 },{0x48,0xf8,RS }}},
{149,'b','E','S','E',O_BSET|O_BYTE,"bset.b",2,{RS,ABS8},3,	{{0x05,0xff,0 },{0x00,0x00,ABS8 },{0x48,0xf8,RS }}},
{149,'b','E','S','E',O_BSET|O_BYTE,"bset.b",2,{RS,RNIND_D16},4,	{{0xf0,0xf8,RN },{0x00,0x00,DISP16 },{0x00,0x00,0 },{0x48,0xf8,RS }}},
{149,'b','E','I','E',O_BSET|O_BYTE,"bset.b",2,{IMM4,ABS16},4,	{{0x15,0xff,0 },{0x00,0x00,ABS16 },{0x00,0x00,0 },{0xc0,0xf0,IMM4 }}},
{149,'b','E','S','E',O_BSET|O_BYTE,"bset.b",2,{RS,ABS16},4,	{{0x15,0xff,0 },{0x00,0x00,ABS16 },{0x00,0x00,0 },{0x48,0xf8,RS }}},
{149,'b','E','I','E',O_BSET|O_BYTE,"bset.b",2,{IMM4,RNIND_D16},4,	{{0xf0,0xf8,RN },{0x00,0x00,DISP16 },{0x00,0x00,0 },{0xc0,0xf0,IMM4 }}},
{150,'b','E','S','E',O_BSET|O_UNSZ,"bset",2,{RS,RN},2,	{{0xa8,0xf8,RN },{0x48,0xf8,RS }}},
{150,'b','E','I','E',O_BSET|O_UNSZ,"bset",2,{IMM4,RN},2,	{{0xa8,0xf8,RN },{0xc0,0xf0,IMM4 }}},
{150,'b','E','I','E',O_BSET|O_UNSZ,"bset",2,{IMM4,RNIND},2,	{{0xd8,0xf8,RN },{0xc0,0xf0,IMM4 }}},
{150,'b','E','S','E',O_BSET|O_UNSZ,"bset",2,{RS,RNIND},2,	{{0xd8,0xf8,RN },{0x48,0xf8,RS }}},
{150,'b','E','S','E',O_BSET|O_UNSZ,"bset",2,{RS,RNINC},2,	{{0xc8,0xf8,RN },{0x48,0xf8,RS }}},
{150,'b','E','S','E',O_BSET|O_UNSZ,"bset",2,{RS,RNDEC},2,	{{0xb8,0xf8,RN },{0x48,0xf8,RS }}},
{150,'b','E','I','E',O_BSET|O_UNSZ,"bset",2,{IMM4,RNINC},2,	{{0xc8,0xf8,RN },{0xc0,0xf0,IMM4 }}},
{150,'b','E','I','E',O_BSET|O_UNSZ,"bset",2,{IMM4,RNDEC},2,	{{0xb8,0xf8,RN },{0xc0,0xf0,IMM4 }}},
{150,'b','E','S','E',O_BSET|O_UNSZ,"bset",2,{RS,RNIND_D8},3,	{{0xe8,0xf8,RN },{0x00,0x00,DISP8 },{0x48,0xf8,RS }}},
{150,'b','E','I','E',O_BSET|O_UNSZ,"bset",2,{IMM4,RNIND_D8},3,	{{0xe8,0xf8,RN },{0x00,0x00,DISP8 },{0xc0,0xf0,IMM4 }}},
{150,'b','E','S','E',O_BSET|O_UNSZ,"bset",2,{RS,ABS8},3,	{{0x0d,0xff,0 },{0x00,0x00,ABS8 },{0x48,0xf8,RS }}},
{150,'b','E','I','E',O_BSET|O_UNSZ,"bset",2,{IMM4,ABS8},3,	{{0x0d,0xff,0 },{0x00,0x00,ABS8 },{0xc0,0xf0,IMM4 }}},
{150,'b','E','I','E',O_BSET|O_UNSZ,"bset",2,{IMM4,ABS16},4,	{{0x1d,0xff,0 },{0x00,0x00,ABS16 },{0x00,0x00,0 },{0xc0,0xf0,IMM4 }}},
{150,'b','E','S','E',O_BSET|O_UNSZ,"bset",2,{RS,RNIND_D16},4,	{{0xf8,0xf8,RN },{0x00,0x00,DISP16 },{0x00,0x00,0 },{0x48,0xf8,RS }}},
{150,'b','E','S','E',O_BSET|O_UNSZ,"bset",2,{RS,ABS16},4,	{{0x1d,0xff,0 },{0x00,0x00,ABS16 },{0x00,0x00,0 },{0x48,0xf8,RS }}},
{150,'b','E','I','E',O_BSET|O_UNSZ,"bset",2,{IMM4,RNIND_D16},4,	{{0xf8,0xf8,RN },{0x00,0x00,DISP16 },{0x00,0x00,0 },{0xc0,0xf0,IMM4 }}},
{151,'-','B','!','!',O_BRN|O_WORD,"brn.w",1,{PCREL16,0},3,	{{0x31,0xff,0 },{0x00,0x00,PCREL16 },{0x00,0x00,0 }}},
{152,'-','B','!','!',O_BRN|O_BYTE,"brn.b",1,{PCREL8,0},2,	{{0x21,0xff,0 },{0x00,0x00,PCREL8 }}},
{153,'-','B','!','!',O_BRN|O_UNSZ,"brn",1,{PCREL8,0},2,	{{0x21,0xff,0 },{0x00,0x00,PCREL8 }}},
{153,'-','B','!','!',O_BRN|O_UNSZ,"brn",1,{PCREL16,0},3,	{{0x31,0xff,0 },{0x00,0x00,PCREL16 },{0x00,0x00,0 }}},
{154,'-','B','!','!',O_BRA|O_WORD,"bra.w",1,{PCREL16,0},3,	{{0x30,0xff,0 },{0x00,0x00,PCREL16 },{0x00,0x00,0 }}},
{155,'-','B','!','!',O_BRA|O_BYTE,"bra.b",1,{PCREL8,0},2,	{{0x20,0xff,0 },{0x00,0x00,PCREL8 }}},
{156,'-','B','!','!',O_BRA|O_UNSZ,"bra",1,{PCREL8,0},2,	{{0x20,0xff,0 },{0x00,0x00,PCREL8 }}},
{156,'-','B','!','!',O_BRA|O_UNSZ,"bra",1,{PCREL16,0},3,	{{0x30,0xff,0 },{0x00,0x00,PCREL16 },{0x00,0x00,0 }}},
{157,'-','!','!','!',O_BPT|O_UNSZ,"bpt",0,{0,0},1,	{{0x0b,0xff,0 }}},
{158,'-','B','!','!',O_BPL|O_WORD,"bpl.w",1,{PCREL16,0},3,	{{0x3a,0xff,0 },{0x00,0x00,PCREL16 },{0x00,0x00,0 }}},
{159,'-','B','!','!',O_BPL|O_BYTE,"bpl.b",1,{PCREL8,0},2,	{{0x2a,0xff,0 },{0x00,0x00,PCREL8 }}},
{160,'-','B','!','!',O_BPL|O_UNSZ,"bpl",1,{PCREL8,0},2,	{{0x2a,0xff,0 },{0x00,0x00,PCREL8 }}},
{160,'-','B','!','!',O_BPL|O_UNSZ,"bpl",1,{PCREL16,0},3,	{{0x3a,0xff,0 },{0x00,0x00,PCREL16 },{0x00,0x00,0 }}},
{161,'b','E','S','E',O_BNOT|O_WORD,"bnot.w",2,{RS,RN},2,	{{0xa8,0xf8,RN },{0x68,0xf8,RS }}},
{161,'b','E','S','E',O_BNOT|O_WORD,"bnot.w",2,{RS,RNINC},2,	{{0xc8,0xf8,RN },{0x68,0xf8,RS }}},
{161,'b','E','I','E',O_BNOT|O_WORD,"bnot.w",2,{IMM4,RN},2,	{{0xa8,0xf8,RN },{0xe0,0xf0,IMM4 }}},
{161,'b','E','S','E',O_BNOT|O_WORD,"bnot.w",2,{RS,RNIND},2,	{{0xd8,0xf8,RN },{0x68,0xf8,RS }}},
{161,'b','E','I','E',O_BNOT|O_WORD,"bnot.w",2,{IMM4,RNDEC},2,	{{0xb8,0xf8,RN },{0xe0,0xf0,IMM4 }}},
{161,'b','E','S','E',O_BNOT|O_WORD,"bnot.w",2,{RS,RNDEC},2,	{{0xb8,0xf8,RN },{0x68,0xf8,RS }}},
{161,'b','E','I','E',O_BNOT|O_WORD,"bnot.w",2,{IMM4,RNIND},2,	{{0xd8,0xf8,RN },{0xe0,0xf0,IMM4 }}},
{161,'b','E','I','E',O_BNOT|O_WORD,"bnot.w",2,{IMM4,RNINC},2,	{{0xc8,0xf8,RN },{0xe0,0xf0,IMM4 }}},
{161,'b','E','I','E',O_BNOT|O_WORD,"bnot.w",2,{IMM4,RNIND_D8},3,	{{0xe8,0xf8,RN },{0x00,0x00,DISP8 },{0xe0,0xf0,IMM4 }}},
{161,'b','E','S','E',O_BNOT|O_WORD,"bnot.w",2,{RS,RNIND_D8},3,	{{0xe8,0xf8,RN },{0x00,0x00,DISP8 },{0x68,0xf8,RS }}},
{161,'b','E','I','E',O_BNOT|O_WORD,"bnot.w",2,{IMM4,ABS8},3,	{{0x0d,0xff,0 },{0x00,0x00,ABS8 },{0xe0,0xf0,IMM4 }}},
{161,'b','E','S','E',O_BNOT|O_WORD,"bnot.w",2,{RS,ABS8},3,	{{0x0d,0xff,0 },{0x00,0x00,ABS8 },{0x68,0xf8,RS }}},
{161,'b','E','I','E',O_BNOT|O_WORD,"bnot.w",2,{IMM4,ABS16},4,	{{0x1d,0xff,0 },{0x00,0x00,ABS16 },{0x00,0x00,0 },{0xe0,0xf0,IMM4 }}},
{161,'b','E','S','E',O_BNOT|O_WORD,"bnot.w",2,{RS,RNIND_D16},4,	{{0xf8,0xf8,RN },{0x00,0x00,DISP16 },{0x00,0x00,0 },{0x68,0xf8,RS }}},
{161,'b','E','S','E',O_BNOT|O_WORD,"bnot.w",2,{RS,ABS16},4,	{{0x1d,0xff,0 },{0x00,0x00,ABS16 },{0x00,0x00,0 },{0x68,0xf8,RS }}},
{161,'b','E','I','E',O_BNOT|O_WORD,"bnot.w",2,{IMM4,RNIND_D16},4,	{{0xf8,0xf8,RN },{0x00,0x00,DISP16 },{0x00,0x00,0 },{0xe0,0xf0,IMM4 }}},
{162,'b','E','S','E',O_BNOT|O_BYTE,"bnot.b",2,{RS,RN},2,	{{0xa0,0xf8,RN },{0x68,0xf8,RS }}},
{162,'b','E','I','E',O_BNOT|O_BYTE,"bnot.b",2,{IMM4,RNIND},2,	{{0xd0,0xf8,RN },{0xe0,0xf0,IMM4 }}},
{162,'b','E','I','E',O_BNOT|O_BYTE,"bnot.b",2,{IMM4,RN},2,	{{0xa0,0xf8,RN },{0xe0,0xf0,IMM4 }}},
{162,'b','E','S','E',O_BNOT|O_BYTE,"bnot.b",2,{RS,RNIND},2,	{{0xd0,0xf8,RN },{0x68,0xf8,RS }}},
{162,'b','E','S','E',O_BNOT|O_BYTE,"bnot.b",2,{RS,RNINC},2,	{{0xc0,0xf8,RN },{0x68,0xf8,RS }}},
{162,'b','E','S','E',O_BNOT|O_BYTE,"bnot.b",2,{RS,RNDEC},2,	{{0xb0,0xf8,RN },{0x68,0xf8,RS }}},
{162,'b','E','I','E',O_BNOT|O_BYTE,"bnot.b",2,{IMM4,RNDEC},2,	{{0xb0,0xf8,RN },{0xe0,0xf0,IMM4 }}},
{162,'b','E','I','E',O_BNOT|O_BYTE,"bnot.b",2,{IMM4,RNINC},2,	{{0xc0,0xf8,RN },{0xe0,0xf0,IMM4 }}},
{162,'b','E','I','E',O_BNOT|O_BYTE,"bnot.b",2,{IMM4,ABS8},3,	{{0x05,0xff,0 },{0x00,0x00,ABS8 },{0xe0,0xf0,IMM4 }}},
{162,'b','E','I','E',O_BNOT|O_BYTE,"bnot.b",2,{IMM4,RNIND_D8},3,	{{0xe0,0xf8,RN },{0x00,0x00,DISP8 },{0xe0,0xf0,IMM4 }}},
{162,'b','E','S','E',O_BNOT|O_BYTE,"bnot.b",2,{RS,RNIND_D8},3,	{{0xe0,0xf8,RN },{0x00,0x00,DISP8 },{0x68,0xf8,RS }}},
{162,'b','E','S','E',O_BNOT|O_BYTE,"bnot.b",2,{RS,ABS8},3,	{{0x05,0xff,0 },{0x00,0x00,ABS8 },{0x68,0xf8,RS }}},
{162,'b','E','S','E',O_BNOT|O_BYTE,"bnot.b",2,{RS,RNIND_D16},4,	{{0xf0,0xf8,RN },{0x00,0x00,DISP16 },{0x00,0x00,0 },{0x68,0xf8,RS }}},
{162,'b','E','I','E',O_BNOT|O_BYTE,"bnot.b",2,{IMM4,ABS16},4,	{{0x15,0xff,0 },{0x00,0x00,ABS16 },{0x00,0x00,0 },{0xe0,0xf0,IMM4 }}},
{162,'b','E','S','E',O_BNOT|O_BYTE,"bnot.b",2,{RS,ABS16},4,	{{0x15,0xff,0 },{0x00,0x00,ABS16 },{0x00,0x00,0 },{0x68,0xf8,RS }}},
{162,'b','E','I','E',O_BNOT|O_BYTE,"bnot.b",2,{IMM4,RNIND_D16},4,	{{0xf0,0xf8,RN },{0x00,0x00,DISP16 },{0x00,0x00,0 },{0xe0,0xf0,IMM4 }}},
{163,'b','E','S','E',O_BNOT|O_UNSZ,"bnot",2,{RS,RN},2,	{{0xa8,0xf8,RN },{0x68,0xf8,RS }}},
{163,'b','E','S','E',O_BNOT|O_UNSZ,"bnot",2,{RS,RNIND},2,	{{0xd8,0xf8,RN },{0x68,0xf8,RS }}},
{163,'b','E','I','E',O_BNOT|O_UNSZ,"bnot",2,{IMM4,RNIND},2,	{{0xd8,0xf8,RN },{0xe0,0xf0,IMM4 }}},
{163,'b','E','I','E',O_BNOT|O_UNSZ,"bnot",2,{IMM4,RN},2,	{{0xa8,0xf8,RN },{0xe0,0xf0,IMM4 }}},
{163,'b','E','S','E',O_BNOT|O_UNSZ,"bnot",2,{RS,RNINC},2,	{{0xc8,0xf8,RN },{0x68,0xf8,RS }}},
{163,'b','E','S','E',O_BNOT|O_UNSZ,"bnot",2,{RS,RNDEC},2,	{{0xb8,0xf8,RN },{0x68,0xf8,RS }}},
{163,'b','E','I','E',O_BNOT|O_UNSZ,"bnot",2,{IMM4,RNDEC},2,	{{0xb8,0xf8,RN },{0xe0,0xf0,IMM4 }}},
{163,'b','E','I','E',O_BNOT|O_UNSZ,"bnot",2,{IMM4,RNINC},2,	{{0xc8,0xf8,RN },{0xe0,0xf0,IMM4 }}},
{163,'b','E','S','E',O_BNOT|O_UNSZ,"bnot",2,{RS,ABS8},3,	{{0x0d,0xff,0 },{0x00,0x00,ABS8 },{0x68,0xf8,RS }}},
{163,'b','E','I','E',O_BNOT|O_UNSZ,"bnot",2,{IMM4,ABS8},3,	{{0x0d,0xff,0 },{0x00,0x00,ABS8 },{0xe0,0xf0,IMM4 }}},
{163,'b','E','S','E',O_BNOT|O_UNSZ,"bnot",2,{RS,RNIND_D8},3,	{{0xe8,0xf8,RN },{0x00,0x00,DISP8 },{0x68,0xf8,RS }}},
{163,'b','E','I','E',O_BNOT|O_UNSZ,"bnot",2,{IMM4,RNIND_D8},3,	{{0xe8,0xf8,RN },{0x00,0x00,DISP8 },{0xe0,0xf0,IMM4 }}},
{163,'b','E','S','E',O_BNOT|O_UNSZ,"bnot",2,{RS,RNIND_D16},4,	{{0xf8,0xf8,RN },{0x00,0x00,DISP16 },{0x00,0x00,0 },{0x68,0xf8,RS }}},
{163,'b','E','I','E',O_BNOT|O_UNSZ,"bnot",2,{IMM4,ABS16},4,	{{0x1d,0xff,0 },{0x00,0x00,ABS16 },{0x00,0x00,0 },{0xe0,0xf0,IMM4 }}},
{163,'b','E','S','E',O_BNOT|O_UNSZ,"bnot",2,{RS,ABS16},4,	{{0x1d,0xff,0 },{0x00,0x00,ABS16 },{0x00,0x00,0 },{0x68,0xf8,RS }}},
{163,'b','E','I','E',O_BNOT|O_UNSZ,"bnot",2,{IMM4,RNIND_D16},4,	{{0xf8,0xf8,RN },{0x00,0x00,DISP16 },{0x00,0x00,0 },{0xe0,0xf0,IMM4 }}},
{164,'-','B','!','!',O_BNE|O_WORD,"bne.w",1,{PCREL16,0},3,	{{0x36,0xff,0 },{0x00,0x00,PCREL16 },{0x00,0x00,0 }}},
{165,'-','B','!','!',O_BNE|O_BYTE,"bne.b",1,{PCREL8,0},2,	{{0x26,0xff,0 },{0x00,0x00,PCREL8 }}},
{166,'-','B','!','!',O_BNE|O_UNSZ,"bne",1,{PCREL8,0},2,	{{0x26,0xff,0 },{0x00,0x00,PCREL8 }}},
{166,'-','B','!','!',O_BNE|O_UNSZ,"bne",1,{PCREL16,0},3,	{{0x36,0xff,0 },{0x00,0x00,PCREL16 },{0x00,0x00,0 }}},
{167,'-','B','!','!',O_BMI|O_WORD,"bmi.w",1,{PCREL16,0},3,	{{0x3b,0xff,0 },{0x00,0x00,PCREL16 },{0x00,0x00,0 }}},
{168,'-','B','!','!',O_BMI|O_BYTE,"bmi.b",1,{PCREL8,0},2,	{{0x2b,0xff,0 },{0x00,0x00,PCREL8 }}},
{169,'-','B','!','!',O_BMI|O_UNSZ,"bmi",1,{PCREL8,0},2,	{{0x2b,0xff,0 },{0x00,0x00,PCREL8 }}},
{169,'-','B','!','!',O_BMI|O_UNSZ,"bmi",1,{PCREL16,0},3,	{{0x3b,0xff,0 },{0x00,0x00,PCREL16 },{0x00,0x00,0 }}},
{170,'-','B','!','!',O_BLT|O_WORD,"blt.w",1,{PCREL16,0},3,	{{0x3d,0xff,0 },{0x00,0x00,PCREL16 },{0x00,0x00,0 }}},
{171,'-','B','!','!',O_BLT|O_BYTE,"blt.b",1,{PCREL8,0},2,	{{0x2d,0xff,0 },{0x00,0x00,PCREL8 }}},
{172,'-','B','!','!',O_BLT|O_UNSZ,"blt",1,{PCREL8,0},2,	{{0x2d,0xff,0 },{0x00,0x00,PCREL8 }}},
{172,'-','B','!','!',O_BLT|O_UNSZ,"blt",1,{PCREL16,0},3,	{{0x3d,0xff,0 },{0x00,0x00,PCREL16 },{0x00,0x00,0 }}},
{173,'-','B','!','!',O_BLS|O_WORD,"bls.w",1,{PCREL16,0},3,	{{0x33,0xff,0 },{0x00,0x00,PCREL16 },{0x00,0x00,0 }}},
{174,'-','B','!','!',O_BLS|O_BYTE,"bls.b",1,{PCREL8,0},2,	{{0x23,0xff,0 },{0x00,0x00,PCREL8 }}},
{175,'-','B','!','!',O_BLS|O_UNSZ,"bls",1,{PCREL8,0},2,	{{0x23,0xff,0 },{0x00,0x00,PCREL8 }}},
{175,'-','B','!','!',O_BLS|O_UNSZ,"bls",1,{PCREL16,0},3,	{{0x33,0xff,0 },{0x00,0x00,PCREL16 },{0x00,0x00,0 }}},
{176,'-','B','!','!',O_BLO|O_WORD,"blo.w",1,{PCREL16,0},3,	{{0x35,0xff,0 },{0x00,0x00,PCREL16 },{0x00,0x00,0 }}},
{177,'-','B','!','!',O_BLO|O_BYTE,"blo.b",1,{PCREL8,0},2,	{{0x25,0xff,0 },{0x00,0x00,PCREL8 }}},
{178,'-','B','!','!',O_BLO|O_UNSZ,"blo",1,{PCREL8,0},2,	{{0x25,0xff,0 },{0x00,0x00,PCREL8 }}},
{178,'-','B','!','!',O_BLO|O_UNSZ,"blo",1,{PCREL16,0},3,	{{0x35,0xff,0 },{0x00,0x00,PCREL16 },{0x00,0x00,0 }}},
{179,'-','B','!','!',O_BLE|O_WORD,"ble.w",1,{PCREL16,0},3,	{{0x3f,0xff,0 },{0x00,0x00,PCREL16 },{0x00,0x00,0 }}},
{180,'-','B','!','!',O_BLE|O_BYTE,"ble.b",1,{PCREL8,0},2,	{{0x2f,0xff,0 },{0x00,0x00,PCREL8 }}},
{181,'-','B','!','!',O_BLE|O_UNSZ,"ble",1,{PCREL8,0},2,	{{0x2f,0xff,0 },{0x00,0x00,PCREL8 }}},
{181,'-','B','!','!',O_BLE|O_UNSZ,"ble",1,{PCREL16,0},3,	{{0x3f,0xff,0 },{0x00,0x00,PCREL16 },{0x00,0x00,0 }}},
{182,'-','B','!','!',O_BHS|O_WORD,"bhs.w",1,{PCREL16,0},3,	{{0x34,0xff,0 },{0x00,0x00,PCREL16 },{0x00,0x00,0 }}},
{183,'-','B','!','!',O_BHS|O_BYTE,"bhs.b",1,{PCREL8,0},2,	{{0x24,0xff,0 },{0x00,0x00,PCREL8 }}},
{184,'-','B','!','!',O_BHS|O_UNSZ,"bhs",1,{PCREL8,0},2,	{{0x24,0xff,0 },{0x00,0x00,PCREL8 }}},
{184,'-','B','!','!',O_BHS|O_UNSZ,"bhs",1,{PCREL16,0},3,	{{0x34,0xff,0 },{0x00,0x00,PCREL16 },{0x00,0x00,0 }}},
{185,'-','B','!','!',O_BHI|O_WORD,"bhi.w",1,{PCREL16,0},3,	{{0x32,0xff,0 },{0x00,0x00,PCREL16 },{0x00,0x00,0 }}},
{186,'-','B','!','!',O_BHI|O_BYTE,"bhi.b",1,{PCREL8,0},2,	{{0x22,0xff,0 },{0x00,0x00,PCREL8 }}},
{187,'-','B','!','!',O_BHI|O_UNSZ,"bhi",1,{PCREL8,0},2,	{{0x22,0xff,0 },{0x00,0x00,PCREL8 }}},
{187,'-','B','!','!',O_BHI|O_UNSZ,"bhi",1,{PCREL16,0},3,	{{0x32,0xff,0 },{0x00,0x00,PCREL16 },{0x00,0x00,0 }}},
{188,'-','B','!','!',O_BGT|O_WORD,"bgt.w",1,{PCREL16,0},3,	{{0x3e,0xff,0 },{0x00,0x00,PCREL16 },{0x00,0x00,0 }}},
{189,'-','B','!','!',O_BGT|O_BYTE,"bgt.b",1,{PCREL8,0},2,	{{0x2e,0xff,0 },{0x00,0x00,PCREL8 }}},
{190,'-','B','!','!',O_BGT|O_UNSZ,"bgt",1,{PCREL8,0},2,	{{0x2e,0xff,0 },{0x00,0x00,PCREL8 }}},
{190,'-','B','!','!',O_BGT|O_UNSZ,"bgt",1,{PCREL16,0},3,	{{0x3e,0xff,0 },{0x00,0x00,PCREL16 },{0x00,0x00,0 }}},
{191,'-','B','!','!',O_BGE|O_WORD,"bge.w",1,{PCREL16,0},3,	{{0x3c,0xff,0 },{0x00,0x00,PCREL16 },{0x00,0x00,0 }}},
{192,'-','B','!','!',O_BGE|O_BYTE,"bge.b",1,{PCREL8,0},2,	{{0x2c,0xff,0 },{0x00,0x00,PCREL8 }}},
{193,'-','B','!','!',O_BGE|O_UNSZ,"bge",1,{PCREL8,0},2,	{{0x2c,0xff,0 },{0x00,0x00,PCREL8 }}},
{193,'-','B','!','!',O_BGE|O_UNSZ,"bge",1,{PCREL16,0},3,	{{0x3c,0xff,0 },{0x00,0x00,PCREL16 },{0x00,0x00,0 }}},
{194,'-','B','!','!',O_BF|O_WORD,"bf.w",1,{PCREL16,0},3,	{{0x31,0xff,0 },{0x00,0x00,PCREL16 },{0x00,0x00,0 }}},
{195,'-','B','!','!',O_BF|O_BYTE,"bf.b",1,{PCREL8,0},2,	{{0x21,0xff,0 },{0x00,0x00,PCREL8 }}},
{196,'-','B','!','!',O_BF|O_UNSZ,"bf",1,{PCREL8,0},2,	{{0x21,0xff,0 },{0x00,0x00,PCREL8 }}},
{196,'-','B','!','!',O_BF|O_UNSZ,"bf",1,{PCREL16,0},3,	{{0x31,0xff,0 },{0x00,0x00,PCREL16 },{0x00,0x00,0 }}},
{197,'-','B','!','!',O_BEQ|O_WORD,"beq.w",1,{PCREL16,0},3,	{{0x37,0xff,0 },{0x00,0x00,PCREL16 },{0x00,0x00,0 }}},
{198,'-','B','!','!',O_BEQ|O_BYTE,"beq.b",1,{PCREL8,0},2,	{{0x27,0xff,0 },{0x00,0x00,PCREL8 }}},
{199,'-','B','!','!',O_BEQ|O_UNSZ,"beq",1,{PCREL8,0},2,	{{0x27,0xff,0 },{0x00,0x00,PCREL8 }}},
{199,'-','B','!','!',O_BEQ|O_UNSZ,"beq",1,{PCREL16,0},3,	{{0x37,0xff,0 },{0x00,0x00,PCREL16 },{0x00,0x00,0 }}},
{200,'-','B','!','!',O_BCS|O_WORD,"bcs.w",1,{PCREL16,0},3,	{{0x35,0xff,0 },{0x00,0x00,PCREL16 },{0x00,0x00,0 }}},
{201,'-','B','!','!',O_BCS|O_BYTE,"bcs.b",1,{PCREL8,0},2,	{{0x25,0xff,0 },{0x00,0x00,PCREL8 }}},
{202,'-','B','!','!',O_BCS|O_UNSZ,"bcs",1,{PCREL8,0},2,	{{0x25,0xff,0 },{0x00,0x00,PCREL8 }}},
{202,'-','B','!','!',O_BCS|O_UNSZ,"bcs",1,{PCREL16,0},3,	{{0x35,0xff,0 },{0x00,0x00,PCREL16 },{0x00,0x00,0 }}},
{203,'b','E','S','E',O_BCLR|O_WORD,"bclr.w",2,{RS,RN},2,	{{0xa8,0xf8,RN },{0x58,0xf8,RS }}},
{203,'b','E','S','E',O_BCLR|O_WORD,"bclr.w",2,{RS,RNDEC},2,	{{0xb8,0xf8,RN },{0x58,0xf8,RS }}},
{203,'b','E','I','E',O_BCLR|O_WORD,"bclr.w",2,{IMM4,RNDEC},2,	{{0xb8,0xf8,RN },{0xd0,0xf0,IMM4 }}},
{203,'b','E','S','E',O_BCLR|O_WORD,"bclr.w",2,{RS,RNIND},2,	{{0xd8,0xf8,RN },{0x58,0xf8,RS }}},
{203,'b','E','S','E',O_BCLR|O_WORD,"bclr.w",2,{RS,RNINC},2,	{{0xc8,0xf8,RN },{0x58,0xf8,RS }}},
{203,'b','E','I','E',O_BCLR|O_WORD,"bclr.w",2,{IMM4,RNINC},2,	{{0xc8,0xf8,RN },{0xd0,0xf0,IMM4 }}},
{203,'b','E','I','E',O_BCLR|O_WORD,"bclr.w",2,{IMM4,RN},2,	{{0xa8,0xf8,RN },{0xd0,0xf0,IMM4 }}},
{203,'b','E','I','E',O_BCLR|O_WORD,"bclr.w",2,{IMM4,RNIND},2,	{{0xd8,0xf8,RN },{0xd0,0xf0,IMM4 }}},
{203,'b','E','S','E',O_BCLR|O_WORD,"bclr.w",2,{RS,ABS8},3,	{{0x0d,0xff,0 },{0x00,0x00,ABS8 },{0x58,0xf8,RS }}},
{203,'b','E','S','E',O_BCLR|O_WORD,"bclr.w",2,{RS,RNIND_D8},3,	{{0xe8,0xf8,RN },{0x00,0x00,DISP8 },{0x58,0xf8,RS }}},
{203,'b','E','I','E',O_BCLR|O_WORD,"bclr.w",2,{IMM4,ABS8},3,	{{0x0d,0xff,0 },{0x00,0x00,ABS8 },{0xd0,0xf0,IMM4 }}},
{203,'b','E','I','E',O_BCLR|O_WORD,"bclr.w",2,{IMM4,RNIND_D8},3,	{{0xe8,0xf8,RN },{0x00,0x00,DISP8 },{0xd0,0xf0,IMM4 }}},
{203,'b','E','S','E',O_BCLR|O_WORD,"bclr.w",2,{RS,RNIND_D16},4,	{{0xf8,0xf8,RN },{0x00,0x00,DISP16 },{0x00,0x00,0 },{0x58,0xf8,RS }}},
{203,'b','E','S','E',O_BCLR|O_WORD,"bclr.w",2,{RS,ABS16},4,	{{0x1d,0xff,0 },{0x00,0x00,ABS16 },{0x00,0x00,0 },{0x58,0xf8,RS }}},
{203,'b','E','I','E',O_BCLR|O_WORD,"bclr.w",2,{IMM4,ABS16},4,	{{0x1d,0xff,0 },{0x00,0x00,ABS16 },{0x00,0x00,0 },{0xd0,0xf0,IMM4 }}},
{203,'b','E','I','E',O_BCLR|O_WORD,"bclr.w",2,{IMM4,RNIND_D16},4,	{{0xf8,0xf8,RN },{0x00,0x00,DISP16 },{0x00,0x00,0 },{0xd0,0xf0,IMM4 }}},
{204,'b','E','S','E',O_BCLR|O_BYTE,"bclr.b",2,{RS,RN},2,	{{0xa0,0xf8,RN },{0x58,0xf8,RS }}},
{204,'b','E','I','E',O_BCLR|O_BYTE,"bclr.b",2,{IMM4,RN},2,	{{0xa0,0xf8,RN },{0xd0,0xf0,IMM4 }}},
{204,'b','E','I','E',O_BCLR|O_BYTE,"bclr.b",2,{IMM4,RNIND},2,	{{0xd0,0xf8,RN },{0xd0,0xf0,IMM4 }}},
{204,'b','E','S','E',O_BCLR|O_BYTE,"bclr.b",2,{RS,RNIND},2,	{{0xd0,0xf8,RN },{0x58,0xf8,RS }}},
{204,'b','E','S','E',O_BCLR|O_BYTE,"bclr.b",2,{RS,RNINC},2,	{{0xc0,0xf8,RN },{0x58,0xf8,RS }}},
{204,'b','E','S','E',O_BCLR|O_BYTE,"bclr.b",2,{RS,RNDEC},2,	{{0xb0,0xf8,RN },{0x58,0xf8,RS }}},
{204,'b','E','I','E',O_BCLR|O_BYTE,"bclr.b",2,{IMM4,RNINC},2,	{{0xc0,0xf8,RN },{0xd0,0xf0,IMM4 }}},
{204,'b','E','I','E',O_BCLR|O_BYTE,"bclr.b",2,{IMM4,RNDEC},2,	{{0xb0,0xf8,RN },{0xd0,0xf0,IMM4 }}},
{204,'b','E','S','E',O_BCLR|O_BYTE,"bclr.b",2,{RS,RNIND_D8},3,	{{0xe0,0xf8,RN },{0x00,0x00,DISP8 },{0x58,0xf8,RS }}},
{204,'b','E','I','E',O_BCLR|O_BYTE,"bclr.b",2,{IMM4,ABS8},3,	{{0x05,0xff,0 },{0x00,0x00,ABS8 },{0xd0,0xf0,IMM4 }}},
{204,'b','E','S','E',O_BCLR|O_BYTE,"bclr.b",2,{RS,ABS8},3,	{{0x05,0xff,0 },{0x00,0x00,ABS8 },{0x58,0xf8,RS }}},
{204,'b','E','I','E',O_BCLR|O_BYTE,"bclr.b",2,{IMM4,RNIND_D8},3,	{{0xe0,0xf8,RN },{0x00,0x00,DISP8 },{0xd0,0xf0,IMM4 }}},
{204,'b','E','I','E',O_BCLR|O_BYTE,"bclr.b",2,{IMM4,ABS16},4,	{{0x15,0xff,0 },{0x00,0x00,ABS16 },{0x00,0x00,0 },{0xd0,0xf0,IMM4 }}},
{204,'b','E','S','E',O_BCLR|O_BYTE,"bclr.b",2,{RS,RNIND_D16},4,	{{0xf0,0xf8,RN },{0x00,0x00,DISP16 },{0x00,0x00,0 },{0x58,0xf8,RS }}},
{204,'b','E','S','E',O_BCLR|O_BYTE,"bclr.b",2,{RS,ABS16},4,	{{0x15,0xff,0 },{0x00,0x00,ABS16 },{0x00,0x00,0 },{0x58,0xf8,RS }}},
{204,'b','E','I','E',O_BCLR|O_BYTE,"bclr.b",2,{IMM4,RNIND_D16},4,	{{0xf0,0xf8,RN },{0x00,0x00,DISP16 },{0x00,0x00,0 },{0xd0,0xf0,IMM4 }}},
{205,'b','E','S','E',O_BCLR|O_UNSZ,"bclr",2,{RS,RN},2,	{{0xa8,0xf8,RN },{0x58,0xf8,RS }}},
{205,'b','E','I','E',O_BCLR|O_UNSZ,"bclr",2,{IMM4,RNDEC},2,	{{0xb8,0xf8,RN },{0xd0,0xf0,IMM4 }}},
{205,'b','E','I','E',O_BCLR|O_UNSZ,"bclr",2,{IMM4,RNINC},2,	{{0xc8,0xf8,RN },{0xd0,0xf0,IMM4 }}},
{205,'b','E','S','E',O_BCLR|O_UNSZ,"bclr",2,{RS,RNIND},2,	{{0xd8,0xf8,RN },{0x58,0xf8,RS }}},
{205,'b','E','S','E',O_BCLR|O_UNSZ,"bclr",2,{RS,RNINC},2,	{{0xc8,0xf8,RN },{0x58,0xf8,RS }}},
{205,'b','E','S','E',O_BCLR|O_UNSZ,"bclr",2,{RS,RNDEC},2,	{{0xb8,0xf8,RN },{0x58,0xf8,RS }}},
{205,'b','E','I','E',O_BCLR|O_UNSZ,"bclr",2,{IMM4,RNIND},2,	{{0xd8,0xf8,RN },{0xd0,0xf0,IMM4 }}},
{205,'b','E','I','E',O_BCLR|O_UNSZ,"bclr",2,{IMM4,RN},2,	{{0xa8,0xf8,RN },{0xd0,0xf0,IMM4 }}},
{205,'b','E','S','E',O_BCLR|O_UNSZ,"bclr",2,{RS,RNIND_D8},3,	{{0xe8,0xf8,RN },{0x00,0x00,DISP8 },{0x58,0xf8,RS }}},
{205,'b','E','I','E',O_BCLR|O_UNSZ,"bclr",2,{IMM4,ABS8},3,	{{0x0d,0xff,0 },{0x00,0x00,ABS8 },{0xd0,0xf0,IMM4 }}},
{205,'b','E','S','E',O_BCLR|O_UNSZ,"bclr",2,{RS,ABS8},3,	{{0x0d,0xff,0 },{0x00,0x00,ABS8 },{0x58,0xf8,RS }}},
{205,'b','E','I','E',O_BCLR|O_UNSZ,"bclr",2,{IMM4,RNIND_D8},3,	{{0xe8,0xf8,RN },{0x00,0x00,DISP8 },{0xd0,0xf0,IMM4 }}},
{205,'b','E','I','E',O_BCLR|O_UNSZ,"bclr",2,{IMM4,ABS16},4,	{{0x1d,0xff,0 },{0x00,0x00,ABS16 },{0x00,0x00,0 },{0xd0,0xf0,IMM4 }}},
{205,'b','E','S','E',O_BCLR|O_UNSZ,"bclr",2,{RS,RNIND_D16},4,	{{0xf8,0xf8,RN },{0x00,0x00,DISP16 },{0x00,0x00,0 },{0x58,0xf8,RS }}},
{205,'b','E','S','E',O_BCLR|O_UNSZ,"bclr",2,{RS,ABS16},4,	{{0x1d,0xff,0 },{0x00,0x00,ABS16 },{0x00,0x00,0 },{0x58,0xf8,RS }}},
{205,'b','E','I','E',O_BCLR|O_UNSZ,"bclr",2,{IMM4,RNIND_D16},4,	{{0xf8,0xf8,RN },{0x00,0x00,DISP16 },{0x00,0x00,0 },{0xd0,0xf0,IMM4 }}},
{206,'-','B','!','!',O_BCC|O_WORD,"bcc.w",1,{PCREL16,0},3,	{{0x34,0xff,0 },{0x00,0x00,PCREL16 },{0x00,0x00,0 }}},
{207,'-','B','!','!',O_BCC|O_BYTE,"bcc.b",1,{PCREL8,0},2,	{{0x24,0xff,0 },{0x00,0x00,PCREL8 }}},
{208,'-','B','!','!',O_BCC|O_UNSZ,"bcc",1,{PCREL8,0},2,	{{0x24,0xff,0 },{0x00,0x00,PCREL8 }}},
{208,'-','B','!','!',O_BCC|O_UNSZ,"bcc",1,{PCREL16,0},3,	{{0x34,0xff,0 },{0x00,0x00,PCREL16 },{0x00,0x00,0 }}},
{209,'s','I','S','S',O_ANDC|O_WORD,"andc.w",2,{IMM16,CRW},4,	{{0x0c,0xff,0 },{0x00,0x00,IMM16 },{0x00,0x00,0 },{0x58,0xf8,CRW }}},
{210,'s','I','S','S',O_ANDC|O_BYTE,"andc.b",2,{IMM8,CRB},3,	{{0x04,0xff,0 },{0x00,0x00,IMM8 },{0x58,0xf8,CRB }}},
{211,'s','I','S','S',O_ANDC|O_UNSZ,"andc",2,{IMM8,CRB},3,	{{0x04,0xff,0 },{0x00,0x00,IMM8 },{0x58,0xf8,CRB }}},
{211,'s','I','S','S',O_ANDC|O_UNSZ,"andc",2,{IMM16,CRW},4,	{{0x0c,0xff,0 },{0x00,0x00,IMM16 },{0x00,0x00,0 },{0x58,0xf8,CRW }}},
{212,'m','E','D','D',O_AND|O_WORD,"and.w",2,{RN,RD},2,	{{0xa8,0xf8,RN },{0x50,0xf8,RD }}},
{212,'m','E','D','D',O_AND|O_WORD,"and.w",2,{RNIND,RD},2,	{{0xd8,0xf8,RN },{0x50,0xf8,RD }}},
{212,'m','E','D','D',O_AND|O_WORD,"and.w",2,{RNDEC,RD},2,	{{0xb8,0xf8,RN },{0x50,0xf8,RD }}},
{212,'m','E','D','D',O_AND|O_WORD,"and.w",2,{RNINC,RD},2,	{{0xc8,0xf8,RN },{0x50,0xf8,RD }}},
{212,'m','E','D','D',O_AND|O_WORD,"and.w",2,{ABS8,RD},3,	{{0x0d,0xff,0 },{0x00,0x00,ABS8 },{0x50,0xf8,RD }}},
{212,'m','E','D','D',O_AND|O_WORD,"and.w",2,{RNIND_D8,RD},3,	{{0xe8,0xf8,RN },{0x00,0x00,DISP8 },{0x50,0xf8,RD }}},
{212,'m','E','D','D',O_AND|O_WORD,"and.w",2,{ABS16,RD},4,	{{0x1d,0xff,0 },{0x00,0x00,ABS16 },{0x00,0x00,0 },{0x50,0xf8,RD }}},
{212,'m','E','D','D',O_AND|O_WORD,"and.w",2,{IMM16,RD},4,	{{0x0c,0xff,0 },{0x00,0x00,IMM16 },{0x00,0x00,0 },{0x50,0xf8,RD }}},
{212,'m','E','D','D',O_AND|O_WORD,"and.w",2,{RNIND_D16,RD},4,	{{0xf8,0xf8,RN },{0x00,0x00,DISP16 },{0x00,0x00,0 },{0x50,0xf8,RD }}},
{213,'m','E','D','D',O_AND|O_BYTE,"and.b",2,{RN,RD},2,	{{0xa0,0xf8,RN },{0x50,0xf8,RD }}},
{213,'m','E','D','D',O_AND|O_BYTE,"and.b",2,{RNDEC,RD},2,	{{0xb0,0xf8,RN },{0x50,0xf8,RD }}},
{213,'m','E','D','D',O_AND|O_BYTE,"and.b",2,{RNINC,RD},2,	{{0xc0,0xf8,RN },{0x50,0xf8,RD }}},
{213,'m','E','D','D',O_AND|O_BYTE,"and.b",2,{RNIND,RD},2,	{{0xd0,0xf8,RN },{0x50,0xf8,RD }}},
{213,'m','E','D','D',O_AND|O_BYTE,"and.b",2,{ABS8,RD},3,	{{0x05,0xff,0 },{0x00,0x00,ABS8 },{0x50,0xf8,RD }}},
{213,'m','E','D','D',O_AND|O_BYTE,"and.b",2,{IMM8,RD},3,	{{0x04,0xff,0 },{0x00,0x00,IMM8 },{0x50,0xf8,RD }}},
{213,'m','E','D','D',O_AND|O_BYTE,"and.b",2,{RNIND_D8,RD},3,	{{0xe0,0xf8,RN },{0x00,0x00,DISP8 },{0x50,0xf8,RD }}},
{213,'m','E','D','D',O_AND|O_BYTE,"and.b",2,{ABS16,RD},4,	{{0x15,0xff,0 },{0x00,0x00,ABS16 },{0x00,0x00,0 },{0x50,0xf8,RD }}},
{213,'m','E','D','D',O_AND|O_BYTE,"and.b",2,{RNIND_D16,RD},4,	{{0xf0,0xf8,RN },{0x00,0x00,DISP16 },{0x00,0x00,0 },{0x50,0xf8,RD }}},
{214,'m','E','D','D',O_AND|O_UNSZ,"and",2,{RN,RD},2,	{{0xa8,0xf8,RN },{0x50,0xf8,RD }}},
{214,'m','E','D','D',O_AND|O_UNSZ,"and",2,{RNDEC,RD},2,	{{0xb8,0xf8,RN },{0x50,0xf8,RD }}},
{214,'m','E','D','D',O_AND|O_UNSZ,"and",2,{RNINC,RD},2,	{{0xc8,0xf8,RN },{0x50,0xf8,RD }}},
{214,'m','E','D','D',O_AND|O_UNSZ,"and",2,{RNIND,RD},2,	{{0xd8,0xf8,RN },{0x50,0xf8,RD }}},
{214,'m','E','D','D',O_AND|O_UNSZ,"and",2,{ABS8,RD},3,	{{0x0d,0xff,0 },{0x00,0x00,ABS8 },{0x50,0xf8,RD }}},
{214,'m','E','D','D',O_AND|O_UNSZ,"and",2,{RNIND_D8,RD},3,	{{0xe8,0xf8,RN },{0x00,0x00,DISP8 },{0x50,0xf8,RD }}},
{214,'m','E','D','D',O_AND|O_UNSZ,"and",2,{IMM16,RD},4,	{{0x0c,0xff,0 },{0x00,0x00,IMM16 },{0x00,0x00,0 },{0x50,0xf8,RD }}},
{214,'m','E','D','D',O_AND|O_UNSZ,"and",2,{ABS16,RD},4,	{{0x1d,0xff,0 },{0x00,0x00,ABS16 },{0x00,0x00,0 },{0x50,0xf8,RD }}},
{214,'m','E','D','D',O_AND|O_UNSZ,"and",2,{RNIND_D16,RD},4,	{{0xf8,0xf8,RN },{0x00,0x00,DISP16 },{0x00,0x00,0 },{0x50,0xf8,RD }}},
{215,'a','E','D','D',O_ADDX|O_WORD,"addx.w",2,{RN,RD},2,	{{0xa8,0xf8,RN },{0xa0,0xf8,RD }}},
{215,'a','E','D','D',O_ADDX|O_WORD,"addx.w",2,{RNIND,RD},2,	{{0xd8,0xf8,RN },{0xa0,0xf8,RD }}},
{215,'a','E','D','D',O_ADDX|O_WORD,"addx.w",2,{RNDEC,RD},2,	{{0xb8,0xf8,RN },{0xa0,0xf8,RD }}},
{215,'a','E','D','D',O_ADDX|O_WORD,"addx.w",2,{RNINC,RD},2,	{{0xc8,0xf8,RN },{0xa0,0xf8,RD }}},
{215,'a','E','D','D',O_ADDX|O_WORD,"addx.w",2,{ABS8,RD},3,	{{0x0d,0xff,0 },{0x00,0x00,ABS8 },{0xa0,0xf8,RD }}},
{215,'a','E','D','D',O_ADDX|O_WORD,"addx.w",2,{RNIND_D8,RD},3,	{{0xe8,0xf8,RN },{0x00,0x00,DISP8 },{0xa0,0xf8,RD }}},
{215,'a','E','D','D',O_ADDX|O_WORD,"addx.w",2,{ABS16,RD},4,	{{0x1d,0xff,0 },{0x00,0x00,ABS16 },{0x00,0x00,0 },{0xa0,0xf8,RD }}},
{215,'a','E','D','D',O_ADDX|O_WORD,"addx.w",2,{IMM16,RD},4,	{{0x0c,0xff,0 },{0x00,0x00,IMM16 },{0x00,0x00,0 },{0xa0,0xf8,RD }}},
{215,'a','E','D','D',O_ADDX|O_WORD,"addx.w",2,{RNIND_D16,RD},4,	{{0xf8,0xf8,RN },{0x00,0x00,DISP16 },{0x00,0x00,0 },{0xa0,0xf8,RD }}},
{216,'a','E','D','D',O_ADDX|O_BYTE,"addx.b",2,{RN,RD},2,	{{0xa0,0xf8,RN },{0xa0,0xf8,RD }}},
{216,'a','E','D','D',O_ADDX|O_BYTE,"addx.b",2,{RNIND,RD},2,	{{0xd0,0xf8,RN },{0xa0,0xf8,RD }}},
{216,'a','E','D','D',O_ADDX|O_BYTE,"addx.b",2,{RNDEC,RD},2,	{{0xb0,0xf8,RN },{0xa0,0xf8,RD }}},
{216,'a','E','D','D',O_ADDX|O_BYTE,"addx.b",2,{RNINC,RD},2,	{{0xc0,0xf8,RN },{0xa0,0xf8,RD }}},
{216,'a','E','D','D',O_ADDX|O_BYTE,"addx.b",2,{IMM8,RD},3,	{{0x04,0xff,0 },{0x00,0x00,IMM8 },{0xa0,0xf8,RD }}},
{216,'a','E','D','D',O_ADDX|O_BYTE,"addx.b",2,{ABS8,RD},3,	{{0x05,0xff,0 },{0x00,0x00,ABS8 },{0xa0,0xf8,RD }}},
{216,'a','E','D','D',O_ADDX|O_BYTE,"addx.b",2,{RNIND_D8,RD},3,	{{0xe0,0xf8,RN },{0x00,0x00,DISP8 },{0xa0,0xf8,RD }}},
{216,'a','E','D','D',O_ADDX|O_BYTE,"addx.b",2,{ABS16,RD},4,	{{0x15,0xff,0 },{0x00,0x00,ABS16 },{0x00,0x00,0 },{0xa0,0xf8,RD }}},
{216,'a','E','D','D',O_ADDX|O_BYTE,"addx.b",2,{RNIND_D16,RD},4,	{{0xf0,0xf8,RN },{0x00,0x00,DISP16 },{0x00,0x00,0 },{0xa0,0xf8,RD }}},
{217,'a','E','D','D',O_ADDX|O_UNSZ,"addx",2,{RN,RD},2,	{{0xa8,0xf8,RN },{0xa0,0xf8,RD }}},
{217,'a','E','D','D',O_ADDX|O_UNSZ,"addx",2,{RNINC,RD},2,	{{0xc8,0xf8,RN },{0xa0,0xf8,RD }}},
{217,'a','E','D','D',O_ADDX|O_UNSZ,"addx",2,{RNIND,RD},2,	{{0xd8,0xf8,RN },{0xa0,0xf8,RD }}},
{217,'a','E','D','D',O_ADDX|O_UNSZ,"addx",2,{RNDEC,RD},2,	{{0xb8,0xf8,RN },{0xa0,0xf8,RD }}},
{217,'a','E','D','D',O_ADDX|O_UNSZ,"addx",2,{ABS8,RD},3,	{{0x0d,0xff,0 },{0x00,0x00,ABS8 },{0xa0,0xf8,RD }}},
{217,'a','E','D','D',O_ADDX|O_UNSZ,"addx",2,{RNIND_D8,RD},3,	{{0xe8,0xf8,RN },{0x00,0x00,DISP8 },{0xa0,0xf8,RD }}},
{217,'a','E','D','D',O_ADDX|O_UNSZ,"addx",2,{ABS16,RD},4,	{{0x1d,0xff,0 },{0x00,0x00,ABS16 },{0x00,0x00,0 },{0xa0,0xf8,RD }}},
{217,'a','E','D','D',O_ADDX|O_UNSZ,"addx",2,{IMM16,RD},4,	{{0x0c,0xff,0 },{0x00,0x00,IMM16 },{0x00,0x00,0 },{0xa0,0xf8,RD }}},
{217,'a','E','D','D',O_ADDX|O_UNSZ,"addx",2,{RNIND_D16,RD},4,	{{0xf8,0xf8,RN },{0x00,0x00,DISP16 },{0x00,0x00,0 },{0xa0,0xf8,RD }}},
{218,'-','E','D','D',O_ADDS|O_WORD,"adds.w",2,{RN,RD},2,	{{0xa8,0xf8,RN },{0x28,0xf8,RD }}},
{218,'-','E','D','D',O_ADDS|O_WORD,"adds.w",2,{RNIND,RD},2,	{{0xd8,0xf8,RN },{0x28,0xf8,RD }}},
{218,'-','E','D','D',O_ADDS|O_WORD,"adds.w",2,{RNDEC,RD},2,	{{0xb8,0xf8,RN },{0x28,0xf8,RD }}},
{218,'-','E','D','D',O_ADDS|O_WORD,"adds.w",2,{RNINC,RD},2,	{{0xc8,0xf8,RN },{0x28,0xf8,RD }}},
{218,'-','E','D','D',O_ADDS|O_WORD,"adds.w",2,{ABS8,RD},3,	{{0x0d,0xff,0 },{0x00,0x00,ABS8 },{0x28,0xf8,RD }}},
{218,'-','E','D','D',O_ADDS|O_WORD,"adds.w",2,{RNIND_D8,RD},3,	{{0xe8,0xf8,RN },{0x00,0x00,DISP8 },{0x28,0xf8,RD }}},
{218,'-','E','D','D',O_ADDS|O_WORD,"adds.w",2,{ABS16,RD},4,	{{0x1d,0xff,0 },{0x00,0x00,ABS16 },{0x00,0x00,0 },{0x28,0xf8,RD }}},
{218,'-','E','D','D',O_ADDS|O_WORD,"adds.w",2,{IMM16,RD},4,	{{0x0c,0xff,0 },{0x00,0x00,IMM16 },{0x00,0x00,0 },{0x28,0xf8,RD }}},
{218,'-','E','D','D',O_ADDS|O_WORD,"adds.w",2,{RNIND_D16,RD},4,	{{0xf8,0xf8,RN },{0x00,0x00,DISP16 },{0x00,0x00,0 },{0x28,0xf8,RD }}},
{219,'-','E','D','D',O_ADDS|O_BYTE,"adds.b",2,{RN,RD},2,	{{0xa0,0xf8,RN },{0x28,0xf8,RD }}},
{219,'-','E','D','D',O_ADDS|O_BYTE,"adds.b",2,{RNINC,RD},2,	{{0xc0,0xf8,RN },{0x28,0xf8,RD }}},
{219,'-','E','D','D',O_ADDS|O_BYTE,"adds.b",2,{RNDEC,RD},2,	{{0xb0,0xf8,RN },{0x28,0xf8,RD }}},
{219,'-','E','D','D',O_ADDS|O_BYTE,"adds.b",2,{RNIND,RD},2,	{{0xd0,0xf8,RN },{0x28,0xf8,RD }}},
{219,'-','E','D','D',O_ADDS|O_BYTE,"adds.b",2,{ABS8,RD},3,	{{0x05,0xff,0 },{0x00,0x00,ABS8 },{0x28,0xf8,RD }}},
{219,'-','E','D','D',O_ADDS|O_BYTE,"adds.b",2,{IMM8,RD},3,	{{0x04,0xff,0 },{0x00,0x00,IMM8 },{0x28,0xf8,RD }}},
{219,'-','E','D','D',O_ADDS|O_BYTE,"adds.b",2,{RNIND_D8,RD},3,	{{0xe0,0xf8,RN },{0x00,0x00,DISP8 },{0x28,0xf8,RD }}},
{219,'-','E','D','D',O_ADDS|O_BYTE,"adds.b",2,{ABS16,RD},4,	{{0x15,0xff,0 },{0x00,0x00,ABS16 },{0x00,0x00,0 },{0x28,0xf8,RD }}},
{219,'-','E','D','D',O_ADDS|O_BYTE,"adds.b",2,{RNIND_D16,RD},4,	{{0xf0,0xf8,RN },{0x00,0x00,DISP16 },{0x00,0x00,0 },{0x28,0xf8,RD }}},
{220,'-','E','D','D',O_ADDS|O_UNSZ,"adds",2,{RN,RD},2,	{{0xa8,0xf8,RN },{0x28,0xf8,RD }}},
{220,'-','E','D','D',O_ADDS|O_UNSZ,"adds",2,{RNIND,RD},2,	{{0xd8,0xf8,RN },{0x28,0xf8,RD }}},
{220,'-','E','D','D',O_ADDS|O_UNSZ,"adds",2,{RNINC,RD},2,	{{0xc8,0xf8,RN },{0x28,0xf8,RD }}},
{220,'-','E','D','D',O_ADDS|O_UNSZ,"adds",2,{RNDEC,RD},2,	{{0xb8,0xf8,RN },{0x28,0xf8,RD }}},
{220,'-','E','D','D',O_ADDS|O_UNSZ,"adds",2,{ABS8,RD},3,	{{0x0d,0xff,0 },{0x00,0x00,ABS8 },{0x28,0xf8,RD }}},
{220,'-','E','D','D',O_ADDS|O_UNSZ,"adds",2,{RNIND_D8,RD},3,	{{0xe8,0xf8,RN },{0x00,0x00,DISP8 },{0x28,0xf8,RD }}},
{220,'-','E','D','D',O_ADDS|O_UNSZ,"adds",2,{ABS16,RD},4,	{{0x1d,0xff,0 },{0x00,0x00,ABS16 },{0x00,0x00,0 },{0x28,0xf8,RD }}},
{220,'-','E','D','D',O_ADDS|O_UNSZ,"adds",2,{IMM16,RD},4,	{{0x0c,0xff,0 },{0x00,0x00,IMM16 },{0x00,0x00,0 },{0x28,0xf8,RD }}},
{220,'-','E','D','D',O_ADDS|O_UNSZ,"adds",2,{RNIND_D16,RD},4,	{{0xf8,0xf8,RN },{0x00,0x00,DISP16 },{0x00,0x00,0 },{0x28,0xf8,RD }}},
{221,'a','I','E','E',O_ADD|O_WORD,"add:q.w",2,{QIM,RN},2,	{{0xa8,0xf8,RN },{0x08,0xf8,QIM }}},
{221,'a','I','E','E',O_ADD|O_WORD,"add:q.w",2,{QIM,RNINC},2,	{{0xc8,0xf8,RN },{0x08,0xf8,QIM }}},
{221,'a','I','E','E',O_ADD|O_WORD,"add:q.w",2,{QIM,RNDEC},2,	{{0xb8,0xf8,RN },{0x08,0xf8,QIM }}},
{221,'a','I','E','E',O_ADD|O_WORD,"add:q.w",2,{QIM,RNIND},2,	{{0xd8,0xf8,RN },{0x08,0xf8,QIM }}},
{221,'a','I','E','E',O_ADD|O_WORD,"add:q.w",2,{QIM,ABS8},3,	{{0x0d,0xff,0 },{0x00,0x00,ABS8 },{0x08,0xf8,QIM }}},
{221,'a','I','E','E',O_ADD|O_WORD,"add:q.w",2,{QIM,RNIND_D8},3,	{{0xe8,0xf8,RN },{0x00,0x00,DISP8 },{0x08,0xf8,QIM }}},
{221,'a','I','E','E',O_ADD|O_WORD,"add:q.w",2,{QIM,ABS16},4,	{{0x1d,0xff,0 },{0x00,0x00,ABS16 },{0x00,0x00,0 },{0x08,0xf8,QIM }}},
{221,'a','I','E','E',O_ADD|O_WORD,"add:q.w",2,{QIM,RNIND_D16},4,	{{0xf8,0xf8,RN },{0x00,0x00,DISP16 },{0x00,0x00,0 },{0x08,0xf8,QIM }}},
{222,'a','I','E','E',O_ADD|O_BYTE,"add:q.b",2,{QIM,RN},2,	{{0xa0,0xf8,RN },{0x08,0xf8,QIM }}},
{222,'a','I','E','E',O_ADD|O_BYTE,"add:q.b",2,{QIM,RNINC},2,	{{0xc0,0xf8,RN },{0x08,0xf8,QIM }}},
{222,'a','I','E','E',O_ADD|O_BYTE,"add:q.b",2,{QIM,RNDEC},2,	{{0xb0,0xf8,RN },{0x08,0xf8,QIM }}},
{222,'a','I','E','E',O_ADD|O_BYTE,"add:q.b",2,{QIM,RNIND},2,	{{0xd0,0xf8,RN },{0x08,0xf8,QIM }}},
{222,'a','I','E','E',O_ADD|O_BYTE,"add:q.b",2,{QIM,ABS8},3,	{{0x05,0xff,0 },{0x00,0x00,ABS8 },{0x08,0xf8,QIM }}},
{222,'a','I','E','E',O_ADD|O_BYTE,"add:q.b",2,{QIM,RNIND_D8},3,	{{0xe0,0xf8,RN },{0x00,0x00,DISP8 },{0x08,0xf8,QIM }}},
{222,'a','I','E','E',O_ADD|O_BYTE,"add:q.b",2,{QIM,ABS16},4,	{{0x15,0xff,0 },{0x00,0x00,ABS16 },{0x00,0x00,0 },{0x08,0xf8,QIM }}},
{222,'a','I','E','E',O_ADD|O_BYTE,"add:q.b",2,{QIM,RNIND_D16},4,	{{0xf0,0xf8,RN },{0x00,0x00,DISP16 },{0x00,0x00,0 },{0x08,0xf8,QIM }}},
{223,'a','I','E','E',O_ADD|O_UNSZ,"add:q",2,{QIM,RN},2,	{{0xa8,0xf8,RN },{0x08,0xf8,QIM }}},
{223,'a','I','E','E',O_ADD|O_UNSZ,"add:q",2,{QIM,RNDEC},2,	{{0xb8,0xf8,RN },{0x08,0xf8,QIM }}},
{223,'a','I','E','E',O_ADD|O_UNSZ,"add:q",2,{QIM,RNINC},2,	{{0xc8,0xf8,RN },{0x08,0xf8,QIM }}},
{223,'a','I','E','E',O_ADD|O_UNSZ,"add:q",2,{QIM,RNIND},2,	{{0xd8,0xf8,RN },{0x08,0xf8,QIM }}},
{223,'a','I','E','E',O_ADD|O_UNSZ,"add:q",2,{QIM,ABS8},3,	{{0x0d,0xff,0 },{0x00,0x00,ABS8 },{0x08,0xf8,QIM }}},
{223,'a','I','E','E',O_ADD|O_UNSZ,"add:q",2,{QIM,RNIND_D8},3,	{{0xe8,0xf8,RN },{0x00,0x00,DISP8 },{0x08,0xf8,QIM }}},
{223,'a','I','E','E',O_ADD|O_UNSZ,"add:q",2,{QIM,ABS16},4,	{{0x1d,0xff,0 },{0x00,0x00,ABS16 },{0x00,0x00,0 },{0x08,0xf8,QIM }}},
{223,'a','I','E','E',O_ADD|O_UNSZ,"add:q",2,{QIM,RNIND_D16},4,	{{0xf8,0xf8,RN },{0x00,0x00,DISP16 },{0x00,0x00,0 },{0x08,0xf8,QIM }}},
{224,'a','E','D','D',O_ADD|O_WORD,"add:g.w",2,{RN,RD},2,	{{0xa8,0xf8,RN },{0x20,0xf8,RD }}},
{224,'a','E','D','D',O_ADD|O_WORD,"add:g.w",2,{RNDEC,RD},2,	{{0xb8,0xf8,RN },{0x20,0xf8,RD }}},
{224,'a','E','D','D',O_ADD|O_WORD,"add:g.w",2,{RNIND,RD},2,	{{0xd8,0xf8,RN },{0x20,0xf8,RD }}},
{224,'a','E','D','D',O_ADD|O_WORD,"add:g.w",2,{RNINC,RD},2,	{{0xc8,0xf8,RN },{0x20,0xf8,RD }}},
{224,'a','E','D','D',O_ADD|O_WORD,"add:g.w",2,{ABS8,RD},3,	{{0x0d,0xff,0 },{0x00,0x00,ABS8 },{0x20,0xf8,RD }}},
{224,'a','E','D','D',O_ADD|O_WORD,"add:g.w",2,{RNIND_D8,RD},3,	{{0xe8,0xf8,RN },{0x00,0x00,DISP8 },{0x20,0xf8,RD }}},
{224,'a','E','D','D',O_ADD|O_WORD,"add:g.w",2,{ABS16,RD},4,	{{0x1d,0xff,0 },{0x00,0x00,ABS16 },{0x00,0x00,0 },{0x20,0xf8,RD }}},
{224,'a','E','D','D',O_ADD|O_WORD,"add:g.w",2,{IMM16,RD},4,	{{0x0c,0xff,0 },{0x00,0x00,IMM16 },{0x00,0x00,0 },{0x20,0xf8,RD }}},
{224,'a','E','D','D',O_ADD|O_WORD,"add:g.w",2,{RNIND_D16,RD},4,	{{0xf8,0xf8,RN },{0x00,0x00,DISP16 },{0x00,0x00,0 },{0x20,0xf8,RD }}},
{225,'a','E','D','D',O_ADD|O_BYTE,"add:g.b",2,{RN,RD},2,	{{0xa0,0xf8,RN },{0x20,0xf8,RD }}},
{225,'a','E','D','D',O_ADD|O_BYTE,"add:g.b",2,{RNDEC,RD},2,	{{0xb0,0xf8,RN },{0x20,0xf8,RD }}},
{225,'a','E','D','D',O_ADD|O_BYTE,"add:g.b",2,{RNINC,RD},2,	{{0xc0,0xf8,RN },{0x20,0xf8,RD }}},
{225,'a','E','D','D',O_ADD|O_BYTE,"add:g.b",2,{RNIND,RD},2,	{{0xd0,0xf8,RN },{0x20,0xf8,RD }}},
{225,'a','E','D','D',O_ADD|O_BYTE,"add:g.b",2,{ABS8,RD},3,	{{0x05,0xff,0 },{0x00,0x00,ABS8 },{0x20,0xf8,RD }}},
{225,'a','E','D','D',O_ADD|O_BYTE,"add:g.b",2,{RNIND_D8,RD},3,	{{0xe0,0xf8,RN },{0x00,0x00,DISP8 },{0x20,0xf8,RD }}},
{225,'a','E','D','D',O_ADD|O_BYTE,"add:g.b",2,{IMM8,RD},3,	{{0x04,0xff,0 },{0x00,0x00,IMM8 },{0x20,0xf8,RD }}},
{225,'a','E','D','D',O_ADD|O_BYTE,"add:g.b",2,{ABS16,RD},4,	{{0x15,0xff,0 },{0x00,0x00,ABS16 },{0x00,0x00,0 },{0x20,0xf8,RD }}},
{225,'a','E','D','D',O_ADD|O_BYTE,"add:g.b",2,{RNIND_D16,RD},4,	{{0xf0,0xf8,RN },{0x00,0x00,DISP16 },{0x00,0x00,0 },{0x20,0xf8,RD }}},
{226,'a','E','D','D',O_ADD|O_UNSZ,"add:g",2,{RN,RD},2,	{{0xa8,0xf8,RN },{0x20,0xf8,RD }}},
{226,'a','E','D','D',O_ADD|O_UNSZ,"add:g",2,{RNDEC,RD},2,	{{0xb8,0xf8,RN },{0x20,0xf8,RD }}},
{226,'a','E','D','D',O_ADD|O_UNSZ,"add:g",2,{RNINC,RD},2,	{{0xc8,0xf8,RN },{0x20,0xf8,RD }}},
{226,'a','E','D','D',O_ADD|O_UNSZ,"add:g",2,{RNIND,RD},2,	{{0xd8,0xf8,RN },{0x20,0xf8,RD }}},
{226,'a','E','D','D',O_ADD|O_UNSZ,"add:g",2,{ABS8,RD},3,	{{0x0d,0xff,0 },{0x00,0x00,ABS8 },{0x20,0xf8,RD }}},
{226,'a','E','D','D',O_ADD|O_UNSZ,"add:g",2,{RNIND_D8,RD},3,	{{0xe8,0xf8,RN },{0x00,0x00,DISP8 },{0x20,0xf8,RD }}},
{226,'a','E','D','D',O_ADD|O_UNSZ,"add:g",2,{ABS16,RD},4,	{{0x1d,0xff,0 },{0x00,0x00,ABS16 },{0x00,0x00,0 },{0x20,0xf8,RD }}},
{226,'a','E','D','D',O_ADD|O_UNSZ,"add:g",2,{IMM16,RD},4,	{{0x0c,0xff,0 },{0x00,0x00,IMM16 },{0x00,0x00,0 },{0x20,0xf8,RD }}},
{226,'a','E','D','D',O_ADD|O_UNSZ,"add:g",2,{RNIND_D16,RD},4,	{{0xf8,0xf8,RN },{0x00,0x00,DISP16 },{0x00,0x00,0 },{0x20,0xf8,RD }}},
{227,'a','E','D','D',O_ADD|O_WORD,"add.w",2,{RN,RD},2,	{{0xa8,0xf8,RN },{0x20,0xf8,RD }}},
{227,'a','I','E','E',O_ADD|O_WORD,"add.w",2,{QIM,RN},2,	{{0xa8,0xf8,RN },{0x08,0xf8,QIM }}},
{227,'a','I','E','E',O_ADD|O_WORD,"add.w",2,{QIM,RNIND},2,	{{0xd8,0xf8,RN },{0x08,0xf8,QIM }}},
{227,'a','E','D','D',O_ADD|O_WORD,"add.w",2,{RNDEC,RD},2,	{{0xb8,0xf8,RN },{0x20,0xf8,RD }}},
{227,'a','I','E','E',O_ADD|O_WORD,"add.w",2,{QIM,RNDEC},2,	{{0xb8,0xf8,RN },{0x08,0xf8,QIM }}},
{227,'a','I','E','E',O_ADD|O_WORD,"add.w",2,{QIM,RNINC},2,	{{0xc8,0xf8,RN },{0x08,0xf8,QIM }}},
{227,'a','E','D','D',O_ADD|O_WORD,"add.w",2,{RNIND,RD},2,	{{0xd8,0xf8,RN },{0x20,0xf8,RD }}},
{227,'a','E','D','D',O_ADD|O_WORD,"add.w",2,{RNINC,RD},2,	{{0xc8,0xf8,RN },{0x20,0xf8,RD }}},
{227,'a','I','E','E',O_ADD|O_WORD,"add.w",2,{QIM,ABS8},3,	{{0x0d,0xff,0 },{0x00,0x00,ABS8 },{0x08,0xf8,QIM }}},
{227,'a','I','E','E',O_ADD|O_WORD,"add.w",2,{QIM,RNIND_D8},3,	{{0xe8,0xf8,RN },{0x00,0x00,DISP8 },{0x08,0xf8,QIM }}},
{227,'a','E','D','D',O_ADD|O_WORD,"add.w",2,{ABS8,RD},3,	{{0x0d,0xff,0 },{0x00,0x00,ABS8 },{0x20,0xf8,RD }}},
{227,'a','E','D','D',O_ADD|O_WORD,"add.w",2,{RNIND_D8,RD},3,	{{0xe8,0xf8,RN },{0x00,0x00,DISP8 },{0x20,0xf8,RD }}},
{227,'a','E','D','D',O_ADD|O_WORD,"add.w",2,{ABS16,RD},4,	{{0x1d,0xff,0 },{0x00,0x00,ABS16 },{0x00,0x00,0 },{0x20,0xf8,RD }}},
{227,'a','I','E','E',O_ADD|O_WORD,"add.w",2,{QIM,RNIND_D16},4,	{{0xf8,0xf8,RN },{0x00,0x00,DISP16 },{0x00,0x00,0 },{0x08,0xf8,QIM }}},
{227,'a','E','D','D',O_ADD|O_WORD,"add.w",2,{IMM16,RD},4,	{{0x0c,0xff,0 },{0x00,0x00,IMM16 },{0x00,0x00,0 },{0x20,0xf8,RD }}},
{227,'a','I','E','E',O_ADD|O_WORD,"add.w",2,{QIM,ABS16},4,	{{0x1d,0xff,0 },{0x00,0x00,ABS16 },{0x00,0x00,0 },{0x08,0xf8,QIM }}},
{227,'a','E','D','D',O_ADD|O_WORD,"add.w",2,{RNIND_D16,RD},4,	{{0xf8,0xf8,RN },{0x00,0x00,DISP16 },{0x00,0x00,0 },{0x20,0xf8,RD }}},
{228,'a','E','D','D',O_ADD|O_BYTE,"add.b",2,{RN,RD},2,	{{0xa0,0xf8,RN },{0x20,0xf8,RD }}},
{228,'a','I','E','E',O_ADD|O_BYTE,"add.b",2,{QIM,RN},2,	{{0xa0,0xf8,RN },{0x08,0xf8,QIM }}},
{228,'a','I','E','E',O_ADD|O_BYTE,"add.b",2,{QIM,RNINC},2,	{{0xc0,0xf8,RN },{0x08,0xf8,QIM }}},
{228,'a','E','D','D',O_ADD|O_BYTE,"add.b",2,{RNDEC,RD},2,	{{0xb0,0xf8,RN },{0x20,0xf8,RD }}},
{228,'a','I','E','E',O_ADD|O_BYTE,"add.b",2,{QIM,RNIND},2,	{{0xd0,0xf8,RN },{0x08,0xf8,QIM }}},
{228,'a','E','D','D',O_ADD|O_BYTE,"add.b",2,{RNINC,RD},2,	{{0xc0,0xf8,RN },{0x20,0xf8,RD }}},
{228,'a','I','E','E',O_ADD|O_BYTE,"add.b",2,{QIM,RNDEC},2,	{{0xb0,0xf8,RN },{0x08,0xf8,QIM }}},
{228,'a','E','D','D',O_ADD|O_BYTE,"add.b",2,{RNIND,RD},2,	{{0xd0,0xf8,RN },{0x20,0xf8,RD }}},
{228,'a','I','E','E',O_ADD|O_BYTE,"add.b",2,{QIM,RNIND_D8},3,	{{0xe0,0xf8,RN },{0x00,0x00,DISP8 },{0x08,0xf8,QIM }}},
{228,'a','E','D','D',O_ADD|O_BYTE,"add.b",2,{IMM8,RD},3,	{{0x04,0xff,0 },{0x00,0x00,IMM8 },{0x20,0xf8,RD }}},
{228,'a','I','E','E',O_ADD|O_BYTE,"add.b",2,{QIM,ABS8},3,	{{0x05,0xff,0 },{0x00,0x00,ABS8 },{0x08,0xf8,QIM }}},
{228,'a','E','D','D',O_ADD|O_BYTE,"add.b",2,{ABS8,RD},3,	{{0x05,0xff,0 },{0x00,0x00,ABS8 },{0x20,0xf8,RD }}},
{228,'a','E','D','D',O_ADD|O_BYTE,"add.b",2,{RNIND_D8,RD},3,	{{0xe0,0xf8,RN },{0x00,0x00,DISP8 },{0x20,0xf8,RD }}},
{228,'a','I','E','E',O_ADD|O_BYTE,"add.b",2,{QIM,RNIND_D16},4,	{{0xf0,0xf8,RN },{0x00,0x00,DISP16 },{0x00,0x00,0 },{0x08,0xf8,QIM }}},
{228,'a','I','E','E',O_ADD|O_BYTE,"add.b",2,{QIM,ABS16},4,	{{0x15,0xff,0 },{0x00,0x00,ABS16 },{0x00,0x00,0 },{0x08,0xf8,QIM }}},
{228,'a','E','D','D',O_ADD|O_BYTE,"add.b",2,{ABS16,RD},4,	{{0x15,0xff,0 },{0x00,0x00,ABS16 },{0x00,0x00,0 },{0x20,0xf8,RD }}},
{228,'a','E','D','D',O_ADD|O_BYTE,"add.b",2,{RNIND_D16,RD},4,	{{0xf0,0xf8,RN },{0x00,0x00,DISP16 },{0x00,0x00,0 },{0x20,0xf8,RD }}},
{229,'a','E','D','D',O_ADD|O_UNSZ,"add",2,{RN,RD},2,	{{0xa8,0xf8,RN },{0x20,0xf8,RD }}},
{229,'a','I','E','E',O_ADD|O_UNSZ,"add",2,{QIM,RN},2,	{{0xa8,0xf8,RN },{0x08,0xf8,QIM }}},
{229,'a','I','E','E',O_ADD|O_UNSZ,"add",2,{QIM,RNDEC},2,	{{0xb8,0xf8,RN },{0x08,0xf8,QIM }}},
{229,'a','E','D','D',O_ADD|O_UNSZ,"add",2,{RNDEC,RD},2,	{{0xb8,0xf8,RN },{0x20,0xf8,RD }}},
{229,'a','I','E','E',O_ADD|O_UNSZ,"add",2,{QIM,RNIND},2,	{{0xd8,0xf8,RN },{0x08,0xf8,QIM }}},
{229,'a','I','E','E',O_ADD|O_UNSZ,"add",2,{QIM,RNINC},2,	{{0xc8,0xf8,RN },{0x08,0xf8,QIM }}},
{229,'a','E','D','D',O_ADD|O_UNSZ,"add",2,{RNINC,RD},2,	{{0xc8,0xf8,RN },{0x20,0xf8,RD }}},
{229,'a','E','D','D',O_ADD|O_UNSZ,"add",2,{RNIND,RD},2,	{{0xd8,0xf8,RN },{0x20,0xf8,RD }}},
{229,'a','I','E','E',O_ADD|O_UNSZ,"add",2,{QIM,ABS8},3,	{{0x0d,0xff,0 },{0x00,0x00,ABS8 },{0x08,0xf8,QIM }}},
{229,'a','I','E','E',O_ADD|O_UNSZ,"add",2,{QIM,RNIND_D8},3,	{{0xe8,0xf8,RN },{0x00,0x00,DISP8 },{0x08,0xf8,QIM }}},
{229,'a','E','D','D',O_ADD|O_UNSZ,"add",2,{RNIND_D8,RD},3,	{{0xe8,0xf8,RN },{0x00,0x00,DISP8 },{0x20,0xf8,RD }}},
{229,'a','E','D','D',O_ADD|O_UNSZ,"add",2,{ABS8,RD},3,	{{0x0d,0xff,0 },{0x00,0x00,ABS8 },{0x20,0xf8,RD }}},
{229,'a','E','D','D',O_ADD|O_UNSZ,"add",2,{ABS16,RD},4,	{{0x1d,0xff,0 },{0x00,0x00,ABS16 },{0x00,0x00,0 },{0x20,0xf8,RD }}},
{229,'a','I','E','E',O_ADD|O_UNSZ,"add",2,{QIM,RNIND_D16},4,	{{0xf8,0xf8,RN },{0x00,0x00,DISP16 },{0x00,0x00,0 },{0x08,0xf8,QIM }}},
{229,'a','E','D','D',O_ADD|O_UNSZ,"add",2,{IMM16,RD},4,	{{0x0c,0xff,0 },{0x00,0x00,IMM16 },{0x00,0x00,0 },{0x20,0xf8,RD }}},
{229,'a','I','E','E',O_ADD|O_UNSZ,"add",2,{QIM,ABS16},4,	{{0x1d,0xff,0 },{0x00,0x00,ABS16 },{0x00,0x00,0 },{0x08,0xf8,QIM }}},
{229,'a','E','D','D',O_ADD|O_UNSZ,"add",2,{RNIND_D16,RD},4,	{{0xf8,0xf8,RN },{0x00,0x00,DISP16 },{0x00,0x00,0 },{0x20,0xf8,RD }}},
{0,0,0,0,0,0,0,0,{0,0},0,{{0,0,0}}}
}
#endif
;
#endif
#ifdef DISASSEMBLER_TABLE
#ifdef DEFINE_TABLE
={
{4,'m','E','D','D',O_XOR|O_BYTE,"xor.b",2,{RN,RD},2,	{{0xa0,0xf8,RN },{0x60,0xf8,RD }}},
{29,'s','C','!','E',O_STC|O_BYTE,"stc.b",2,{CRB,RN},2,	{{0xa0,0xf8,RN },{0x98,0xf8,CRB }}},
{3,'m','E','D','D',O_XOR|O_WORD,"xor.w",2,{RN,RD},2,	{{0xa8,0xf8,RN },{0x60,0xf8,RD }}},
{4,'m','E','D','D',O_XOR|O_BYTE,"xor.b",2,{RNDEC,RD},2,	{{0xb0,0xf8,RN },{0x60,0xf8,RD }}},
{29,'s','C','!','E',O_STC|O_BYTE,"stc.b",2,{CRB,RNDEC},2,	{{0xb0,0xf8,RN },{0x98,0xf8,CRB }}},
{3,'m','E','D','D',O_XOR|O_WORD,"xor.w",2,{RNDEC,RD},2,	{{0xb8,0xf8,RN },{0x60,0xf8,RD }}},
{4,'m','E','D','D',O_XOR|O_BYTE,"xor.b",2,{RNINC,RD},2,	{{0xc0,0xf8,RN },{0x60,0xf8,RD }}},
{29,'s','C','!','E',O_STC|O_BYTE,"stc.b",2,{CRB,RNINC},2,	{{0xc0,0xf8,RN },{0x98,0xf8,CRB }}},
{3,'m','E','D','D',O_XOR|O_WORD,"xor.w",2,{RNINC,RD},2,	{{0xc8,0xf8,RN },{0x60,0xf8,RD }}},
{4,'m','E','D','D',O_XOR|O_BYTE,"xor.b",2,{RNIND,RD},2,	{{0xd0,0xf8,RN },{0x60,0xf8,RD }}},
{29,'s','C','!','E',O_STC|O_BYTE,"stc.b",2,{CRB,RNIND},2,	{{0xd0,0xf8,RN },{0x98,0xf8,CRB }}},
{3,'m','E','D','D',O_XOR|O_WORD,"xor.w",2,{RNIND,RD},2,	{{0xd8,0xf8,RN },{0x60,0xf8,RD }}},
{25,'a','E','D','D',O_SUB|O_BYTE,"sub.b",2,{RNIND_D8,RD},3,	{{0xe0,0xf8,RN },{0x00,0x00,DISP8 },{0x30,0xf8,RD }}},
{4,'m','E','D','D',O_XOR|O_BYTE,"xor.b",2,{RNIND_D8,RD},3,	{{0xe0,0xf8,RN },{0x00,0x00,DISP8 },{0x60,0xf8,RD }}},
{29,'s','C','!','E',O_STC|O_BYTE,"stc.b",2,{CRB,RNIND_D8},3,	{{0xe0,0xf8,RN },{0x00,0x00,DISP8 },{0x98,0xf8,CRB }}},
{3,'m','E','D','D',O_XOR|O_WORD,"xor.w",2,{RNIND_D8,RD},3,	{{0xe8,0xf8,RN },{0x00,0x00,DISP8 },{0x60,0xf8,RD }}},
{25,'a','E','D','D',O_SUB|O_BYTE,"sub.b",2,{RNIND_D16,RD},4,	{{0xf0,0xf8,RN },{0x00,0x00,DISP16 },{0x00,0x00,0},{0x30,0xf8,RD }}},
{4,'m','E','D','D',O_XOR|O_BYTE,"xor.b",2,{RNIND_D16,RD},4,	{{0xf0,0xf8,RN },{0x00,0x00,DISP16 },{0x00,0x00,0},{0x60,0xf8,RD }}},
{29,'s','C','!','E',O_STC|O_BYTE,"stc.b",2,{CRB,RNIND_D16},4,	{{0xf0,0xf8,RN },{0x00,0x00,DISP16 },{0x00,0x00,0},{0x98,0xf8,CRB }}},
{3,'m','E','D','D',O_XOR|O_WORD,"xor.w",2,{RNIND_D16,RD},4,	{{0xf8,0xf8,RN },{0x00,0x00,DISP16 },{0x00,0x00,0},{0x60,0xf8,RD }}},
{25,'a','E','D','D',O_SUB|O_BYTE,"sub.b",2,{RN,RD},2,	{{0xa0,0xf8,RN },{0x30,0xf8,RD }}},
{10,'a','E','!','!',O_TST|O_BYTE,"tst.b",1,{RN,0},2,	{{0xa0,0xf8,RN },{0x16,0xff,0}}},
{6,'-','X','!','!',O_XCH|O_WORD,"xch.w",2,{RS,RD},2,	{{0xa8,0xf8,RS },{0x90,0xf8,RD }}},
{9,'a','E','!','!',O_TST|O_WORD,"tst.w",1,{RN,0},2,	{{0xa8,0xf8,RN },{0x16,0xff,0}}},
{25,'a','E','D','D',O_SUB|O_BYTE,"sub.b",2,{RNDEC,RD},2,	{{0xb0,0xf8,RN },{0x30,0xf8,RD }}},
{10,'a','E','!','!',O_TST|O_BYTE,"tst.b",1,{RNDEC,0},2,	{{0xb0,0xf8,RN },{0x16,0xff,0}}},
{24,'a','E','D','D',O_SUB|O_WORD,"sub.w",2,{RNDEC,RD},2,	{{0xb8,0xf8,RN },{0x30,0xf8,RD }}},
{9,'a','E','!','!',O_TST|O_WORD,"tst.w",1,{RNDEC,0},2,	{{0xb8,0xf8,RN },{0x16,0xff,0}}},
{25,'a','E','D','D',O_SUB|O_BYTE,"sub.b",2,{RNINC,RD},2,	{{0xc0,0xf8,RN },{0x30,0xf8,RD }}},
{10,'a','E','!','!',O_TST|O_BYTE,"tst.b",1,{RNINC,0},2,	{{0xc0,0xf8,RN },{0x16,0xff,0}}},
{9,'a','E','!','!',O_TST|O_WORD,"tst.w",1,{RNINC,0},2,	{{0xc8,0xf8,RN },{0x16,0xff,0}}},
{25,'a','E','D','D',O_SUB|O_BYTE,"sub.b",2,{RNIND,RD},2,	{{0xd0,0xf8,RN },{0x30,0xf8,RD }}},
{10,'a','E','!','!',O_TST|O_BYTE,"tst.b",1,{RNIND,0},2,	{{0xd0,0xf8,RN },{0x16,0xff,0}}},
{24,'a','E','D','D',O_SUB|O_WORD,"sub.w",2,{RNIND,RD},2,	{{0xd8,0xf8,RN },{0x30,0xf8,RD }}},
{9,'a','E','!','!',O_TST|O_WORD,"tst.w",1,{RNIND,0},2,	{{0xd8,0xf8,RN },{0x16,0xff,0}}},
{10,'a','E','!','!',O_TST|O_BYTE,"tst.b",1,{RNIND_D8,0},3,	{{0xe0,0xf8,RN },{0x00,0x00,DISP8 },{0x16,0xff,0}}},
{24,'a','E','D','D',O_SUB|O_WORD,"sub.w",2,{RNIND_D8,RD},3,	{{0xe8,0xf8,RN },{0x00,0x00,DISP8 },{0x30,0xf8,RD }}},
{9,'a','E','!','!',O_TST|O_WORD,"tst.w",1,{RNIND_D8,0},3,	{{0xe8,0xf8,RN },{0x00,0x00,DISP8 },{0x16,0xff,0}}},
{10,'a','E','!','!',O_TST|O_BYTE,"tst.b",1,{RNIND_D16,0},4,	{{0xf0,0xf8,RN },{0x00,0x00,DISP16 },{0x00,0x00,0},{0x16,0xff,0}}},
{24,'a','E','D','D',O_SUB|O_WORD,"sub.w",2,{RNIND_D16,RD},4,	{{0xf8,0xf8,RN },{0x00,0x00,DISP16 },{0x00,0x00,0},{0x30,0xf8,RD }}},
{16,'m','D','!','D',O_SWAP|O_WORD,"swap.b",1,{RD,0},2,	{{0xa0,0xf8,RD },{0x10,0xff,0}}},
{14,'s','E','!','E',O_TAS|O_BYTE,"tas.b",1,{RN,0},2,	{{0xa0,0xf8,RN },{0x17,0xff,0}}},
{24,'a','E','D','D',O_SUB|O_WORD,"sub.w",2,{RN,RD},2,	{{0xa8,0xf8,RN },{0x30,0xf8,RD }}},
{18,'a','E','D','D',O_SUBX|O_WORD,"subx.w",2,{RN,RD},2,	{{0xa8,0xf8,RN },{0xb0,0xf8,RD }}},
{14,'s','E','!','E',O_TAS|O_BYTE,"tas.b",1,{RNDEC,0},2,	{{0xb0,0xf8,RN },{0x17,0xff,0}}},
{18,'a','E','D','D',O_SUBX|O_WORD,"subx.w",2,{RNDEC,RD},2,	{{0xb8,0xf8,RN },{0xb0,0xf8,RD }}},
{14,'s','E','!','E',O_TAS|O_BYTE,"tas.b",1,{RNINC,0},2,	{{0xc0,0xf8,RN },{0x17,0xff,0}}},
{24,'a','E','D','D',O_SUB|O_WORD,"sub.w",2,{RNINC,RD},2,	{{0xc8,0xf8,RN },{0x30,0xf8,RD }}},
{22,'-','E','D','D',O_SUBS|O_BYTE,"subs.b",2,{RNIND,RD},2,	{{0xd0,0xf8,RN },{0x38,0xf8,RD }}},
{14,'s','E','!','E',O_TAS|O_BYTE,"tas.b",1,{RNIND,0},2,	{{0xd0,0xf8,RN },{0x17,0xff,0}}},
{18,'a','E','D','D',O_SUBX|O_WORD,"subx.w",2,{RNIND,RD},2,	{{0xd8,0xf8,RN },{0xb0,0xf8,RD }}},
{14,'s','E','!','E',O_TAS|O_BYTE,"tas.b",1,{RNIND_D8,0},3,	{{0xe0,0xf8,RN },{0x00,0x00,DISP8 },{0x17,0xff,0}}},
{22,'-','E','D','D',O_SUBS|O_BYTE,"subs.b",2,{RNIND_D8,RD},3,	{{0xe0,0xf8,RN },{0x00,0x00,DISP8 },{0x38,0xf8,RD }}},
{22,'-','E','D','D',O_SUBS|O_BYTE,"subs.b",2,{RNIND_D16,RD},4,	{{0xf0,0xf8,RN },{0x00,0x00,DISP16 },{0x00,0x00,0},{0x38,0xf8,RD }}},
{14,'s','E','!','E',O_TAS|O_BYTE,"tas.b",1,{RNIND_D16,0},4,	{{0xf0,0xf8,RN },{0x00,0x00,DISP16 },{0x00,0x00,0},{0x17,0xff,0}}},
{22,'-','E','D','D',O_SUBS|O_BYTE,"subs.b",2,{RN,RD},2,	{{0xa0,0xf8,RN },{0x38,0xf8,RD }}},
{19,'a','E','D','D',O_SUBX|O_BYTE,"subx.b",2,{RN,RD},2,	{{0xa0,0xf8,RN },{0xb0,0xf8,RD }}},
{21,'-','E','D','D',O_SUBS|O_WORD,"subs.w",2,{RN,RD},2,	{{0xa8,0xf8,RN },{0x38,0xf8,RD }}},
{22,'-','E','D','D',O_SUBS|O_BYTE,"subs.b",2,{RNDEC,RD},2,	{{0xb0,0xf8,RN },{0x38,0xf8,RD }}},
{19,'a','E','D','D',O_SUBX|O_BYTE,"subx.b",2,{RNDEC,RD},2,	{{0xb0,0xf8,RN },{0xb0,0xf8,RD }}},
{21,'-','E','D','D',O_SUBS|O_WORD,"subs.w",2,{RNDEC,RD},2,	{{0xb8,0xf8,RN },{0x38,0xf8,RD }}},
{22,'-','E','D','D',O_SUBS|O_BYTE,"subs.b",2,{RNINC,RD},2,	{{0xc0,0xf8,RN },{0x38,0xf8,RD }}},
{19,'a','E','D','D',O_SUBX|O_BYTE,"subx.b",2,{RNINC,RD},2,	{{0xc0,0xf8,RN },{0xb0,0xf8,RD }}},
{21,'-','E','D','D',O_SUBS|O_WORD,"subs.w",2,{RNINC,RD},2,	{{0xc8,0xf8,RN },{0x38,0xf8,RD }}},
{18,'a','E','D','D',O_SUBX|O_WORD,"subx.w",2,{RNINC,RD},2,	{{0xc8,0xf8,RN },{0xb0,0xf8,RD }}},
{19,'a','E','D','D',O_SUBX|O_BYTE,"subx.b",2,{RNIND,RD},2,	{{0xd0,0xf8,RN },{0xb0,0xf8,RD }}},
{19,'a','E','D','D',O_SUBX|O_BYTE,"subx.b",2,{RNIND_D8,RD},3,	{{0xe0,0xf8,RN },{0x00,0x00,DISP8 },{0xb0,0xf8,RD }}},
{21,'-','E','D','D',O_SUBS|O_WORD,"subs.w",2,{RNIND_D8,RD},3,	{{0xe8,0xf8,RN },{0x00,0x00,DISP8 },{0x38,0xf8,RD }}},
{19,'a','E','D','D',O_SUBX|O_BYTE,"subx.b",2,{RNIND_D16,RD},4,	{{0xf0,0xf8,RN },{0x00,0x00,DISP16 },{0x00,0x00,0},{0xb0,0xf8,RD }}},
{21,'-','E','D','D',O_SUBS|O_WORD,"subs.w",2,{RNIND_D16,RD},4,	{{0xf8,0xf8,RN },{0x00,0x00,DISP16 },{0x00,0x00,0},{0x38,0xf8,RD }}},
{21,'-','E','D','D',O_SUBS|O_WORD,"subs.w",2,{RNIND,RD},2,	{{0xd8,0xf8,RN },{0x38,0xf8,RD }}},
{18,'a','E','D','D',O_SUBX|O_WORD,"subx.w",2,{RNIND_D8,RD},3,	{{0xe8,0xf8,RN },{0x00,0x00,DISP8 },{0xb0,0xf8,RD }}},
{18,'a','E','D','D',O_SUBX|O_WORD,"subx.w",2,{RNIND_D16,RD},4,	{{0xf8,0xf8,RN },{0x00,0x00,DISP16 },{0x00,0x00,0},{0xb0,0xf8,RD }}},
{9,'a','E','!','!',O_TST|O_WORD,"tst.w",1,{RNIND_D16,0},4,	{{0xf8,0xf8,RN },{0x00,0x00,DISP16 },{0x00,0x00,0},{0x16,0xff,0}}},
{25,'a','E','D','D',O_SUB|O_BYTE,"sub.b",2,{IMM8,RD},3,	{{0x04,0xff,0},{0x00,0x00,IMM8 },{0x30,0xf8,RD }}},
{22,'-','E','D','D',O_SUBS|O_BYTE,"subs.b",2,{IMM8,RD},3,	{{0x04,0xff,0},{0x00,0x00,IMM8 },{0x38,0xf8,RD }}},
{4,'m','E','D','D',O_XOR|O_BYTE,"xor.b",2,{IMM8,RD},3,	{{0x04,0xff,0},{0x00,0x00,IMM8 },{0x60,0xf8,RD }}},
{2,'s','E','C','C',O_XORC|O_BYTE,"xorc.b",2,{IMM8,CRB},3,	{{0x04,0xff,0},{0x00,0x00,IMM8 },{0x68,0xf8,CRB }}},
{19,'a','E','D','D',O_SUBX|O_BYTE,"subx.b",2,{IMM8,RD},3,	{{0x04,0xff,0},{0x00,0x00,IMM8 },{0xb0,0xf8,RD }}},
{10,'a','E','!','!',O_TST|O_BYTE,"tst.b",1,{IMM8,0},3,	{{0x04,0xff,0},{0x00,0x00,IMM8 },{0x16,0xff,0}}},
{14,'s','E','!','E',O_TAS|O_BYTE,"tas.b",1,{IMM8,0},3,	{{0x04,0xff,0},{0x00,0x00,IMM8 },{0x17,0xff,0}}},
{25,'a','E','D','D',O_SUB|O_BYTE,"sub.b",2,{ABS8,RD},3,	{{0x05,0xff,0},{0x00,0x00,ABS8 },{0x30,0xf8,RD }}},
{22,'-','E','D','D',O_SUBS|O_BYTE,"subs.b",2,{ABS8,RD},3,	{{0x05,0xff,0},{0x00,0x00,ABS8 },{0x38,0xf8,RD }}},
{4,'m','E','D','D',O_XOR|O_BYTE,"xor.b",2,{ABS8,RD},3,	{{0x05,0xff,0},{0x00,0x00,ABS8 },{0x60,0xf8,RD }}},
{29,'s','C','!','E',O_STC|O_BYTE,"stc.b",2,{CRB,ABS8},3,	{{0x05,0xff,0},{0x00,0x00,ABS8 },{0x98,0xf8,CRB }}},
{19,'a','E','D','D',O_SUBX|O_BYTE,"subx.b",2,{ABS8,RD},3,	{{0x05,0xff,0},{0x00,0x00,ABS8 },{0xb0,0xf8,RD }}},
{10,'a','E','!','!',O_TST|O_BYTE,"tst.b",1,{ABS8,0},3,	{{0x05,0xff,0},{0x00,0x00,ABS8 },{0x16,0xff,0}}},
{14,'s','E','!','E',O_TAS|O_BYTE,"tas.b",1,{ABS8,0},3,	{{0x05,0xff,0},{0x00,0x00,ABS8 },{0x17,0xff,0}}},
{12,'-','I','!','!',O_TRAPA|O_UNSZ,"trapa",1,{IMM4,0},2,	{{0x08,0xff,0},{0x10,0xf0,IMM4 }}},
{13,'-','B','!','!',O_TRAP_VS|O_UNSZ,"trap/vs",0,{0,0},1,	{{0x09,0xff,0}}},
{24,'a','E','D','D',O_SUB|O_WORD,"sub.w",2,{IMM16,RD},4,	{{0x0c,0xff,0},{0x00,0x00,IMM16 },{0x00,0x00,0},{0x30,0xf8,RD }}},
{21,'-','E','D','D',O_SUBS|O_WORD,"subs.w",2,{IMM16,RD},4,	{{0x0c,0xff,0},{0x00,0x00,IMM16 },{0x00,0x00,0},{0x38,0xf8,RD }}},
{3,'m','E','D','D',O_XOR|O_WORD,"xor.w",2,{IMM16,RD},4,	{{0x0c,0xff,0},{0x00,0x00,IMM16 },{0x00,0x00,0},{0x60,0xf8,RD }}},
{1,'s','E','C','C',O_XORC|O_WORD,"xorc.w",2,{IMM16,CRW},4,	{{0x0c,0xff,0},{0x00,0x00,IMM16 },{0x00,0x00,0},{0x68,0xf8,CRW }}},
{18,'a','E','D','D',O_SUBX|O_WORD,"subx.w",2,{IMM16,RD},4,	{{0x0c,0xff,0},{0x00,0x00,IMM16 },{0x00,0x00,0},{0xb0,0xf8,RD }}},
{9,'a','E','!','!',O_TST|O_WORD,"tst.w",1,{IMM16,0},4,	{{0x0c,0xff,0},{0x00,0x00,IMM16 },{0x00,0x00,0},{0x16,0xff,0}}},
{24,'a','E','D','D',O_SUB|O_WORD,"sub.w",2,{ABS8,RD},3,	{{0x0d,0xff,0},{0x00,0x00,ABS8 },{0x30,0xf8,RD }}},
{21,'-','E','D','D',O_SUBS|O_WORD,"subs.w",2,{ABS8,RD},3,	{{0x0d,0xff,0},{0x00,0x00,ABS8 },{0x38,0xf8,RD }}},
{3,'m','E','D','D',O_XOR|O_WORD,"xor.w",2,{ABS8,RD},3,	{{0x0d,0xff,0},{0x00,0x00,ABS8 },{0x60,0xf8,RD }}},
{18,'a','E','D','D',O_SUBX|O_WORD,"subx.w",2,{ABS8,RD},3,	{{0x0d,0xff,0},{0x00,0x00,ABS8 },{0xb0,0xf8,RD }}},
{9,'a','E','!','!',O_TST|O_WORD,"tst.w",1,{ABS8,0},3,	{{0x0d,0xff,0},{0x00,0x00,ABS8 },{0x16,0xff,0}}},
{8,'-','B','!','!',O_UNLK|O_UNSZ,"unlk",1,{FP,0},1,	{{0x0f,0xff,0}}},
{25,'a','E','D','D',O_SUB|O_BYTE,"sub.b",2,{ABS16,RD},4,	{{0x15,0xff,0},{0x00,0x00,ABS16 },{0x00,0x00,0},{0x30,0xf8,RD }}},
{22,'-','E','D','D',O_SUBS|O_BYTE,"subs.b",2,{ABS16,RD},4,	{{0x15,0xff,0},{0x00,0x00,ABS16 },{0x00,0x00,0},{0x38,0xf8,RD }}},
{4,'m','E','D','D',O_XOR|O_BYTE,"xor.b",2,{ABS16,RD},4,	{{0x15,0xff,0},{0x00,0x00,ABS16 },{0x00,0x00,0},{0x60,0xf8,RD }}},
{29,'s','C','!','E',O_STC|O_BYTE,"stc.b",2,{CRB,ABS16},4,	{{0x15,0xff,0},{0x00,0x00,ABS16 },{0x00,0x00,0},{0x98,0xf8,CRB }}},
{27,'-','I','!','E',O_STM|O_UNSZ,"stm",2,{RLIST,SPDEC},2,	{{0x12,0xff,0},{0x00,0x00,RLIST }}},
{19,'a','E','D','D',O_SUBX|O_BYTE,"subx.b",2,{ABS16,RD},4,	{{0x15,0xff,0},{0x00,0x00,ABS16 },{0x00,0x00,0},{0xb0,0xf8,RD }}},
{10,'a','E','!','!',O_TST|O_BYTE,"tst.b",1,{ABS16,0},4,	{{0x15,0xff,0},{0x00,0x00,ABS16 },{0x00,0x00,0},{0x16,0xff,0}}},
{14,'s','E','!','E',O_TAS|O_BYTE,"tas.b",1,{ABS16,0},4,	{{0x15,0xff,0},{0x00,0x00,ABS16 },{0x00,0x00,0},{0x17,0xff,0}}},
{24,'a','E','D','D',O_SUB|O_WORD,"sub.w",2,{ABS16,RD},4,	{{0x1d,0xff,0},{0x00,0x00,ABS16 },{0x00,0x00,0},{0x30,0xf8,RD }}},
{21,'-','E','D','D',O_SUBS|O_WORD,"subs.w",2,{ABS16,RD},4,	{{0x1d,0xff,0},{0x00,0x00,ABS16 },{0x00,0x00,0},{0x38,0xf8,RD }}},
{3,'m','E','D','D',O_XOR|O_WORD,"xor.w",2,{ABS16,RD},4,	{{0x1d,0xff,0},{0x00,0x00,ABS16 },{0x00,0x00,0},{0x60,0xf8,RD }}},
{18,'a','E','D','D',O_SUBX|O_WORD,"subx.w",2,{ABS16,RD},4,	{{0x1d,0xff,0},{0x00,0x00,ABS16 },{0x00,0x00,0},{0xb0,0xf8,RD }}},
{9,'a','E','!','!',O_TST|O_WORD,"tst.w",1,{ABS16,0},4,	{{0x1d,0xff,0},{0x00,0x00,ABS16 },{0x00,0x00,0},{0x16,0xff,0}}},
{38,'h','E','!','E',O_SHAR|O_WORD,"shar.w",1,{RNIND,0},2,	{{0xd8,0xf8,RN },{0x19,0xff,0}}},
{39,'h','E','!','E',O_SHAR|O_BYTE,"shar.b",1,{RNIND,0},2,	{{0xd0,0xf8,RN },{0x19,0xff,0}}},
{32,'h','E','!','E',O_SHLR|O_WORD,"shlr.w",1,{RN,0},2,	{{0xa8,0xf8,RN },{0x1b,0xff,0}}},
{39,'h','E','!','E',O_SHAR|O_BYTE,"shar.b",1,{RNINC,0},2,	{{0xc0,0xf8,RN },{0x19,0xff,0}}},
{39,'h','E','!','E',O_SHAR|O_BYTE,"shar.b",1,{RN,0},2,	{{0xa0,0xf8,RN },{0x19,0xff,0}}},
{39,'h','E','!','E',O_SHAR|O_BYTE,"shar.b",1,{RNDEC,0},2,	{{0xb0,0xf8,RN },{0x19,0xff,0}}},
{32,'h','E','!','E',O_SHLR|O_WORD,"shlr.w",1,{RNDEC,0},2,	{{0xb8,0xf8,RN },{0x1b,0xff,0}}},
{38,'h','E','!','E',O_SHAR|O_WORD,"shar.w",1,{RNINC,0},2,	{{0xc8,0xf8,RN },{0x19,0xff,0}}},
{32,'h','E','!','E',O_SHLR|O_WORD,"shlr.w",1,{RNINC,0},2,	{{0xc8,0xf8,RN },{0x1b,0xff,0}}},
{36,'h','E','!','E',O_SHLL|O_BYTE,"shll.b",1,{RN,0},2,	{{0xa0,0xf8,RN },{0x1a,0xff,0}}},
{33,'h','E','!','E',O_SHLR|O_BYTE,"shlr.b",1,{RN,0},2,	{{0xa0,0xf8,RN },{0x1b,0xff,0}}},
{38,'h','E','!','E',O_SHAR|O_WORD,"shar.w",1,{RN,0},2,	{{0xa8,0xf8,RN },{0x19,0xff,0}}},
{36,'h','E','!','E',O_SHLL|O_BYTE,"shll.b",1,{RNDEC,0},2,	{{0xb0,0xf8,RN },{0x1a,0xff,0}}},
{33,'h','E','!','E',O_SHLR|O_BYTE,"shlr.b",1,{RNDEC,0},2,	{{0xb0,0xf8,RN },{0x1b,0xff,0}}},
{38,'h','E','!','E',O_SHAR|O_WORD,"shar.w",1,{RNDEC,0},2,	{{0xb8,0xf8,RN },{0x19,0xff,0}}},
{36,'h','E','!','E',O_SHLL|O_BYTE,"shll.b",1,{RNINC,0},2,	{{0xc0,0xf8,RN },{0x1a,0xff,0}}},
{33,'h','E','!','E',O_SHLR|O_BYTE,"shlr.b",1,{RNINC,0},2,	{{0xc0,0xf8,RN },{0x1b,0xff,0}}},
{36,'h','E','!','E',O_SHLL|O_BYTE,"shll.b",1,{RNIND,0},2,	{{0xd0,0xf8,RN },{0x1a,0xff,0}}},
{33,'h','E','!','E',O_SHLR|O_BYTE,"shlr.b",1,{RNIND,0},2,	{{0xd0,0xf8,RN },{0x1b,0xff,0}}},
{35,'h','E','!','E',O_SHLL|O_WORD,"shll.w",1,{RN,0},2,	{{0xa8,0xf8,RN },{0x1a,0xff,0}}},
{35,'h','E','!','E',O_SHLL|O_WORD,"shll.w",1,{RNDEC,0},2,	{{0xb8,0xf8,RN },{0x1a,0xff,0}}},
{35,'h','E','!','E',O_SHLL|O_WORD,"shll.w",1,{RNINC,0},2,	{{0xc8,0xf8,RN },{0x1a,0xff,0}}},
{35,'h','E','!','E',O_SHLL|O_WORD,"shll.w",1,{RNIND,0},2,	{{0xd8,0xf8,RN },{0x1a,0xff,0}}},
{36,'h','E','!','E',O_SHLL|O_BYTE,"shll.b",1,{RNIND_D8,0},3,	{{0xe0,0xf8,RN },{0x00,0x00,DISP8 },{0x1a,0xff,0}}},
{33,'h','E','!','E',O_SHLR|O_BYTE,"shlr.b",1,{RNIND_D8,0},3,	{{0xe0,0xf8,RN },{0x00,0x00,DISP8 },{0x1b,0xff,0}}},
{35,'h','E','!','E',O_SHLL|O_WORD,"shll.w",1,{RNIND_D8,0},3,	{{0xe8,0xf8,RN },{0x00,0x00,DISP8 },{0x1a,0xff,0}}},
{36,'h','E','!','E',O_SHLL|O_BYTE,"shll.b",1,{RNIND_D16,0},4,	{{0xf0,0xf8,RN },{0x00,0x00,DISP16 },{0x00,0x00,0},{0x1a,0xff,0}}},
{33,'h','E','!','E',O_SHLR|O_BYTE,"shlr.b",1,{RNIND_D16,0},4,	{{0xf0,0xf8,RN },{0x00,0x00,DISP16 },{0x00,0x00,0},{0x1b,0xff,0}}},
{35,'h','E','!','E',O_SHLL|O_WORD,"shll.w",1,{RNIND_D16,0},4,	{{0xf8,0xf8,RN },{0x00,0x00,DISP16 },{0x00,0x00,0},{0x1a,0xff,0}}},
{36,'h','E','!','E',O_SHLL|O_BYTE,"shll.b",1,{IMM8,0},3,	{{0x04,0xff,0},{0x00,0x00,IMM8 },{0x1a,0xff,0}}},
{33,'h','E','!','E',O_SHLR|O_BYTE,"shlr.b",1,{IMM8,0},3,	{{0x04,0xff,0},{0x00,0x00,IMM8 },{0x1b,0xff,0}}},
{36,'h','E','!','E',O_SHLL|O_BYTE,"shll.b",1,{ABS8,0},3,	{{0x05,0xff,0},{0x00,0x00,ABS8 },{0x1a,0xff,0}}},
{35,'h','E','!','E',O_SHLL|O_WORD,"shll.w",1,{IMM16,0},4,	{{0x0c,0xff,0},{0x00,0x00,IMM16 },{0x00,0x00,0},{0x1a,0xff,0}}},
{35,'h','E','!','E',O_SHLL|O_WORD,"shll.w",1,{ABS8,0},3,	{{0x0d,0xff,0},{0x00,0x00,ABS8 },{0x1a,0xff,0}}},
{36,'h','E','!','E',O_SHLL|O_BYTE,"shll.b",1,{ABS16,0},4,	{{0x15,0xff,0},{0x00,0x00,ABS16 },{0x00,0x00,0},{0x1a,0xff,0}}},
{33,'h','E','!','E',O_SHLR|O_BYTE,"shlr.b",1,{ABS16,0},4,	{{0x15,0xff,0},{0x00,0x00,ABS16 },{0x00,0x00,0},{0x1b,0xff,0}}},
{35,'h','E','!','E',O_SHLL|O_WORD,"shll.w",1,{ABS16,0},4,	{{0x1d,0xff,0},{0x00,0x00,ABS16 },{0x00,0x00,0},{0x1a,0xff,0}}},
{32,'h','E','!','E',O_SHLR|O_WORD,"shlr.w",1,{RNIND,0},2,	{{0xd8,0xf8,RN },{0x1b,0xff,0}}},
{39,'h','E','!','E',O_SHAR|O_BYTE,"shar.b",1,{RNIND_D8,0},3,	{{0xe0,0xf8,RN },{0x00,0x00,DISP8 },{0x19,0xff,0}}},
{38,'h','E','!','E',O_SHAR|O_WORD,"shar.w",1,{RNIND_D8,0},3,	{{0xe8,0xf8,RN },{0x00,0x00,DISP8 },{0x19,0xff,0}}},
{32,'h','E','!','E',O_SHLR|O_WORD,"shlr.w",1,{RNIND_D8,0},3,	{{0xe8,0xf8,RN },{0x00,0x00,DISP8 },{0x1b,0xff,0}}},
{39,'h','E','!','E',O_SHAR|O_BYTE,"shar.b",1,{RNIND_D16,0},4,	{{0xf0,0xf8,RN },{0x00,0x00,DISP16 },{0x00,0x00,0},{0x19,0xff,0}}},
{38,'h','E','!','E',O_SHAR|O_WORD,"shar.w",1,{RNIND_D16,0},4,	{{0xf8,0xf8,RN },{0x00,0x00,DISP16 },{0x00,0x00,0},{0x19,0xff,0}}},
{32,'h','E','!','E',O_SHLR|O_WORD,"shlr.w",1,{RNIND_D16,0},4,	{{0xf8,0xf8,RN },{0x00,0x00,DISP16 },{0x00,0x00,0},{0x1b,0xff,0}}},
{39,'h','E','!','E',O_SHAR|O_BYTE,"shar.b",1,{IMM8,0},3,	{{0x04,0xff,0},{0x00,0x00,IMM8 },{0x19,0xff,0}}},
{39,'h','E','!','E',O_SHAR|O_BYTE,"shar.b",1,{ABS8,0},3,	{{0x05,0xff,0},{0x00,0x00,ABS8 },{0x19,0xff,0}}},
{33,'h','E','!','E',O_SHLR|O_BYTE,"shlr.b",1,{ABS8,0},3,	{{0x05,0xff,0},{0x00,0x00,ABS8 },{0x1b,0xff,0}}},
{38,'h','E','!','E',O_SHAR|O_WORD,"shar.w",1,{IMM16,0},4,	{{0x0c,0xff,0},{0x00,0x00,IMM16 },{0x00,0x00,0},{0x19,0xff,0}}},
{32,'h','E','!','E',O_SHLR|O_WORD,"shlr.w",1,{IMM16,0},4,	{{0x0c,0xff,0},{0x00,0x00,IMM16 },{0x00,0x00,0},{0x1b,0xff,0}}},
{38,'h','E','!','E',O_SHAR|O_WORD,"shar.w",1,{ABS8,0},3,	{{0x0d,0xff,0},{0x00,0x00,ABS8 },{0x19,0xff,0}}},
{32,'h','E','!','E',O_SHLR|O_WORD,"shlr.w",1,{ABS8,0},3,	{{0x0d,0xff,0},{0x00,0x00,ABS8 },{0x1b,0xff,0}}},
{39,'h','E','!','E',O_SHAR|O_BYTE,"shar.b",1,{ABS16,0},4,	{{0x15,0xff,0},{0x00,0x00,ABS16 },{0x00,0x00,0},{0x19,0xff,0}}},
{31,'-','!','!','!',O_SLEEP|O_UNSZ,"sleep",0,{0,0},1,	{{0x1a,0xff,0}}},
{38,'h','E','!','E',O_SHAR|O_WORD,"shar.w",1,{ABS16,0},4,	{{0x1d,0xff,0},{0x00,0x00,ABS16 },{0x00,0x00,0},{0x19,0xff,0}}},
{32,'h','E','!','E',O_SHLR|O_WORD,"shlr.w",1,{ABS16,0},4,	{{0x1d,0xff,0},{0x00,0x00,ABS16 },{0x00,0x00,0},{0x1b,0xff,0}}},
{42,'h','E','!','E',O_SHAL|O_BYTE,"shal.b",1,{RN,0},2,	{{0xa0,0xf8,RN },{0x18,0xff,0}}},
{41,'h','E','!','E',O_SHAL|O_WORD,"shal.w",1,{RN,0},2,	{{0xa8,0xf8,RN },{0x18,0xff,0}}},
{42,'h','E','!','E',O_SHAL|O_BYTE,"shal.b",1,{RNDEC,0},2,	{{0xb0,0xf8,RN },{0x18,0xff,0}}},
{41,'h','E','!','E',O_SHAL|O_WORD,"shal.w",1,{RNDEC,0},2,	{{0xb8,0xf8,RN },{0x18,0xff,0}}},
{52,'h','E','!','E',O_ROTXL|O_WORD,"rotxl.w",1,{RNDEC,0},2,	{{0xb8,0xf8,RN },{0x1e,0xff,0}}},
{42,'h','E','!','E',O_SHAL|O_BYTE,"shal.b",1,{RNINC,0},2,	{{0xc0,0xf8,RN },{0x18,0xff,0}}},
{41,'h','E','!','E',O_SHAL|O_WORD,"shal.w",1,{RNINC,0},2,	{{0xc8,0xf8,RN },{0x18,0xff,0}}},
{42,'h','E','!','E',O_SHAL|O_BYTE,"shal.b",1,{RNIND,0},2,	{{0xd0,0xf8,RN },{0x18,0xff,0}}},
{41,'h','E','!','E',O_SHAL|O_WORD,"shal.w",1,{RNIND,0},2,	{{0xd8,0xf8,RN },{0x18,0xff,0}}},
{42,'h','E','!','E',O_SHAL|O_BYTE,"shal.b",1,{RNIND_D8,0},3,	{{0xe0,0xf8,RN },{0x00,0x00,DISP8 },{0x18,0xff,0}}},
{53,'h','E','!','E',O_ROTXL|O_BYTE,"rotxl.b",1,{RNIND_D8,0},3,	{{0xe0,0xf8,RN },{0x00,0x00,DISP8 },{0x1e,0xff,0}}},
{41,'h','E','!','E',O_SHAL|O_WORD,"shal.w",1,{RNIND_D8,0},3,	{{0xe8,0xf8,RN },{0x00,0x00,DISP8 },{0x18,0xff,0}}},
{52,'h','E','!','E',O_ROTXL|O_WORD,"rotxl.w",1,{RNIND_D8,0},3,	{{0xe8,0xf8,RN },{0x00,0x00,DISP8 },{0x1e,0xff,0}}},
{42,'h','E','!','E',O_SHAL|O_BYTE,"shal.b",1,{RNIND_D16,0},4,	{{0xf0,0xf8,RN },{0x00,0x00,DISP16 },{0x00,0x00,0},{0x18,0xff,0}}},
{53,'h','E','!','E',O_ROTXL|O_BYTE,"rotxl.b",1,{RNIND_D16,0},4,	{{0xf0,0xf8,RN },{0x00,0x00,DISP16 },{0x00,0x00,0},{0x1e,0xff,0}}},
{41,'h','E','!','E',O_SHAL|O_WORD,"shal.w",1,{RNIND_D16,0},4,	{{0xf8,0xf8,RN },{0x00,0x00,DISP16 },{0x00,0x00,0},{0x18,0xff,0}}},
{52,'h','E','!','E',O_ROTXL|O_WORD,"rotxl.w",1,{RNIND_D16,0},4,	{{0xf8,0xf8,RN },{0x00,0x00,DISP16 },{0x00,0x00,0},{0x1e,0xff,0}}},
{42,'h','E','!','E',O_SHAL|O_BYTE,"shal.b",1,{IMM8,0},3,	{{0x04,0xff,0},{0x00,0x00,IMM8 },{0x18,0xff,0}}},
{50,'h','E','!','E',O_ROTXR|O_BYTE,"rotxr.b",1,{RNDEC,0},2,	{{0xb0,0xf8,RN },{0x1f,0xff,0}}},
{53,'h','E','!','E',O_ROTXL|O_BYTE,"rotxl.b",1,{RN,0},2,	{{0xa0,0xf8,RN },{0x1e,0xff,0}}},
{50,'h','E','!','E',O_ROTXR|O_BYTE,"rotxr.b",1,{RN,0},2,	{{0xa0,0xf8,RN },{0x1f,0xff,0}}},
{52,'h','E','!','E',O_ROTXL|O_WORD,"rotxl.w",1,{RN,0},2,	{{0xa8,0xf8,RN },{0x1e,0xff,0}}},
{49,'h','E','!','E',O_ROTXR|O_WORD,"rotxr.w",1,{RN,0},2,	{{0xa8,0xf8,RN },{0x1f,0xff,0}}},
{53,'h','E','!','E',O_ROTXL|O_BYTE,"rotxl.b",1,{RNDEC,0},2,	{{0xb0,0xf8,RN },{0x1e,0xff,0}}},
{49,'h','E','!','E',O_ROTXR|O_WORD,"rotxr.w",1,{RNDEC,0},2,	{{0xb8,0xf8,RN },{0x1f,0xff,0}}},
{53,'h','E','!','E',O_ROTXL|O_BYTE,"rotxl.b",1,{RNINC,0},2,	{{0xc0,0xf8,RN },{0x1e,0xff,0}}},
{50,'h','E','!','E',O_ROTXR|O_BYTE,"rotxr.b",1,{RNINC,0},2,	{{0xc0,0xf8,RN },{0x1f,0xff,0}}},
{52,'h','E','!','E',O_ROTXL|O_WORD,"rotxl.w",1,{RNINC,0},2,	{{0xc8,0xf8,RN },{0x1e,0xff,0}}},
{49,'h','E','!','E',O_ROTXR|O_WORD,"rotxr.w",1,{RNINC,0},2,	{{0xc8,0xf8,RN },{0x1f,0xff,0}}},
{53,'h','E','!','E',O_ROTXL|O_BYTE,"rotxl.b",1,{RNIND,0},2,	{{0xd0,0xf8,RN },{0x1e,0xff,0}}},
{50,'h','E','!','E',O_ROTXR|O_BYTE,"rotxr.b",1,{RNIND,0},2,	{{0xd0,0xf8,RN },{0x1f,0xff,0}}},
{52,'h','E','!','E',O_ROTXL|O_WORD,"rotxl.w",1,{RNIND,0},2,	{{0xd8,0xf8,RN },{0x1e,0xff,0}}},
{49,'h','E','!','E',O_ROTXR|O_WORD,"rotxr.w",1,{RNIND,0},2,	{{0xd8,0xf8,RN },{0x1f,0xff,0}}},
{50,'h','E','!','E',O_ROTXR|O_BYTE,"rotxr.b",1,{RNIND_D8,0},3,	{{0xe0,0xf8,RN },{0x00,0x00,DISP8 },{0x1f,0xff,0}}},
{49,'h','E','!','E',O_ROTXR|O_WORD,"rotxr.w",1,{RNIND_D8,0},3,	{{0xe8,0xf8,RN },{0x00,0x00,DISP8 },{0x1f,0xff,0}}},
{50,'h','E','!','E',O_ROTXR|O_BYTE,"rotxr.b",1,{RNIND_D16,0},4,	{{0xf0,0xf8,RN },{0x00,0x00,DISP16 },{0x00,0x00,0},{0x1f,0xff,0}}},
{49,'h','E','!','E',O_ROTXR|O_WORD,"rotxr.w",1,{RNIND_D16,0},4,	{{0xf8,0xf8,RN },{0x00,0x00,DISP16 },{0x00,0x00,0},{0x1f,0xff,0}}},
{45,'-','B','S','S',O_SCB_F|O_UNSZ,"scb/f",2,{RS,PCREL8},3,	{{0x01,0xff,0},{0xb8,0xf8,RS },{0x00,0x00,PCREL8 }}},
{53,'h','E','!','E',O_ROTXL|O_BYTE,"rotxl.b",1,{IMM8,0},3,	{{0x04,0xff,0},{0x00,0x00,IMM8 },{0x1e,0xff,0}}},
{50,'h','E','!','E',O_ROTXR|O_BYTE,"rotxr.b",1,{IMM8,0},3,	{{0x04,0xff,0},{0x00,0x00,IMM8 },{0x1f,0xff,0}}},
{50,'h','E','!','E',O_ROTXR|O_BYTE,"rotxr.b",1,{ABS8,0},3,	{{0x05,0xff,0},{0x00,0x00,ABS8 },{0x1f,0xff,0}}},
{49,'h','E','!','E',O_ROTXR|O_WORD,"rotxr.w",1,{IMM16,0},4,	{{0x0c,0xff,0},{0x00,0x00,IMM16 },{0x00,0x00,0},{0x1f,0xff,0}}},
{42,'h','E','!','E',O_SHAL|O_BYTE,"shal.b",1,{ABS8,0},3,	{{0x05,0xff,0},{0x00,0x00,ABS8 },{0x18,0xff,0}}},
{53,'h','E','!','E',O_ROTXL|O_BYTE,"rotxl.b",1,{ABS8,0},3,	{{0x05,0xff,0},{0x00,0x00,ABS8 },{0x1e,0xff,0}}},
{44,'-','B','S','S',O_SCB_NE|O_UNSZ,"scb/ne",2,{RS,PCREL8},3,	{{0x06,0xff,0},{0xb8,0xf8,RS },{0x00,0x00,PCREL8 }}},
{46,'-','B','S','S',O_SCB_EQ|O_UNSZ,"scb/eq",2,{RS,PCREL8},3,	{{0x07,0xff,0},{0xb8,0xf8,RS },{0x00,0x00,PCREL8 }}},
{41,'h','E','!','E',O_SHAL|O_WORD,"shal.w",1,{IMM16,0},4,	{{0x0c,0xff,0},{0x00,0x00,IMM16 },{0x00,0x00,0},{0x18,0xff,0}}},
{52,'h','E','!','E',O_ROTXL|O_WORD,"rotxl.w",1,{IMM16,0},4,	{{0x0c,0xff,0},{0x00,0x00,IMM16 },{0x00,0x00,0},{0x1e,0xff,0}}},
{41,'h','E','!','E',O_SHAL|O_WORD,"shal.w",1,{ABS8,0},3,	{{0x0d,0xff,0},{0x00,0x00,ABS8 },{0x18,0xff,0}}},
{52,'h','E','!','E',O_ROTXL|O_WORD,"rotxl.w",1,{ABS8,0},3,	{{0x0d,0xff,0},{0x00,0x00,ABS8 },{0x1e,0xff,0}}},
{49,'h','E','!','E',O_ROTXR|O_WORD,"rotxr.w",1,{ABS8,0},3,	{{0x0d,0xff,0},{0x00,0x00,ABS8 },{0x1f,0xff,0}}},
{48,'-','B','!','!',O_RTD|O_UNSZ,"rtd",1,{IMM16,0},3,	{{0x14,0xff,0},{0x00,0x00,IMM16 },{0x00,0x00,0}}},
{48,'-','B','!','!',O_RTD|O_UNSZ,"rtd",1,{IMM8,0},2,	{{0x14,0xff,0},{0x00,0x00,IMM8 }}},
{42,'h','E','!','E',O_SHAL|O_BYTE,"shal.b",1,{ABS16,0},4,	{{0x15,0xff,0},{0x00,0x00,ABS16 },{0x00,0x00,0},{0x18,0xff,0}}},
{53,'h','E','!','E',O_ROTXL|O_BYTE,"rotxl.b",1,{ABS16,0},4,	{{0x15,0xff,0},{0x00,0x00,ABS16 },{0x00,0x00,0},{0x1e,0xff,0}}},
{50,'h','E','!','E',O_ROTXR|O_BYTE,"rotxr.b",1,{ABS16,0},4,	{{0x15,0xff,0},{0x00,0x00,ABS16 },{0x00,0x00,0},{0x1f,0xff,0}}},
{47,'-','B','!','!',O_RTS|O_UNSZ,"rts",0,{0,0},1,	{{0x19,0xff,0}}},
{41,'h','E','!','E',O_SHAL|O_WORD,"shal.w",1,{ABS16,0},4,	{{0x1d,0xff,0},{0x00,0x00,ABS16 },{0x00,0x00,0},{0x18,0xff,0}}},
{52,'h','E','!','E',O_ROTXL|O_WORD,"rotxl.w",1,{ABS16,0},4,	{{0x1d,0xff,0},{0x00,0x00,ABS16 },{0x00,0x00,0},{0x1e,0xff,0}}},
{49,'h','E','!','E',O_ROTXR|O_WORD,"rotxr.w",1,{ABS16,0},4,	{{0x1d,0xff,0},{0x00,0x00,ABS16 },{0x00,0x00,0},{0x1f,0xff,0}}},
{99,'m','I','!','D',O_MOV|O_BYTE,"mov:e.b",2,{IMM8,RD},2,	{{0x50,0xf8,RD },{0x00,0x00,IMM8 }}},
{97,'m','E','!','D',O_MOV|O_BYTE,"mov:f.b",2,{FPIND_D8,RD},2,	{{0x80,0xf8,RD },{0x00,0x00,FPIND_D8 }}},
{96,'m','E','!','D',O_MOV|O_WORD,"mov:f.w",2,{FPIND_D8,RD},2,	{{0x88,0xf8,RD },{0x00,0x00,FPIND_D8 }}},
{97,'m','S','!','E',O_MOV|O_BYTE,"mov:f.b",2,{RS,FPIND_D8},2,	{{0x90,0xf8,RS },{0x00,0x00,FPIND_D8 }}},
{96,'m','S','!','E',O_MOV|O_WORD,"mov:f.w",2,{RS,FPIND_D8},2,	{{0x98,0xf8,RS },{0x00,0x00,FPIND_D8 }}},
{94,'m','E','!','D',O_MOV|O_BYTE,"mov:g.b",2,{RN,RD},2,	{{0xa0,0xf8,RN },{0x80,0xf8,RD }}},
{55,'h','E','!','E',O_ROTR|O_WORD,"rotr.w",1,{RN,0},2,	{{0xa8,0xf8,RN },{0x1d,0xff,0}}},
{94,'m','S','!','E',O_MOV|O_BYTE,"mov:g.b",2,{RS,RNDEC},2,	{{0xb0,0xf8,RN },{0x90,0xf8,RS }}},
{91,'m','I','!','D',O_MOV|O_WORD,"mov:i.w",2,{IMM16,RD},3,	{{0x58,0xf8,RD },{0x00,0x00,IMM16 },{0x00,0x00,0}}},
{89,'m','E','!','D',O_MOV|O_BYTE,"mov:l.b",2,{ABS8,RD},2,	{{0x60,0xf8,RD },{0x00,0x00,ABS8 }}},
{88,'m','E','!','D',O_MOV|O_WORD,"mov:l.w",2,{ABS8,RD},2,	{{0x68,0xf8,RD },{0x00,0x00,ABS8 }}},
{86,'m','S','!','E',O_MOV|O_BYTE,"mov:s.b",2,{RS,ABS8},2,	{{0x70,0xf8,RS },{0x00,0x00,ABS8 }}},
{85,'m','S','!','E',O_MOV|O_WORD,"mov:s.w",2,{RS,ABS8},2,	{{0x78,0xf8,RS },{0x00,0x00,ABS8 }}},
{83,'-','E','!','D',O_MOVFPE|O_BYTE,"movfpe.b",2,{RN,RD},3,	{{0xa0,0xf8,RN },{0x00,0xff,0},{0x80,0xf8,RD }}},
{56,'h','E','!','E',O_ROTR|O_BYTE,"rotr.b",1,{RN,0},2,	{{0xa0,0xf8,RN },{0x1d,0xff,0}}},
{93,'m','E','!','D',O_MOV|O_WORD,"mov:g.w",2,{RN,RD},2,	{{0xa8,0xf8,RN },{0x80,0xf8,RD }}},
{94,'m','E','!','D',O_MOV|O_BYTE,"mov:g.b",2,{RNDEC,RD},2,	{{0xb0,0xf8,RN },{0x80,0xf8,RD }}},
{83,'-','E','!','D',O_MOVFPE|O_BYTE,"movfpe.b",2,{RNDEC,RD},3,	{{0xb0,0xf8,RN },{0x00,0xff,0},{0x80,0xf8,RD }}},
{79,'p','E','D','D',O_MULXU|O_BYTE,"mulxu.b",2,{RNDEC,RD},2,	{{0xb0,0xf8,RN },{0xa8,0xf8,RD }}},
{81,'-','S','!','E',O_MOVTPE|O_BYTE,"movtpe.b",2,{RS,RN},3,	{{0xa0,0xf8,RN },{0x00,0xff,0},{0x90,0xf8,RS }}},
{58,'h','E','!','E',O_ROTL|O_WORD,"rotl.w",1,{RN,0},2,	{{0xa8,0xf8,RN },{0x1c,0xff,0}}},
{69,'m','E','D','D',O_OR|O_BYTE,"or.b",2,{RN,RD},2,	{{0xa0,0xf8,RN },{0x40,0xf8,RD }}},
{79,'p','E','D','D',O_MULXU|O_BYTE,"mulxu.b",2,{RN,RD},2,	{{0xa0,0xf8,RN },{0xa8,0xf8,RD }}},
{76,'a','E','!','E',O_NEG|O_BYTE,"neg.b",1,{RN,0},2,	{{0xa0,0xf8,RN },{0x14,0xff,0}}},
{72,'m','E','!','E',O_NOT|O_BYTE,"not.b",1,{RN,0},2,	{{0xa0,0xf8,RN },{0x15,0xff,0}}},
{59,'h','E','!','E',O_ROTL|O_BYTE,"rotl.b",1,{RN,0},2,	{{0xa0,0xf8,RN },{0x1c,0xff,0}}},
{68,'m','E','D','D',O_OR|O_WORD,"or.w",2,{RN,RD},2,	{{0xa8,0xf8,RN },{0x40,0xf8,RD }}},
{78,'p','E','D','D',O_MULXU|O_WORD,"mulxu.w",2,{RN,RD},2,	{{0xa8,0xf8,RN },{0xa8,0xf8,RD }}},
{75,'a','E','!','E',O_NEG|O_WORD,"neg.w",1,{RN,0},2,	{{0xa8,0xf8,RN },{0x14,0xff,0}}},
{71,'m','E','!','E',O_NOT|O_WORD,"not.w",1,{RN,0},2,	{{0xa8,0xf8,RN },{0x15,0xff,0}}},
{69,'m','E','D','D',O_OR|O_BYTE,"or.b",2,{RNDEC,RD},2,	{{0xb0,0xf8,RN },{0x40,0xf8,RD }}},
{81,'-','S','!','E',O_MOVTPE|O_BYTE,"movtpe.b",2,{RS,RNDEC},3,	{{0xb0,0xf8,RN },{0x00,0xff,0},{0x90,0xf8,RS }}},
{59,'h','E','!','E',O_ROTL|O_BYTE,"rotl.b",1,{RNDEC,0},2,	{{0xb0,0xf8,RN },{0x1c,0xff,0}}},
{75,'a','E','!','E',O_NEG|O_WORD,"neg.w",1,{RNDEC,0},2,	{{0xb8,0xf8,RN },{0x14,0xff,0}}},
{69,'m','E','D','D',O_OR|O_BYTE,"or.b",2,{RNINC,RD},2,	{{0xc0,0xf8,RN },{0x40,0xf8,RD }}},
{59,'h','E','!','E',O_ROTL|O_BYTE,"rotl.b",1,{RNINC,0},2,	{{0xc0,0xf8,RN },{0x1c,0xff,0}}},
{76,'a','E','!','E',O_NEG|O_BYTE,"neg.b",1,{RNDEC,0},2,	{{0xb0,0xf8,RN },{0x14,0xff,0}}},
{72,'m','E','!','E',O_NOT|O_BYTE,"not.b",1,{RNDEC,0},2,	{{0xb0,0xf8,RN },{0x15,0xff,0}}},
{68,'m','E','D','D',O_OR|O_WORD,"or.w",2,{RNDEC,RD},2,	{{0xb8,0xf8,RN },{0x40,0xf8,RD }}},
{71,'m','E','!','E',O_NOT|O_WORD,"not.w",1,{RNDEC,0},2,	{{0xb8,0xf8,RN },{0x15,0xff,0}}},
{55,'h','E','!','E',O_ROTR|O_WORD,"rotr.w",1,{RNDEC,0},2,	{{0xb8,0xf8,RN },{0x1d,0xff,0}}},
{76,'a','E','!','E',O_NEG|O_BYTE,"neg.b",1,{RNINC,0},2,	{{0xc0,0xf8,RN },{0x14,0xff,0}}},
{72,'m','E','!','E',O_NOT|O_BYTE,"not.b",1,{RNINC,0},2,	{{0xc0,0xf8,RN },{0x15,0xff,0}}},
{68,'m','E','D','D',O_OR|O_WORD,"or.w",2,{RNINC,RD},2,	{{0xc8,0xf8,RN },{0x40,0xf8,RD }}},
{75,'a','E','!','E',O_NEG|O_WORD,"neg.w",1,{RNINC,0},2,	{{0xc8,0xf8,RN },{0x14,0xff,0}}},
{71,'m','E','!','E',O_NOT|O_WORD,"not.w",1,{RNINC,0},2,	{{0xc8,0xf8,RN },{0x15,0xff,0}}},
{55,'h','E','!','E',O_ROTR|O_WORD,"rotr.w",1,{RNINC,0},2,	{{0xc8,0xf8,RN },{0x1d,0xff,0}}},
{69,'m','E','D','D',O_OR|O_BYTE,"or.b",2,{RNIND,RD},2,	{{0xd0,0xf8,RN },{0x40,0xf8,RD }}},
{76,'a','E','!','E',O_NEG|O_BYTE,"neg.b",1,{RNIND,0},2,	{{0xd0,0xf8,RN },{0x14,0xff,0}}},
{72,'m','E','!','E',O_NOT|O_BYTE,"not.b",1,{RNIND,0},2,	{{0xd0,0xf8,RN },{0x15,0xff,0}}},
{59,'h','E','!','E',O_ROTL|O_BYTE,"rotl.b",1,{RNIND,0},2,	{{0xd0,0xf8,RN },{0x1c,0xff,0}}},
{68,'m','E','D','D',O_OR|O_WORD,"or.w",2,{RNIND,RD},2,	{{0xd8,0xf8,RN },{0x40,0xf8,RD }}},
{75,'a','E','!','E',O_NEG|O_WORD,"neg.w",1,{RNIND,0},2,	{{0xd8,0xf8,RN },{0x14,0xff,0}}},
{71,'m','E','!','E',O_NOT|O_WORD,"not.w",1,{RNIND,0},2,	{{0xd8,0xf8,RN },{0x15,0xff,0}}},
{55,'h','E','!','E',O_ROTR|O_WORD,"rotr.w",1,{RNIND,0},2,	{{0xd8,0xf8,RN },{0x1d,0xff,0}}},
{69,'m','E','D','D',O_OR|O_BYTE,"or.b",2,{RNIND_D8,RD},3,	{{0xe0,0xf8,RN },{0x00,0x00,DISP8 },{0x40,0xf8,RD }}},
{76,'a','E','!','E',O_NEG|O_BYTE,"neg.b",1,{RNIND_D8,0},3,	{{0xe0,0xf8,RN },{0x00,0x00,DISP8 },{0x14,0xff,0}}},
{72,'m','E','!','E',O_NOT|O_BYTE,"not.b",1,{RNIND_D8,0},3,	{{0xe0,0xf8,RN },{0x00,0x00,DISP8 },{0x15,0xff,0}}},
{59,'h','E','!','E',O_ROTL|O_BYTE,"rotl.b",1,{RNIND_D8,0},3,	{{0xe0,0xf8,RN },{0x00,0x00,DISP8 },{0x1c,0xff,0}}},
{68,'m','E','D','D',O_OR|O_WORD,"or.w",2,{RNIND_D8,RD},3,	{{0xe8,0xf8,RN },{0x00,0x00,DISP8 },{0x40,0xf8,RD }}},
{75,'a','E','!','E',O_NEG|O_WORD,"neg.w",1,{RNIND_D8,0},3,	{{0xe8,0xf8,RN },{0x00,0x00,DISP8 },{0x14,0xff,0}}},
{71,'m','E','!','E',O_NOT|O_WORD,"not.w",1,{RNIND_D8,0},3,	{{0xe8,0xf8,RN },{0x00,0x00,DISP8 },{0x15,0xff,0}}},
{55,'h','E','!','E',O_ROTR|O_WORD,"rotr.w",1,{RNIND_D8,0},3,	{{0xe8,0xf8,RN },{0x00,0x00,DISP8 },{0x1d,0xff,0}}},
{69,'m','E','D','D',O_OR|O_BYTE,"or.b",2,{RNIND_D16,RD},4,	{{0xf0,0xf8,RN },{0x00,0x00,DISP16 },{0x00,0x00,0},{0x40,0xf8,RD }}},
{68,'m','E','D','D',O_OR|O_WORD,"or.w",2,{RNIND_D16,RD},4,	{{0xf8,0xf8,RN },{0x00,0x00,DISP16 },{0x00,0x00,0},{0x40,0xf8,RD }}},
{75,'a','E','!','E',O_NEG|O_WORD,"neg.w",1,{RNIND_D16,0},4,	{{0xf8,0xf8,RN },{0x00,0x00,DISP16 },{0x00,0x00,0},{0x14,0xff,0}}},
{71,'m','E','!','E',O_NOT|O_WORD,"not.w",1,{RNIND_D16,0},4,	{{0xf8,0xf8,RN },{0x00,0x00,DISP16 },{0x00,0x00,0},{0x15,0xff,0}}},
{58,'h','E','!','E',O_ROTL|O_WORD,"rotl.w",1,{RNIND_D16,0},4,	{{0xf8,0xf8,RN },{0x00,0x00,DISP16 },{0x00,0x00,0},{0x1c,0xff,0}}},
{63,'-','J','!','!',O_PJSR|O_UNSZ,"pjsr",1,{ABS24,0},4,	{{0x03,0xff,0},{0x00,0x00,ABS24 },{0x00,0x00,0},{0x00,0x00,0}}},
{69,'m','E','D','D',O_OR|O_BYTE,"or.b",2,{IMM8,RD},3,	{{0x04,0xff,0},{0x00,0x00,IMM8 },{0x40,0xf8,RD }}},
{66,'s','I','C','C',O_ORC|O_BYTE,"orc.b",2,{IMM8,CRB},3,	{{0x04,0xff,0},{0x00,0x00,IMM8 },{0x48,0xf8,CRB }}},
{76,'a','E','!','E',O_NEG|O_BYTE,"neg.b",1,{IMM8,0},3,	{{0x04,0xff,0},{0x00,0x00,IMM8 },{0x14,0xff,0}}},
{59,'h','E','!','E',O_ROTL|O_BYTE,"rotl.b",1,{IMM8,0},3,	{{0x04,0xff,0},{0x00,0x00,IMM8 },{0x1c,0xff,0}}},
{69,'m','E','D','D',O_OR|O_BYTE,"or.b",2,{ABS8,RD},3,	{{0x05,0xff,0},{0x00,0x00,ABS8 },{0x40,0xf8,RD }}},
{94,'m','E','!','D',O_MOV|O_BYTE,"mov:g.b",2,{RNIND_D16,RD},4,	{{0xf0,0xf8,RN },{0x00,0x00,DISP16 },{0x00,0x00,0},{0x80,0xf8,RD }}},
{76,'a','E','!','E',O_NEG|O_BYTE,"neg.b",1,{RNIND_D16,0},4,	{{0xf0,0xf8,RN },{0x00,0x00,DISP16 },{0x00,0x00,0},{0x14,0xff,0}}},
{72,'m','E','!','E',O_NOT|O_BYTE,"not.b",1,{RNIND_D16,0},4,	{{0xf0,0xf8,RN },{0x00,0x00,DISP16 },{0x00,0x00,0},{0x15,0xff,0}}},
{59,'h','E','!','E',O_ROTL|O_BYTE,"rotl.b",1,{RNIND_D16,0},4,	{{0xf0,0xf8,RN },{0x00,0x00,DISP16 },{0x00,0x00,0},{0x1c,0xff,0}}},
{72,'m','E','!','E',O_NOT|O_BYTE,"not.b",1,{IMM8,0},3,	{{0x04,0xff,0},{0x00,0x00,IMM8 },{0x15,0xff,0}}},
{76,'a','E','!','E',O_NEG|O_BYTE,"neg.b",1,{ABS8,0},3,	{{0x05,0xff,0},{0x00,0x00,ABS8 },{0x14,0xff,0}}},
{72,'m','E','!','E',O_NOT|O_BYTE,"not.b",1,{ABS8,0},3,	{{0x05,0xff,0},{0x00,0x00,ABS8 },{0x15,0xff,0}}},
{94,'m','I','!','E',O_MOV|O_BYTE,"mov:g.b",2,{IMM8,RNIND_D16},5,	{{0xf0,0xf8,RN },{0x00,0x00,DISP16 },{0x00,0x00,0},{0x06,0xff,0},{0x00,0x00,IMM8 }}},
{74,'-','!','!','!',O_NOP|O_UNSZ,"nop",0,{0,0},1,	{{0x00,0xff,0}}},
{59,'h','E','!','E',O_ROTL|O_BYTE,"rotl.b",1,{ABS8,0},3,	{{0x05,0xff,0},{0x00,0x00,ABS8 },{0x1c,0xff,0}}},
{68,'m','E','D','D',O_OR|O_WORD,"or.w",2,{IMM16,RD},4,	{{0x0c,0xff,0},{0x00,0x00,IMM16 },{0x00,0x00,0},{0x40,0xf8,RD }}},
{65,'s','I','C','C',O_ORC|O_WORD,"orc.w",2,{IMM16,CRW},4,	{{0x0c,0xff,0},{0x00,0x00,IMM16 },{0x00,0x00,0},{0x48,0xf8,CRW }}},
{75,'a','E','!','E',O_NEG|O_WORD,"neg.w",1,{IMM16,0},4,	{{0x0c,0xff,0},{0x00,0x00,IMM16 },{0x00,0x00,0},{0x14,0xff,0}}},
{71,'m','E','!','E',O_NOT|O_WORD,"not.w",1,{IMM16,0},4,	{{0x0c,0xff,0},{0x00,0x00,IMM16 },{0x00,0x00,0},{0x15,0xff,0}}},
{58,'h','E','!','E',O_ROTL|O_WORD,"rotl.w",1,{IMM16,0},4,	{{0x0c,0xff,0},{0x00,0x00,IMM16 },{0x00,0x00,0},{0x1c,0xff,0}}},
{68,'m','E','D','D',O_OR|O_WORD,"or.w",2,{ABS8,RD},3,	{{0x0d,0xff,0},{0x00,0x00,ABS8 },{0x40,0xf8,RD }}},
{75,'a','E','!','E',O_NEG|O_WORD,"neg.w",1,{ABS8,0},3,	{{0x0d,0xff,0},{0x00,0x00,ABS8 },{0x14,0xff,0}}},
{71,'m','E','!','E',O_NOT|O_WORD,"not.w",1,{ABS8,0},3,	{{0x0d,0xff,0},{0x00,0x00,ABS8 },{0x15,0xff,0}}},
{55,'h','E','!','E',O_ROTR|O_WORD,"rotr.w",1,{ABS8,0},3,	{{0x0d,0xff,0},{0x00,0x00,ABS8 },{0x1d,0xff,0}}},
{64,'-','J','!','!',O_PJMP|O_UNSZ,"pjmp",1,{RDIND,0},2,	{{0x11,0xff,0},{0xc0,0xf8,RDIND }}},
{63,'-','J','!','!',O_PJSR|O_UNSZ,"pjsr",1,{RDIND,0},2,	{{0x11,0xff,0},{0xc8,0xf8,RDIND }}},
{62,'-','B','!','!',O_PRTD|O_UNSZ,"prtd",1,{IMM8,0},3,	{{0x11,0xff,0},{0x14,0xff,0},{0x00,0x00,IMM8 }}},
{61,'-','B','!','!',O_PRTS|O_UNSZ,"prts",0,{0,0},2,	{{0x11,0xff,0},{0x19,0xff,0}}},
{62,'-','B','!','!',O_PRTD|O_UNSZ,"prtd",1,{IMM16,0},4,	{{0x11,0xff,0},{0x1c,0xff,0},{0x00,0x00,IMM16 },{0x00,0x00,0}}},
{64,'-','J','!','!',O_PJMP|O_UNSZ,"pjmp",1,{ABS24,0},4,	{{0x13,0xff,0},{0x00,0x00,ABS24 },{0x00,0x00,0},{0x00,0x00,0}}},
{69,'m','E','D','D',O_OR|O_BYTE,"or.b",2,{ABS16,RD},4,	{{0x15,0xff,0},{0x00,0x00,ABS16 },{0x00,0x00,0},{0x40,0xf8,RD }}},
{78,'p','E','D','D',O_MULXU|O_WORD,"mulxu.w",2,{RNDEC,RD},2,	{{0xb8,0xf8,RN },{0xa8,0xf8,RD }}},
{58,'h','E','!','E',O_ROTL|O_WORD,"rotl.w",1,{RNDEC,0},2,	{{0xb8,0xf8,RN },{0x1c,0xff,0}}},
{79,'p','E','D','D',O_MULXU|O_BYTE,"mulxu.b",2,{RNINC,RD},2,	{{0xc0,0xf8,RN },{0xa8,0xf8,RD }}},
{78,'p','E','D','D',O_MULXU|O_WORD,"mulxu.w",2,{RNINC,RD},2,	{{0xc8,0xf8,RN },{0xa8,0xf8,RD }}},
{79,'p','E','D','D',O_MULXU|O_BYTE,"mulxu.b",2,{RNIND,RD},2,	{{0xd0,0xf8,RN },{0xa8,0xf8,RD }}},
{78,'p','E','D','D',O_MULXU|O_WORD,"mulxu.w",2,{RNIND,RD},2,	{{0xd8,0xf8,RN },{0xa8,0xf8,RD }}},
{79,'p','E','D','D',O_MULXU|O_BYTE,"mulxu.b",2,{RNIND_D8,RD},3,	{{0xe0,0xf8,RN },{0x00,0x00,DISP8 },{0xa8,0xf8,RD }}},
{78,'p','E','D','D',O_MULXU|O_WORD,"mulxu.w",2,{RNIND_D8,RD},3,	{{0xe8,0xf8,RN },{0x00,0x00,DISP8 },{0xa8,0xf8,RD }}},
{58,'h','E','!','E',O_ROTL|O_WORD,"rotl.w",1,{RNIND_D8,0},3,	{{0xe8,0xf8,RN },{0x00,0x00,DISP8 },{0x1c,0xff,0}}},
{79,'p','E','D','D',O_MULXU|O_BYTE,"mulxu.b",2,{RNIND_D16,RD},4,	{{0xf0,0xf8,RN },{0x00,0x00,DISP16 },{0x00,0x00,0},{0xa8,0xf8,RD }}},
{78,'p','E','D','D',O_MULXU|O_WORD,"mulxu.w",2,{RNIND_D16,RD},4,	{{0xf8,0xf8,RN },{0x00,0x00,DISP16 },{0x00,0x00,0},{0xa8,0xf8,RD }}},
{79,'p','E','D','D',O_MULXU|O_BYTE,"mulxu.b",2,{IMM8,RD},3,	{{0x04,0xff,0},{0x00,0x00,IMM8 },{0xa8,0xf8,RD }}},
{79,'p','E','D','D',O_MULXU|O_BYTE,"mulxu.b",2,{ABS8,RD},3,	{{0x05,0xff,0},{0x00,0x00,ABS8 },{0xa8,0xf8,RD }}},
{78,'p','E','D','D',O_MULXU|O_WORD,"mulxu.w",2,{IMM16,RD},4,	{{0x0c,0xff,0},{0x00,0x00,IMM16 },{0x00,0x00,0},{0xa8,0xf8,RD }}},
{78,'p','E','D','D',O_MULXU|O_WORD,"mulxu.w",2,{ABS8,RD},3,	{{0x0d,0xff,0},{0x00,0x00,ABS8 },{0xa8,0xf8,RD }}},
{58,'h','E','!','E',O_ROTL|O_WORD,"rotl.w",1,{ABS8,0},3,	{{0x0d,0xff,0},{0x00,0x00,ABS8 },{0x1c,0xff,0}}},
{94,'m','S','!','E',O_MOV|O_BYTE,"mov:g.b",2,{RS,ABS16},4,	{{0x15,0xff,0},{0x00,0x00,ABS16 },{0x00,0x00,0},{0x90,0xf8,RS }}},
{79,'p','E','D','D',O_MULXU|O_BYTE,"mulxu.b",2,{ABS16,RD},4,	{{0x15,0xff,0},{0x00,0x00,ABS16 },{0x00,0x00,0},{0xa8,0xf8,RD }}},
{81,'-','S','!','E',O_MOVTPE|O_BYTE,"movtpe.b",2,{RS,RNINC},3,	{{0xc0,0xf8,RN },{0x00,0xff,0},{0x90,0xf8,RS }}},
{58,'h','E','!','E',O_ROTL|O_WORD,"rotl.w",1,{RNINC,0},2,	{{0xc8,0xf8,RN },{0x1c,0xff,0}}},
{81,'-','S','!','E',O_MOVTPE|O_BYTE,"movtpe.b",2,{RS,RNIND},3,	{{0xd0,0xf8,RN },{0x00,0xff,0},{0x90,0xf8,RS }}},
{58,'h','E','!','E',O_ROTL|O_WORD,"rotl.w",1,{RNIND,0},2,	{{0xd8,0xf8,RN },{0x1c,0xff,0}}},
{81,'-','S','!','E',O_MOVTPE|O_BYTE,"movtpe.b",2,{RS,RNIND_D8},4,	{{0xe0,0xf8,RN },{0x00,0x00,DISP8 },{0x00,0xff,0},{0x90,0xf8,RS }}},
{94,'m','S','!','E',O_MOV|O_BYTE,"mov:g.b",2,{RS,RNIND_D16},4,	{{0xf0,0xf8,RN },{0x00,0x00,DISP16 },{0x00,0x00,0},{0x90,0xf8,RS }}},
{81,'-','S','!','E',O_MOVTPE|O_BYTE,"movtpe.b",2,{RS,RNIND_D16},5,	{{0xf0,0xf8,RN },{0x00,0x00,DISP16 },{0x00,0x00,0},{0x00,0xff,0},{0x90,0xf8,RS }}},
{81,'-','S','!','E',O_MOVTPE|O_BYTE,"movtpe.b",2,{RS,ABS8},4,	{{0x05,0xff,0},{0x00,0x00,ABS8 },{0x00,0xff,0},{0x90,0xf8,RS }}},
{83,'-','E','!','D',O_MOVFPE|O_BYTE,"movfpe.b",2,{RNIND,RD},3,	{{0xd0,0xf8,RN },{0x00,0xff,0},{0x80,0xf8,RD }}},
{83,'-','E','!','D',O_MOVFPE|O_BYTE,"movfpe.b",2,{RNINC,RD},3,	{{0xc0,0xf8,RN },{0x00,0xff,0},{0x80,0xf8,RD }}},
{56,'h','E','!','E',O_ROTR|O_BYTE,"rotr.b",1,{RNIND_D16,0},4,	{{0xf0,0xf8,RN },{0x00,0x00,DISP16 },{0x00,0x00,0},{0x1d,0xff,0}}},
{94,'m','E','!','D',O_MOV|O_BYTE,"mov:g.b",2,{ABS16,RD},4,	{{0x15,0xff,0},{0x00,0x00,ABS16 },{0x00,0x00,0},{0x80,0xf8,RD }}},
{83,'-','E','!','D',O_MOVFPE|O_BYTE,"movfpe.b",2,{RNIND_D8,RD},4,	{{0xe0,0xf8,RN },{0x00,0x00,DISP8 },{0x00,0xff,0},{0x80,0xf8,RD }}},
{94,'m','I','!','E',O_MOV|O_BYTE,"mov:g.b",2,{IMM8,RNIND_D8},4,	{{0xe0,0xf8,RN },{0x00,0x00,DISP8 },{0x06,0xff,0},{0x00,0x00,IMM8 }}},
{83,'-','E','!','D',O_MOVFPE|O_BYTE,"movfpe.b",2,{RNIND_D16,RD},5,	{{0xf0,0xf8,RN },{0x00,0x00,DISP16 },{0x00,0x00,0},{0x00,0xff,0},{0x80,0xf8,RD }}},
{83,'-','E','!','D',O_MOVFPE|O_BYTE,"movfpe.b",2,{IMM8,RD},4,	{{0x04,0xff,0},{0x00,0x00,IMM8 },{0x00,0xff,0},{0x80,0xf8,RD }}},
{83,'-','E','!','D',O_MOVFPE|O_BYTE,"movfpe.b",2,{ABS8,RD},4,	{{0x05,0xff,0},{0x00,0x00,ABS8 },{0x00,0xff,0},{0x80,0xf8,RD }}},
{56,'h','E','!','E',O_ROTR|O_BYTE,"rotr.b",1,{RNINC,0},2,	{{0xc0,0xf8,RN },{0x1d,0xff,0}}},
{56,'h','E','!','E',O_ROTR|O_BYTE,"rotr.b",1,{RNIND,0},2,	{{0xd0,0xf8,RN },{0x1d,0xff,0}}},
{56,'h','E','!','E',O_ROTR|O_BYTE,"rotr.b",1,{RNIND_D8,0},3,	{{0xe0,0xf8,RN },{0x00,0x00,DISP8 },{0x1d,0xff,0}}},
{93,'m','E','!','D',O_MOV|O_WORD,"mov:g.w",2,{RNDEC,RD},2,	{{0xb8,0xf8,RN },{0x80,0xf8,RD }}},
{93,'m','E','!','D',O_MOV|O_WORD,"mov:g.w",2,{RNINC,RD},2,	{{0xc8,0xf8,RN },{0x80,0xf8,RD }}},
{56,'h','E','!','E',O_ROTR|O_BYTE,"rotr.b",1,{RNDEC,0},2,	{{0xb0,0xf8,RN },{0x1d,0xff,0}}},
{93,'m','S','!','E',O_MOV|O_WORD,"mov:g.w",2,{RS,RNDEC},2,	{{0xb8,0xf8,RN },{0x90,0xf8,RS }}},
{93,'m','S','!','E',O_MOV|O_WORD,"mov:g.w",2,{RS,RNINC},2,	{{0xc8,0xf8,RN },{0x90,0xf8,RS }}},
{93,'m','E','!','D',O_MOV|O_WORD,"mov:g.w",2,{RNIND,RD},2,	{{0xd8,0xf8,RN },{0x80,0xf8,RD }}},
{93,'m','S','!','E',O_MOV|O_WORD,"mov:g.w",2,{RS,RNIND},2,	{{0xd8,0xf8,RN },{0x90,0xf8,RS }}},
{93,'m','E','!','D',O_MOV|O_WORD,"mov:g.w",2,{RNIND_D8,RD},3,	{{0xe8,0xf8,RN },{0x00,0x00,DISP8 },{0x80,0xf8,RD }}},
{93,'m','S','!','E',O_MOV|O_WORD,"mov:g.w",2,{RS,RNIND_D8},3,	{{0xe8,0xf8,RN },{0x00,0x00,DISP8 },{0x90,0xf8,RS }}},
{56,'h','E','!','E',O_ROTR|O_BYTE,"rotr.b",1,{IMM8,0},3,	{{0x04,0xff,0},{0x00,0x00,IMM8 },{0x1d,0xff,0}}},
{56,'h','E','!','E',O_ROTR|O_BYTE,"rotr.b",1,{ABS8,0},3,	{{0x05,0xff,0},{0x00,0x00,ABS8 },{0x1d,0xff,0}}},
{93,'m','S','!','E',O_MOV|O_WORD,"mov:g.w",2,{RS,ABS8},3,	{{0x0d,0xff,0},{0x00,0x00,ABS8 },{0x90,0xf8,RS }}},
{94,'m','I','!','E',O_MOV|O_BYTE,"mov:g.b",2,{IMM8,RNDEC},3,	{{0xb0,0xf8,RN },{0x06,0xff,0},{0x00,0x00,IMM8 }}},
{93,'m','I','!','E',O_MOV|O_WORD,"mov:g.w",2,{IMM16,RNDEC},4,	{{0xb8,0xf8,RN },{0x07,0xff,0},{0x00,0x00,IMM16 },{0x00,0x00,0}}},

{93,'m','I','!','E',O_MOV|O_WORD,"mov:g.w",2,{IMM8,RNDEC},4,	{{0xb8,0xf8,RN },{0x06,0xff,0},{0x00,0x00,IMM8 },{0x00,0x00,0}}},


{94,'m','E','!','D',O_MOV|O_BYTE,"mov:g.b",2,{RNINC,RD},2,	{{0xc0,0xf8,RN },{0x80,0xf8,RD }}},
{94,'m','S','!','E',O_MOV|O_BYTE,"mov:g.b",2,{RS,RNINC},2,	{{0xc0,0xf8,RN },{0x90,0xf8,RS }}},
{94,'m','I','!','E',O_MOV|O_BYTE,"mov:g.b",2,{IMM8,RNINC},3,	{{0xc0,0xf8,RN },{0x06,0xff,0},{0x00,0x00,IMM8 }}},
{93,'m','I','!','E',O_MOV|O_WORD,"mov:g.w",2,{IMM16,RNINC},4,	{{0xc8,0xf8,RN },{0x07,0xff,0},{0x00,0x00,IMM16 },{0x00,0x00,0}}},

{93,'m','I','!','E',O_MOV|O_WORD,"mov:g.w",2,{IMM8,RNINC},4,	{{0xc8,0xf8,RN },{0x06,0xff,0},{0x00,0x00,IMM8 },{0x00,0x00,0}}},

{94,'m','E','!','D',O_MOV|O_BYTE,"mov:g.b",2,{RNIND,RD},2,	{{0xd0,0xf8,RN },{0x80,0xf8,RD }}},
{94,'m','S','!','E',O_MOV|O_BYTE,"mov:g.b",2,{RS,RNIND},2,	{{0xd0,0xf8,RN },{0x90,0xf8,RS }}},
{94,'m','I','!','E',O_MOV|O_BYTE,"mov:g.b",2,{IMM8,RNIND},3,	{{0xd0,0xf8,RN },{0x06,0xff,0},{0x00,0x00,IMM8 }}},

{93,'m','I','!','E',O_MOV|O_WORD,"mov:g.w",2,{IMM16,RNIND},4,	{{0xd8,0xf8,RN },{0x07,0xff,0},{0x00,0x00,IMM16 },{0x00,0x00,0}}},
{93,'m','I','!','E',O_MOV|O_WORD,"mov:g.w",2,{IMM8,RNIND},4,	{{0xd8,0xf8,RN },{0x06,0xff,0},{0x00,0x00,IMM8 },{0x00,0x00,0}}},

{94,'m','E','!','D',O_MOV|O_BYTE,"mov:g.b",2,{RNIND_D8,RD},3,	{{0xe0,0xf8,RN },{0x00,0x00,DISP8 },{0x80,0xf8,RD }}},
{94,'m','S','!','E',O_MOV|O_BYTE,"mov:g.b",2,{RS,RNIND_D8},3,	{{0xe0,0xf8,RN },{0x00,0x00,DISP8 },{0x90,0xf8,RS }}},


{93,'m','I','!','E',O_MOV|O_WORD,"mov:g.w",2,{IMM16,RNIND_D8},5,	{{0xe8,0xf8,RN },{0x00,0x00,DISP8 },{0x07,0xff,0},{0x00,0x00,IMM16 },{0x00,0x00,0}}},

{93,'m','I','!','E',O_MOV|O_WORD,"mov:g.w",2,{IMM8,RNIND_D8},5,	{{0xe8,0xf8,RN },{0x00,0x00,DISP8 },{0x06,0xff,0},{0x00,0x00,IMM8 },{0x00,0x00,0}}},


{93,'m','I','!','E',O_MOV|O_WORD,"mov:g.w",2,{IMM16,RNIND_D16},6,	{{0xf8,0xf8,RN },{0x00,0x00,DISP16 },{0x00,0x00,0},{0x07,0xff,0},{0x00,0x00,IMM16 },{0x00,0x00,0}}},

{93,'m','I','!','E',O_MOV|O_WORD,"mov:g.w",2,{IMM8,RNIND_D16},6,	{{0xf8,0xf8,RN },{0x00,0x00,DISP16 },{0x00,0x00,0},{0x06,0xff,0},{0x00,0x00,IMM8 },{0x00,0x00,0}}},

{93,'m','E','!','D',O_MOV|O_WORD,"mov:g.w",2,{RNIND_D16,RD},4,	{{0xf8,0xf8,RN },{0x00,0x00,DISP16 },{0x00,0x00,0},{0x80,0xf8,RD }}},
{93,'m','S','!','E',O_MOV|O_WORD,"mov:g.w",2,{RS,RNIND_D16},4,	{{0xf8,0xf8,RN },{0x00,0x00,DISP16 },{0x00,0x00,0},{0x90,0xf8,RS }}},
{55,'h','E','!','E',O_ROTR|O_WORD,"rotr.w",1,{RNIND_D16,0},4,	{{0xf8,0xf8,RN },{0x00,0x00,DISP16 },{0x00,0x00,0},{0x1d,0xff,0}}},
{94,'m','E','!','D',O_MOV|O_BYTE,"mov:g.b",2,{IMM8,RD},3,	{{0x04,0xff,0},{0x00,0x00,IMM8 },{0x80,0xf8,RD }}},
{94,'m','E','!','D',O_MOV|O_BYTE,"mov:g.b",2,{ABS8,RD},3,	{{0x05,0xff,0},{0x00,0x00,ABS8 },{0x80,0xf8,RD }}},
{94,'m','S','!','E',O_MOV|O_BYTE,"mov:g.b",2,{RS,ABS8},3,	{{0x05,0xff,0},{0x00,0x00,ABS8 },{0x90,0xf8,RS }}},
{94,'m','I','!','E',O_MOV|O_BYTE,"mov:g.b",2,{IMM8,ABS8},4,	{{0x05,0xff,0},{0x00,0x00,ABS8 },{0x06,0xff,0},{0x00,0x00,IMM8 }}},
{93,'m','I','!','E',O_MOV|O_WORD,"mov:g.w",2,{IMM16,ABS8},5,	{{0x0d,0xff,0},{0x00,0x00,ABS8 },{0x07,0xff,0},{0x00,0x00,IMM16 },{0x00,0x00,0}}},
{93,'m','E','!','D',O_MOV|O_WORD,"mov:g.w",2,{IMM16,RD},4,	{{0x0c,0xff,0},{0x00,0x00,IMM16 },{0x00,0x00,0},{0x80,0xf8,RD }}},

{93,'m','I','!','E',O_MOV|O_WORD,"mov:g.w",2,{IMM8,ABS8},5,	{{0x0d,0xff,0},{0x00,0x00,ABS8 },{0x06,0xff,0},{0x00,0x00,IMM8 },{0x00,0x00,0}}},


{55,'h','E','!','E',O_ROTR|O_WORD,"rotr.w",1,{IMM16,0},4,	{{0x0c,0xff,0},{0x00,0x00,IMM16 },{0x00,0x00,0},{0x1d,0xff,0}}},
{93,'m','E','!','D',O_MOV|O_WORD,"mov:g.w",2,{ABS8,RD},3,	{{0x0d,0xff,0},{0x00,0x00,ABS8 },{0x80,0xf8,RD }}},
{83,'-','E','!','D',O_MOVFPE|O_BYTE,"movfpe.b",2,{ABS16,RD},5,	{{0x15,0xff,0},{0x00,0x00,ABS16 },{0x00,0x00,0},{0x00,0xff,0},{0x80,0xf8,RD }}},
{81,'-','S','!','E',O_MOVTPE|O_BYTE,"movtpe.b",2,{RS,ABS16},5,	{{0x15,0xff,0},{0x00,0x00,ABS16 },{0x00,0x00,0},{0x00,0xff,0},{0x90,0xf8,RS }}},
{94,'m','I','!','E',O_MOV|O_BYTE,"mov:g.b",2,{IMM8,ABS16},5,	{{0x15,0xff,0},{0x00,0x00,ABS16 },{0x00,0x00,0},{0x06,0xff,0},{0x00,0x00,IMM8 }}},

{93,'m','I','!','E',O_MOV|O_WORD,"mov:g.w",2,{IMM16,ABS16},6,	{{0x1d,0xff,0},{0x00,0x00,ABS16 },{0x00,0x00,0},{0x07,0xff,0},{0x00,0x00,IMM16 },{0x00,0x00,0}}},

{93,'m','I','!','E',O_MOV|O_WORD,"mov:g.w",2,{IMM8,ABS16},6,	{{0x1d,0xff,0},{0x00,0x00,ABS16 },{0x00,0x00,0},{0x06,0xff,0},{0x00,0x00,IMM8 },{0x00,0x00,0}}},

{76,'a','E','!','E',O_NEG|O_BYTE,"neg.b",1,{ABS16,0},4,	{{0x15,0xff,0},{0x00,0x00,ABS16 },{0x00,0x00,0},{0x14,0xff,0}}},
{56,'h','E','!','E',O_ROTR|O_BYTE,"rotr.b",1,{ABS16,0},4,	{{0x15,0xff,0},{0x00,0x00,ABS16 },{0x00,0x00,0},{0x1d,0xff,0}}},
{93,'m','E','!','D',O_MOV|O_WORD,"mov:g.w",2,{ABS16,RD},4,	{{0x1d,0xff,0},{0x00,0x00,ABS16 },{0x00,0x00,0},{0x80,0xf8,RD }}},
{93,'m','S','!','E',O_MOV|O_WORD,"mov:g.w",2,{RS,ABS16},4,	{{0x1d,0xff,0},{0x00,0x00,ABS16 },{0x00,0x00,0},{0x90,0xf8,RS }}},
{78,'p','E','D','D',O_MULXU|O_WORD,"mulxu.w",2,{ABS16,RD},4,	{{0x1d,0xff,0},{0x00,0x00,ABS16 },{0x00,0x00,0},{0xa8,0xf8,RD }}},
{75,'a','E','!','E',O_NEG|O_WORD,"neg.w",1,{ABS16,0},4,	{{0x1d,0xff,0},{0x00,0x00,ABS16 },{0x00,0x00,0},{0x14,0xff,0}}},
{55,'h','E','!','E',O_ROTR|O_WORD,"rotr.w",1,{ABS16,0},4,	{{0x1d,0xff,0},{0x00,0x00,ABS16 },{0x00,0x00,0},{0x1d,0xff,0}}},
{72,'m','E','!','E',O_NOT|O_BYTE,"not.b",1,{ABS16,0},4,	{{0x15,0xff,0},{0x00,0x00,ABS16 },{0x00,0x00,0},{0x15,0xff,0}}},
{68,'m','E','D','D',O_OR|O_WORD,"or.w",2,{ABS16,RD},4,	{{0x1d,0xff,0},{0x00,0x00,ABS16 },{0x00,0x00,0},{0x40,0xf8,RD }}},
{71,'m','E','!','E',O_NOT|O_WORD,"not.w",1,{ABS16,0},4,	{{0x1d,0xff,0},{0x00,0x00,ABS16 },{0x00,0x00,0},{0x15,0xff,0}}},
{58,'h','E','!','E',O_ROTL|O_WORD,"rotl.w",1,{ABS16,0},4,	{{0x1d,0xff,0},{0x00,0x00,ABS16 },{0x00,0x00,0},{0x1c,0xff,0}}},
{59,'h','E','!','E',O_ROTL|O_BYTE,"rotl.b",1,{ABS16,0},4,	{{0x15,0xff,0},{0x00,0x00,ABS16 },{0x00,0x00,0},{0x1c,0xff,0}}},
{125,'a','D','I','!',O_CMP|O_BYTE,"cmp:e.b",2,{IMM8,RD},2,	{{0x40,0xf8,RD },{0x00,0x00,IMM8 }}},
{123,'a','D','E','!',O_CMP|O_BYTE,"cmp:g.b",2,{RN,RD},2,	{{0xa0,0xf8,RN },{0x70,0xf8,RD }}},
{123,'a','E','I','!',O_CMP|O_BYTE,"cmp:g.b",2,{IMM8,RN},3,	{{0xa0,0xf8,RN },{0x04,0xff,0},{0x00,0x00,IMM8 }}},
{122,'a','D','E','!',O_CMP|O_WORD,"cmp:g.w",2,{RN,RD},2,	{{0xa8,0xf8,RN },{0x70,0xf8,RD }}},
{122,'a','E','I','!',O_CMP|O_WORD,"cmp:g.w",2,{IMM16,RN},4,	{{0xa8,0xf8,RN },{0x05,0xff,0},{0x00,0x00,IMM16 },{0x00,0x00,0}}},
{107,'s','E','!','C',O_LDC|O_BYTE,"ldc.b",2,{RN,CRB},2,	{{0xa0,0xf8,RN },{0x88,0xf8,CRB }}},
{120,'a','D','I','!',O_CMP|O_WORD,"cmp:i.w",2,{IMM16,RD},3,	{{0x48,0xf8,RD },{0x00,0x00,IMM16 },{0x00,0x00,0}}},
{117,'s','E','D','D',O_DIVXU|O_BYTE,"divxu.b",2,{RN,RD},2,	{{0xa0,0xf8,RN },{0xb8,0xf8,RD }}},
{119,'s','D','!','!',O_DADD|O_UNSZ,"dadd",2,{RS,RD},3,	{{0xa0,0xf8,RS },{0x00,0xff,0},{0xa0,0xf8,RD }}},
{115,'s','D','!','!',O_DSUB|O_UNSZ,"dsub",2,{RS,RD},3,	{{0xa0,0xf8,RS },{0x00,0xff,0},{0xb0,0xf8,RD }}},
{113,'s','D','!','D',O_EXTS|O_BYTE,"exts.b",1,{RD,0},2,	{{0xa0,0xf8,RD },{0x11,0xff,0}}},
{111,'s','D','!','D',O_EXTU|O_BYTE,"extu.b",1,{RD,0},2,	{{0xa0,0xf8,RD },{0x12,0xff,0}}},
{116,'s','E','D','D',O_DIVXU|O_WORD,"divxu.w",2,{RN,RD},2,	{{0xa8,0xf8,RN },{0xb8,0xf8,RD }}},
{107,'s','E','!','C',O_LDC|O_BYTE,"ldc.b",2,{RNIND_D8,CRB},3,	{{0xe0,0xf8,RN },{0x00,0x00,DISP8 },{0x88,0xf8,CRB }}},
{107,'s','E','!','C',O_LDC|O_BYTE,"ldc.b",2,{RNIND_D16,CRB},4,	{{0xf0,0xf8,RN },{0x00,0x00,DISP16 },{0x00,0x00,0},{0x88,0xf8,CRB }}},
{110,'-','B','!','!',O_JMP|O_UNSZ,"jmp",1,{ABS16,0},3,	{{0x10,0xff,0},{0x00,0x00,ABS16 },{0x00,0x00,0}}},
{110,'-','B','!','!',O_JMP|O_UNSZ,"jmp",1,{RDIND,0},2,	{{0x11,0xff,0},{0xd0,0xf8,RD }}},
{109,'-','B','!','!',O_JSR|O_UNSZ,"jsr",1,{RDIND,0},2,	{{0x11,0xff,0},{0xd8,0xf8,RD }}},
{110,'-','B','!','!',O_JMP|O_UNSZ,"jmp",1,{RDIND_D8,0},3,	{{0x11,0xff,0},{0xe0,0xf8,RDIND_D8 },{0x00,0x00,0}}},
{109,'-','B','!','!',O_JSR|O_UNSZ,"jsr",1,{RDIND_D8,0},3,	{{0x11,0xff,0},{0xe8,0xf8,RDIND_D8 },{0x00,0x00,0}}},
{110,'-','B','!','!',O_JMP|O_UNSZ,"jmp",1,{RDIND_D16,0},4,	{{0x11,0xff,0},{0xf0,0xf8,RDIND_D16 },{0x00,0x00,0},{0x00,0x00,0}}},
{109,'-','B','!','!',O_JSR|O_UNSZ,"jsr",1,{RDIND_D16,0},4,	{{0x11,0xff,0},{0xf8,0xf8,RDIND_D16 },{0x00,0x00,0},{0x00,0x00,0}}},
{109,'-','B','!','!',O_JSR|O_UNSZ,"jsr",1,{ABS16,0},3,	{{0x18,0xff,0},{0x00,0x00,ABS16 },{0x00,0x00,0}}},
{107,'s','E','!','C',O_LDC|O_BYTE,"ldc.b",2,{ABS16,CRB},4,	{{0x15,0xff,0},{0x00,0x00,ABS16 },{0x00,0x00,0},{0x88,0xf8,CRB }}},
{117,'s','E','D','D',O_DIVXU|O_BYTE,"divxu.b",2,{RNDEC,RD},2,	{{0xb0,0xf8,RN },{0xb8,0xf8,RD }}},
{116,'s','E','D','D',O_DIVXU|O_WORD,"divxu.w",2,{RNDEC,RD},2,	{{0xb8,0xf8,RN },{0xb8,0xf8,RD }}},
{107,'s','E','!','C',O_LDC|O_BYTE,"ldc.b",2,{RNINC,CRB},2,	{{0xc0,0xf8,RN },{0x88,0xf8,CRB }}},
{117,'s','E','D','D',O_DIVXU|O_BYTE,"divxu.b",2,{RNINC,RD},2,	{{0xc0,0xf8,RN },{0xb8,0xf8,RD }}},
{116,'s','E','D','D',O_DIVXU|O_WORD,"divxu.w",2,{RNINC,RD},2,	{{0xc8,0xf8,RN },{0xb8,0xf8,RD }}},
{107,'s','E','!','C',O_LDC|O_BYTE,"ldc.b",2,{RNIND,CRB},2,	{{0xd0,0xf8,RN },{0x88,0xf8,CRB }}},
{117,'s','E','D','D',O_DIVXU|O_BYTE,"divxu.b",2,{RNIND,RD},2,	{{0xd0,0xf8,RN },{0xb8,0xf8,RD }}},
{116,'s','E','D','D',O_DIVXU|O_WORD,"divxu.w",2,{RNIND,RD},2,	{{0xd8,0xf8,RN },{0xb8,0xf8,RD }}},
{117,'s','E','D','D',O_DIVXU|O_BYTE,"divxu.b",2,{RNIND_D8,RD},3,	{{0xe0,0xf8,RN },{0x00,0x00,DISP8 },{0xb8,0xf8,RD }}},
{116,'s','E','D','D',O_DIVXU|O_WORD,"divxu.w",2,{RNIND_D8,RD},3,	{{0xe8,0xf8,RN },{0x00,0x00,DISP8 },{0xb8,0xf8,RD }}},
{117,'s','E','D','D',O_DIVXU|O_BYTE,"divxu.b",2,{RNIND_D16,RD},4,	{{0xf0,0xf8,RN },{0x00,0x00,DISP16 },{0x00,0x00,0},{0xb8,0xf8,RD }}},
{116,'s','E','D','D',O_DIVXU|O_WORD,"divxu.w",2,{RNIND_D16,RD},4,	{{0xf8,0xf8,RN },{0x00,0x00,DISP16 },{0x00,0x00,0},{0xb8,0xf8,RD }}},
{107,'s','E','!','C',O_LDC|O_BYTE,"ldc.b",2,{IMM8,CRB},3,	{{0x04,0xff,0},{0x00,0x00,IMM8 },{0x88,0xf8,CRB }}},
{117,'s','E','D','D',O_DIVXU|O_BYTE,"divxu.b",2,{IMM8,RD},3,	{{0x04,0xff,0},{0x00,0x00,IMM8 },{0xb8,0xf8,RD }}},
{117,'s','E','D','D',O_DIVXU|O_BYTE,"divxu.b",2,{ABS8,RD},3,	{{0x05,0xff,0},{0x00,0x00,ABS8 },{0xb8,0xf8,RD }}},
{116,'s','E','D','D',O_DIVXU|O_WORD,"divxu.w",2,{IMM16,RD},4,	{{0x0c,0xff,0},{0x00,0x00,IMM16 },{0x00,0x00,0},{0xb8,0xf8,RD }}},
{117,'s','E','D','D',O_DIVXU|O_BYTE,"divxu.b",2,{ABS16,RD},4,	{{0x15,0xff,0},{0x00,0x00,ABS16 },{0x00,0x00,0},{0xb8,0xf8,RD }}},
{116,'s','E','D','D',O_DIVXU|O_WORD,"divxu.w",2,{ABS16,RD},4,	{{0x1d,0xff,0},{0x00,0x00,ABS16 },{0x00,0x00,0},{0xb8,0xf8,RD }}},
{107,'s','E','!','C',O_LDC|O_BYTE,"ldc.b",2,{ABS8,CRB},3,	{{0x05,0xff,0},{0x00,0x00,ABS8 },{0x88,0xf8,CRB }}},
{116,'s','E','D','D',O_DIVXU|O_WORD,"divxu.w",2,{ABS8,RD},3,	{{0x0d,0xff,0},{0x00,0x00,ABS8 },{0xb8,0xf8,RD }}},
{123,'a','D','E','!',O_CMP|O_BYTE,"cmp:g.b",2,{RNDEC,RD},2,	{{0xb0,0xf8,RN },{0x70,0xf8,RD }}},
{107,'s','E','!','C',O_LDC|O_BYTE,"ldc.b",2,{RNDEC,CRB},2,	{{0xb0,0xf8,RN },{0x88,0xf8,CRB }}},
{123,'a','E','I','!',O_CMP|O_BYTE,"cmp:g.b",2,{IMM8,RNDEC},3,	{{0xb0,0xf8,RN },{0x04,0xff,0},{0x00,0x00,IMM8 }}},
{122,'a','E','I','!',O_CMP|O_WORD,"cmp:g.w",2,{IMM16,RNDEC},4,	{{0xb8,0xf8,RN },{0x05,0xff,0},{0x00,0x00,IMM16 },{0x00,0x00,0}}},
{122,'a','D','E','!',O_CMP|O_WORD,"cmp:g.w",2,{RNDEC,RD},2,	{{0xb8,0xf8,RN },{0x70,0xf8,RD }}},
{123,'a','D','E','!',O_CMP|O_BYTE,"cmp:g.b",2,{RNINC,RD},2,	{{0xc0,0xf8,RN },{0x70,0xf8,RD }}},
{123,'a','E','I','!',O_CMP|O_BYTE,"cmp:g.b",2,{IMM8,RNINC},3,	{{0xc0,0xf8,RN },{0x04,0xff,0},{0x00,0x00,IMM8 }}},
{122,'a','E','I','!',O_CMP|O_WORD,"cmp:g.w",2,{IMM16,RNINC},4,	{{0xc8,0xf8,RN },{0x05,0xff,0},{0x00,0x00,IMM16 },{0x00,0x00,0}}},
{122,'a','D','E','!',O_CMP|O_WORD,"cmp:g.w",2,{RNINC,RD},2,	{{0xc8,0xf8,RN },{0x70,0xf8,RD }}},
{123,'a','D','E','!',O_CMP|O_BYTE,"cmp:g.b",2,{RNIND,RD},2,	{{0xd0,0xf8,RN },{0x70,0xf8,RD }}},
{123,'a','E','I','!',O_CMP|O_BYTE,"cmp:g.b",2,{IMM8,RNIND},3,	{{0xd0,0xf8,RN },{0x04,0xff,0},{0x00,0x00,IMM8 }}},
{122,'a','E','I','!',O_CMP|O_WORD,"cmp:g.w",2,{IMM16,RNIND},4,	{{0xd8,0xf8,RN },{0x05,0xff,0},{0x00,0x00,IMM16 },{0x00,0x00,0}}},
{122,'a','D','E','!',O_CMP|O_WORD,"cmp:g.w",2,{RNIND,RD},2,	{{0xd8,0xf8,RN },{0x70,0xf8,RD }}},
{123,'a','D','E','!',O_CMP|O_BYTE,"cmp:g.b",2,{RNIND_D8,RD},3,	{{0xe0,0xf8,RN },{0x00,0x00,DISP8 },{0x70,0xf8,RD }}},
{123,'a','E','I','!',O_CMP|O_BYTE,"cmp:g.b",2,{IMM8,RNIND_D8},4,	{{0xe0,0xf8,RN },{0x00,0x00,DISP8 },{0x04,0xff,0},{0x00,0x00,IMM8 }}},
{122,'a','E','I','!',O_CMP|O_WORD,"cmp:g.w",2,{IMM16,RNIND_D8},5,	{{0xe8,0xf8,RN },{0x00,0x00,DISP8 },{0x05,0xff,0},{0x00,0x00,IMM16 },{0x00,0x00,0}}},
{122,'a','D','E','!',O_CMP|O_WORD,"cmp:g.w",2,{RNIND_D8,RD},3,	{{0xe8,0xf8,RN },{0x00,0x00,DISP8 },{0x70,0xf8,RD }}},
{123,'a','D','E','!',O_CMP|O_BYTE,"cmp:g.b",2,{RNIND_D16,RD},4,	{{0xf0,0xf8,RN },{0x00,0x00,DISP16 },{0x00,0x00,0},{0x70,0xf8,RD }}},
{123,'a','E','I','!',O_CMP|O_BYTE,"cmp:g.b",2,{IMM8,RNIND_D16},5,	{{0xf0,0xf8,RN },{0x00,0x00,DISP16 },{0x00,0x00,0},{0x04,0xff,0},{0x00,0x00,IMM8 }}},
{122,'a','E','I','!',O_CMP|O_WORD,"cmp:g.w",2,{IMM16,RNIND_D16},6,	{{0xf8,0xf8,RN },{0x00,0x00,DISP16 },{0x00,0x00,0},{0x05,0xff,0},{0x00,0x00,IMM16 },{0x00,0x00,0}}},
{122,'a','D','E','!',O_CMP|O_WORD,"cmp:g.w",2,{RNIND_D16,RD},4,	{{0xf8,0xf8,RN },{0x00,0x00,DISP16 },{0x00,0x00,0},{0x70,0xf8,RD }}},
{105,'-','E','!','C',O_LDM|O_UNSZ,"ldm",2,{SPINC,RLIST},2,	{{0x02,0xff,0},{0x00,0x00,RLIST }}},
{123,'a','D','E','!',O_CMP|O_BYTE,"cmp:g.b",2,{IMM8,RD},3,	{{0x04,0xff,0},{0x00,0x00,IMM8 },{0x70,0xf8,RD }}},
{123,'a','D','E','!',O_CMP|O_BYTE,"cmp:g.b",2,{ABS8,RD},3,	{{0x05,0xff,0},{0x00,0x00,ABS8 },{0x70,0xf8,RD }}},
{123,'a','E','I','!',O_CMP|O_BYTE,"cmp:g.b",2,{IMM8,ABS8},4,	{{0x05,0xff,0},{0x00,0x00,ABS8 },{0x04,0xff,0},{0x00,0x00,IMM8 }}},
{122,'a','E','I','!',O_CMP|O_WORD,"cmp:g.w",2,{IMM16,ABS8},5,	{{0x0d,0xff,0},{0x00,0x00,ABS8 },{0x05,0xff,0},{0x00,0x00,IMM16 },{0x00,0x00,0}}},
{122,'a','D','E','!',O_CMP|O_WORD,"cmp:g.w",2,{IMM16,RD},4,	{{0x0c,0xff,0},{0x00,0x00,IMM16 },{0x00,0x00,0},{0x70,0xf8,RD }}},
{106,'s','E','!','C',O_LDC|O_WORD,"ldc.w",2,{IMM16,CRW},4,	{{0x0c,0xff,0},{0x00,0x00,IMM16 },{0x00,0x00,0},{0x88,0xf8,CRW }}},
{122,'a','D','E','!',O_CMP|O_WORD,"cmp:g.w",2,{ABS8,RD},3,	{{0x0d,0xff,0},{0x00,0x00,ABS8 },{0x70,0xf8,RD }}},
{123,'a','D','E','!',O_CMP|O_BYTE,"cmp:g.b",2,{ABS16,RD},4,	{{0x15,0xff,0},{0x00,0x00,ABS16 },{0x00,0x00,0},{0x70,0xf8,RD }}},
{123,'a','E','I','!',O_CMP|O_BYTE,"cmp:g.b",2,{IMM8,ABS16},5,	{{0x15,0xff,0},{0x00,0x00,ABS16 },{0x00,0x00,0},{0x04,0xff,0},{0x00,0x00,IMM8 }}},
{122,'a','E','I','!',O_CMP|O_WORD,"cmp:g.w",2,{IMM16,ABS16},6,	{{0x1d,0xff,0},{0x00,0x00,ABS16 },{0x00,0x00,0},{0x05,0xff,0},{0x00,0x00,IMM16 },{0x00,0x00,0}}},
{122,'a','D','E','!',O_CMP|O_WORD,"cmp:g.w",2,{ABS16,RD},4,	{{0x1d,0xff,0},{0x00,0x00,ABS16 },{0x00,0x00,0},{0x70,0xf8,RD }}},
{104,'-','S','I','!',O_LINK|O_UNSZ,"link",2,{FP,IMM16},3,	{{0x1f,0xff,0},{0x00,0x00,IMM16 },{0x00,0x00,0}}},
{104,'-','S','I','!',O_LINK|O_UNSZ,"link",2,{FP,IMM8},2,	{{0x17,0xff,0},{0x00,0x00,IMM8 }}},
{149,'b','E','I','E',O_BSET|O_BYTE,"bset.b",2,{IMM4,RN},2,	{{0xa0,0xf8,RN },{0xc0,0xf0,IMM4 }}},
{140,'b','E','I','E',O_BTST|O_BYTE,"btst.b",2,{IMM4,RN},2,	{{0xa0,0xf8,RN },{0xf0,0xf0,IMM4 }}},
{149,'b','E','S','E',O_BSET|O_BYTE,"bset.b",2,{RS,RN},2,	{{0xa0,0xf8,RN },{0x48,0xf8,RS }}},
{140,'b','E','S','E',O_BTST|O_BYTE,"btst.b",2,{RS,RN},2,	{{0xa0,0xf8,RN },{0x78,0xf8,RS }}},
{131,'c','!','!','E',O_CLR|O_BYTE,"clr.b",1,{RN,0},2,	{{0xa0,0xf8,RN },{0x13,0xff,0}}},
{148,'b','E','I','E',O_BSET|O_WORD,"bset.w",2,{IMM4,RN},2,	{{0xa8,0xf8,RN },{0xc0,0xf0,IMM4 }}},
{139,'b','E','I','E',O_BTST|O_WORD,"btst.w",2,{IMM4,RN},2,	{{0xa8,0xf8,RN },{0xf0,0xf0,IMM4 }}},
{131,'c','!','!','E',O_CLR|O_BYTE,"clr.b",1,{RNDEC,0},2,	{{0xb0,0xf8,RN },{0x13,0xff,0}}},
{131,'c','!','!','E',O_CLR|O_BYTE,"clr.b",1,{RNINC,0},2,	{{0xc0,0xf8,RN },{0x13,0xff,0}}},
{131,'c','!','!','E',O_CLR|O_BYTE,"clr.b",1,{RNIND,0},2,	{{0xd0,0xf8,RN },{0x13,0xff,0}}},
{139,'b','E','S','E',O_BTST|O_WORD,"btst.w",2,{RS,RNIND},2,	{{0xd8,0xf8,RN },{0x78,0xf8,RS }}},
{131,'c','!','!','E',O_CLR|O_BYTE,"clr.b",1,{RNIND_D8,0},3,	{{0xe0,0xf8,RN },{0x00,0x00,DISP8 },{0x13,0xff,0}}},
{140,'b','E','I','E',O_BTST|O_BYTE,"btst.b",2,{IMM4,RNIND_D16},4,	{{0xf0,0xf8,RN },{0x00,0x00,DISP16 },{0x00,0x00,0},{0xf0,0xf0,IMM4 }}},
{140,'b','E','S','E',O_BTST|O_BYTE,"btst.b",2,{RS,RNIND_D16},4,	{{0xf0,0xf8,RN },{0x00,0x00,DISP16 },{0x00,0x00,0},{0x78,0xf8,RS }}},
{131,'c','!','!','E',O_CLR|O_BYTE,"clr.b",1,{RNIND_D16,0},4,	{{0xf0,0xf8,RN },{0x00,0x00,DISP16 },{0x00,0x00,0},{0x13,0xff,0}}},
{130,'c','!','!','E',O_CLR|O_WORD,"clr.w",1,{RNIND_D16,0},4,	{{0xf8,0xf8,RN },{0x00,0x00,DISP16 },{0x00,0x00,0},{0x13,0xff,0}}},
{131,'c','!','!','E',O_CLR|O_BYTE,"clr.b",1,{IMM8,0},3,	{{0x04,0xff,0},{0x00,0x00,IMM8 },{0x13,0xff,0}}},
{131,'c','!','!','E',O_CLR|O_BYTE,"clr.b",1,{ABS8,0},3,	{{0x05,0xff,0},{0x00,0x00,ABS8 },{0x13,0xff,0}}},
{140,'b','E','I','E',O_BTST|O_BYTE,"btst.b",2,{IMM4,ABS8},3,	{{0x05,0xff,0},{0x00,0x00,ABS8 },{0xf0,0xf0,IMM4 }}},
{140,'b','E','S','E',O_BTST|O_BYTE,"btst.b",2,{RS,ABS8},3,	{{0x05,0xff,0},{0x00,0x00,ABS8 },{0x78,0xf8,RS }}},
{139,'b','E','S','E',O_BTST|O_WORD,"btst.w",2,{RS,RN},2,	{{0xa8,0xf8,RN },{0x78,0xf8,RS }}},
{140,'b','E','I','E',O_BTST|O_BYTE,"btst.b",2,{IMM4,RNDEC},2,	{{0xb0,0xf8,RN },{0xf0,0xf0,IMM4 }}},
{140,'b','E','S','E',O_BTST|O_BYTE,"btst.b",2,{RS,RNDEC},2,	{{0xb0,0xf8,RN },{0x78,0xf8,RS }}},
{139,'b','E','I','E',O_BTST|O_WORD,"btst.w",2,{IMM4,RNDEC},2,	{{0xb8,0xf8,RN },{0xf0,0xf0,IMM4 }}},
{139,'b','E','S','E',O_BTST|O_WORD,"btst.w",2,{RS,RNDEC},2,	{{0xb8,0xf8,RN },{0x78,0xf8,RS }}},
{130,'c','!','!','E',O_CLR|O_WORD,"clr.w",1,{RNDEC,0},2,	{{0xb8,0xf8,RN },{0x13,0xff,0}}},
{140,'b','E','I','E',O_BTST|O_BYTE,"btst.b",2,{IMM4,RNINC},2,	{{0xc0,0xf8,RN },{0xf0,0xf0,IMM4 }}},
{140,'b','E','S','E',O_BTST|O_BYTE,"btst.b",2,{RS,RNINC},2,	{{0xc0,0xf8,RN },{0x78,0xf8,RS }}},
{139,'b','E','I','E',O_BTST|O_WORD,"btst.w",2,{IMM4,RNINC},2,	{{0xc8,0xf8,RN },{0xf0,0xf0,IMM4 }}},
{139,'b','E','S','E',O_BTST|O_WORD,"btst.w",2,{RS,RNINC},2,	{{0xc8,0xf8,RN },{0x78,0xf8,RS }}},
{140,'b','E','I','E',O_BTST|O_BYTE,"btst.b",2,{IMM4,RNIND},2,	{{0xd0,0xf8,RN },{0xf0,0xf0,IMM4 }}},
{140,'b','E','S','E',O_BTST|O_BYTE,"btst.b",2,{RS,RNIND},2,	{{0xd0,0xf8,RN },{0x78,0xf8,RS }}},
{139,'b','E','I','E',O_BTST|O_WORD,"btst.w",2,{IMM4,RNIND},2,	{{0xd8,0xf8,RN },{0xf0,0xf0,IMM4 }}},
{140,'b','E','I','E',O_BTST|O_BYTE,"btst.b",2,{IMM4,RNIND_D8},3,	{{0xe0,0xf8,RN },{0x00,0x00,DISP8 },{0xf0,0xf0,IMM4 }}},
{140,'b','E','S','E',O_BTST|O_BYTE,"btst.b",2,{RS,RNIND_D8},3,	{{0xe0,0xf8,RN },{0x00,0x00,DISP8 },{0x78,0xf8,RS }}},
{139,'b','E','I','E',O_BTST|O_WORD,"btst.w",2,{IMM4,RNIND_D8},3,	{{0xe8,0xf8,RN },{0x00,0x00,DISP8 },{0xf0,0xf0,IMM4 }}},
{139,'b','E','S','E',O_BTST|O_WORD,"btst.w",2,{RS,RNIND_D8},3,	{{0xe8,0xf8,RN },{0x00,0x00,DISP8 },{0x78,0xf8,RS }}},
{130,'c','!','!','E',O_CLR|O_WORD,"clr.w",1,{RNIND_D8,0},3,	{{0xe8,0xf8,RN },{0x00,0x00,DISP8 },{0x13,0xff,0}}},
{139,'b','E','I','E',O_BTST|O_WORD,"btst.w",2,{IMM4,RNIND_D16},4,	{{0xf8,0xf8,RN },{0x00,0x00,DISP16 },{0x00,0x00,0},{0xf0,0xf0,IMM4 }}},
{139,'b','E','S','E',O_BTST|O_WORD,"btst.w",2,{RS,RNIND_D16},4,	{{0xf8,0xf8,RN },{0x00,0x00,DISP16 },{0x00,0x00,0},{0x78,0xf8,RS }}},
{130,'c','!','!','E',O_CLR|O_WORD,"clr.w",1,{IMM16,0},4,	{{0x0c,0xff,0},{0x00,0x00,IMM16 },{0x00,0x00,0},{0x13,0xff,0}}},
{139,'b','E','I','E',O_BTST|O_WORD,"btst.w",2,{IMM4,ABS8},3,	{{0x0d,0xff,0},{0x00,0x00,ABS8 },{0xf0,0xf0,IMM4 }}},
{139,'b','E','S','E',O_BTST|O_WORD,"btst.w",2,{RS,ABS8},3,	{{0x0d,0xff,0},{0x00,0x00,ABS8 },{0x78,0xf8,RS }}},
{130,'c','!','!','E',O_CLR|O_WORD,"clr.w",1,{ABS8,0},3,	{{0x0d,0xff,0},{0x00,0x00,ABS8 },{0x13,0xff,0}}},
{140,'b','E','I','E',O_BTST|O_BYTE,"btst.b",2,{IMM4,ABS16},4,	{{0x15,0xff,0},{0x00,0x00,ABS16 },{0x00,0x00,0},{0xf0,0xf0,IMM4 }}},
{140,'b','E','S','E',O_BTST|O_BYTE,"btst.b",2,{RS,ABS16},4,	{{0x15,0xff,0},{0x00,0x00,ABS16 },{0x00,0x00,0},{0x78,0xf8,RS }}},
{131,'c','!','!','E',O_CLR|O_BYTE,"clr.b",1,{ABS16,0},4,	{{0x15,0xff,0},{0x00,0x00,ABS16 },{0x00,0x00,0},{0x13,0xff,0}}},
{139,'b','E','I','E',O_BTST|O_WORD,"btst.w",2,{IMM4,ABS16},4,	{{0x1d,0xff,0},{0x00,0x00,ABS16 },{0x00,0x00,0},{0xf0,0xf0,IMM4 }}},
{139,'b','E','S','E',O_BTST|O_WORD,"btst.w",2,{RS,ABS16},4,	{{0x1d,0xff,0},{0x00,0x00,ABS16 },{0x00,0x00,0},{0x78,0xf8,RS }}},
{130,'c','!','!','E',O_CLR|O_WORD,"clr.w",1,{ABS16,0},4,	{{0x1d,0xff,0},{0x00,0x00,ABS16 },{0x00,0x00,0},{0x13,0xff,0}}},
{137,'-','B','!','!',O_BVC|O_BYTE,"bvc.b",1,{PCREL8,0},2,	{{0x28,0xff,0},{0x00,0x00,PCREL8 }}},
{134,'-','B','!','!',O_BVS|O_BYTE,"bvs.b",1,{PCREL8,0},2,	{{0x29,0xff,0},{0x00,0x00,PCREL8 }}},
{136,'-','B','!','!',O_BVC|O_WORD,"bvc.w",1,{PCREL16,0},3,	{{0x38,0xff,0},{0x00,0x00,PCREL16 },{0x00,0x00,0}}},
{133,'-','B','!','!',O_BVS|O_WORD,"bvs.w",1,{PCREL16,0},3,	{{0x39,0xff,0},{0x00,0x00,PCREL16 },{0x00,0x00,0}}},
{149,'b','E','I','E',O_BSET|O_BYTE,"bset.b",2,{IMM4,RNDEC},2,	{{0xb0,0xf8,RN },{0xc0,0xf0,IMM4 }}},
{149,'b','E','S','E',O_BSET|O_BYTE,"bset.b",2,{RS,RNDEC},2,	{{0xb0,0xf8,RN },{0x48,0xf8,RS }}},
{148,'b','E','S','E',O_BSET|O_WORD,"bset.w",2,{RS,RN},2,	{{0xa8,0xf8,RN },{0x48,0xf8,RS }}},
{130,'c','!','!','E',O_CLR|O_WORD,"clr.w",1,{RN,0},2,	{{0xa8,0xf8,RN },{0x13,0xff,0}}},
{148,'b','E','I','E',O_BSET|O_WORD,"bset.w",2,{IMM4,RNDEC},2,	{{0xb8,0xf8,RN },{0xc0,0xf0,IMM4 }}},
{148,'b','E','S','E',O_BSET|O_WORD,"bset.w",2,{RS,RNDEC},2,	{{0xb8,0xf8,RN },{0x48,0xf8,RS }}},
{149,'b','E','I','E',O_BSET|O_BYTE,"bset.b",2,{IMM4,RNINC},2,	{{0xc0,0xf8,RN },{0xc0,0xf0,IMM4 }}},
{149,'b','E','S','E',O_BSET|O_BYTE,"bset.b",2,{RS,RNINC},2,	{{0xc0,0xf8,RN },{0x48,0xf8,RS }}},
{148,'b','E','I','E',O_BSET|O_WORD,"bset.w",2,{IMM4,RNINC},2,	{{0xc8,0xf8,RN },{0xc0,0xf0,IMM4 }}},
{148,'b','E','S','E',O_BSET|O_WORD,"bset.w",2,{RS,RNINC},2,	{{0xc8,0xf8,RN },{0x48,0xf8,RS }}},
{130,'c','!','!','E',O_CLR|O_WORD,"clr.w",1,{RNINC,0},2,	{{0xc8,0xf8,RN },{0x13,0xff,0}}},
{149,'b','E','I','E',O_BSET|O_BYTE,"bset.b",2,{IMM4,RNIND},2,	{{0xd0,0xf8,RN },{0xc0,0xf0,IMM4 }}},
{149,'b','E','S','E',O_BSET|O_BYTE,"bset.b",2,{RS,RNIND},2,	{{0xd0,0xf8,RN },{0x48,0xf8,RS }}},
{148,'b','E','I','E',O_BSET|O_WORD,"bset.w",2,{IMM4,RNIND},2,	{{0xd8,0xf8,RN },{0xc0,0xf0,IMM4 }}},
{148,'b','E','S','E',O_BSET|O_WORD,"bset.w",2,{RS,RNIND},2,	{{0xd8,0xf8,RN },{0x48,0xf8,RS }}},
{130,'c','!','!','E',O_CLR|O_WORD,"clr.w",1,{RNIND,0},2,	{{0xd8,0xf8,RN },{0x13,0xff,0}}},
{149,'b','E','I','E',O_BSET|O_BYTE,"bset.b",2,{IMM4,RNIND_D8},3,	{{0xe0,0xf8,RN },{0x00,0x00,DISP8 },{0xc0,0xf0,IMM4 }}},
{149,'b','E','S','E',O_BSET|O_BYTE,"bset.b",2,{RS,RNIND_D8},3,	{{0xe0,0xf8,RN },{0x00,0x00,DISP8 },{0x48,0xf8,RS }}},
{148,'b','E','I','E',O_BSET|O_WORD,"bset.w",2,{IMM4,RNIND_D8},3,	{{0xe8,0xf8,RN },{0x00,0x00,DISP8 },{0xc0,0xf0,IMM4 }}},
{148,'b','E','S','E',O_BSET|O_WORD,"bset.w",2,{RS,RNIND_D8},3,	{{0xe8,0xf8,RN },{0x00,0x00,DISP8 },{0x48,0xf8,RS }}},
{149,'b','E','I','E',O_BSET|O_BYTE,"bset.b",2,{IMM4,RNIND_D16},4,	{{0xf0,0xf8,RN },{0x00,0x00,DISP16 },{0x00,0x00,0},{0xc0,0xf0,IMM4 }}},
{149,'b','E','S','E',O_BSET|O_BYTE,"bset.b",2,{RS,RNIND_D16},4,	{{0xf0,0xf8,RN },{0x00,0x00,DISP16 },{0x00,0x00,0},{0x48,0xf8,RS }}},
{148,'b','E','I','E',O_BSET|O_WORD,"bset.w",2,{IMM4,RNIND_D16},4,	{{0xf8,0xf8,RN },{0x00,0x00,DISP16 },{0x00,0x00,0},{0xc0,0xf0,IMM4 }}},
{148,'b','E','S','E',O_BSET|O_WORD,"bset.w",2,{RS,RNIND_D16},4,	{{0xf8,0xf8,RN },{0x00,0x00,DISP16 },{0x00,0x00,0},{0x48,0xf8,RS }}},
{149,'b','E','I','E',O_BSET|O_BYTE,"bset.b",2,{IMM4,ABS8},3,	{{0x05,0xff,0},{0x00,0x00,ABS8 },{0xc0,0xf0,IMM4 }}},
{149,'b','E','S','E',O_BSET|O_BYTE,"bset.b",2,{RS,ABS8},3,	{{0x05,0xff,0},{0x00,0x00,ABS8 },{0x48,0xf8,RS }}},
{148,'b','E','I','E',O_BSET|O_WORD,"bset.w",2,{IMM4,ABS8},3,	{{0x0d,0xff,0},{0x00,0x00,ABS8 },{0xc0,0xf0,IMM4 }}},
{148,'b','E','S','E',O_BSET|O_WORD,"bset.w",2,{RS,ABS8},3,	{{0x0d,0xff,0},{0x00,0x00,ABS8 },{0x48,0xf8,RS }}},
{146,'-','B','!','!',O_BSR|O_BYTE,"bsr.b",1,{PCREL8,0},2,	{{0x0e,0xff,0},{0x00,0x00,PCREL8 }}},
{149,'b','E','I','E',O_BSET|O_BYTE,"bset.b",2,{IMM4,ABS16},4,	{{0x15,0xff,0},{0x00,0x00,ABS16 },{0x00,0x00,0},{0xc0,0xf0,IMM4 }}},
{149,'b','E','S','E',O_BSET|O_BYTE,"bset.b",2,{RS,ABS16},4,	{{0x15,0xff,0},{0x00,0x00,ABS16 },{0x00,0x00,0},{0x48,0xf8,RS }}},
{148,'b','E','I','E',O_BSET|O_WORD,"bset.w",2,{IMM4,ABS16},4,	{{0x1d,0xff,0},{0x00,0x00,ABS16 },{0x00,0x00,0},{0xc0,0xf0,IMM4 }}},
{148,'b','E','S','E',O_BSET|O_WORD,"bset.w",2,{RS,ABS16},4,	{{0x1d,0xff,0},{0x00,0x00,ABS16 },{0x00,0x00,0},{0x48,0xf8,RS }}},
{145,'-','B','!','!',O_BSR|O_WORD,"bsr.w",1,{PCREL16,0},3,	{{0x1e,0xff,0},{0x00,0x00,PCREL16 },{0x00,0x00,0}}},
{204,'b','E','I','E',O_BCLR|O_BYTE,"bclr.b",2,{IMM4,RN},2,	{{0xa0,0xf8,RN },{0xd0,0xf0,IMM4 }}},
{162,'b','E','I','E',O_BNOT|O_BYTE,"bnot.b",2,{IMM4,RN},2,	{{0xa0,0xf8,RN },{0xe0,0xf0,IMM4 }}},
{222,'a','I','E','E',O_ADD|O_BYTE,"add:q.b",2,{QIM,RN},2,	{{0xa0,0xf8,RN },{0x08,0xf8,QIM }}},
{219,'-','E','D','D',O_ADDS|O_BYTE,"adds.b",2,{RN,RD},2,	{{0xa0,0xf8,RN },{0x28,0xf8,RD }}},
{225,'a','E','D','D',O_ADD|O_BYTE,"add:g.b",2,{RN,RD},2,	{{0xa0,0xf8,RN },{0x20,0xf8,RD }}},
{213,'m','E','D','D',O_AND|O_BYTE,"and.b",2,{RN,RD},2,	{{0xa0,0xf8,RN },{0x50,0xf8,RD }}},
{204,'b','E','S','E',O_BCLR|O_BYTE,"bclr.b",2,{RS,RN},2,	{{0xa0,0xf8,RN },{0x58,0xf8,RS }}},
{162,'b','E','S','E',O_BNOT|O_BYTE,"bnot.b",2,{RS,RN},2,	{{0xa0,0xf8,RN },{0x68,0xf8,RS }}},
{216,'a','E','D','D',O_ADDX|O_BYTE,"addx.b",2,{RN,RD},2,	{{0xa0,0xf8,RN },{0xa0,0xf8,RD }}},
{203,'b','E','I','E',O_BCLR|O_WORD,"bclr.w",2,{IMM4,RN},2,	{{0xa8,0xf8,RN },{0xd0,0xf0,IMM4 }}},
{161,'b','E','I','E',O_BNOT|O_WORD,"bnot.w",2,{IMM4,RN},2,	{{0xa8,0xf8,RN },{0xe0,0xf0,IMM4 }}},
{221,'a','I','E','E',O_ADD|O_WORD,"add:q.w",2,{QIM,RN},2,	{{0xa8,0xf8,RN },{0x08,0xf8,QIM }}},
{224,'a','E','D','D',O_ADD|O_WORD,"add:g.w",2,{RN,RD},2,	{{0xa8,0xf8,RN },{0x20,0xf8,RD }}},
{218,'-','E','D','D',O_ADDS|O_WORD,"adds.w",2,{RN,RD},2,	{{0xa8,0xf8,RN },{0x28,0xf8,RD }}},
{212,'m','E','D','D',O_AND|O_WORD,"and.w",2,{RN,RD},2,	{{0xa8,0xf8,RN },{0x50,0xf8,RD }}},
{203,'b','E','S','E',O_BCLR|O_WORD,"bclr.w",2,{RS,RN},2,	{{0xa8,0xf8,RN },{0x58,0xf8,RS }}},
{161,'b','E','S','E',O_BNOT|O_WORD,"bnot.w",2,{RS,RN},2,	{{0xa8,0xf8,RN },{0x68,0xf8,RS }}},
{215,'a','E','D','D',O_ADDX|O_WORD,"addx.w",2,{RN,RD},2,	{{0xa8,0xf8,RN },{0xa0,0xf8,RD }}},
{204,'b','E','I','E',O_BCLR|O_BYTE,"bclr.b",2,{IMM4,RNDEC},2,	{{0xb0,0xf8,RN },{0xd0,0xf0,IMM4 }}},
{162,'b','E','I','E',O_BNOT|O_BYTE,"bnot.b",2,{IMM4,RNDEC},2,	{{0xb0,0xf8,RN },{0xe0,0xf0,IMM4 }}},
{222,'a','I','E','E',O_ADD|O_BYTE,"add:q.b",2,{QIM,RNDEC},2,	{{0xb0,0xf8,RN },{0x08,0xf8,QIM }}},
{225,'a','E','D','D',O_ADD|O_BYTE,"add:g.b",2,{RNDEC,RD},2,	{{0xb0,0xf8,RN },{0x20,0xf8,RD }}},
{219,'-','E','D','D',O_ADDS|O_BYTE,"adds.b",2,{RNDEC,RD},2,	{{0xb0,0xf8,RN },{0x28,0xf8,RD }}},
{213,'m','E','D','D',O_AND|O_BYTE,"and.b",2,{RNDEC,RD},2,	{{0xb0,0xf8,RN },{0x50,0xf8,RD }}},
{204,'b','E','S','E',O_BCLR|O_BYTE,"bclr.b",2,{RS,RNDEC},2,	{{0xb0,0xf8,RN },{0x58,0xf8,RS }}},
{162,'b','E','S','E',O_BNOT|O_BYTE,"bnot.b",2,{RS,RNDEC},2,	{{0xb0,0xf8,RN },{0x68,0xf8,RS }}},
{216,'a','E','D','D',O_ADDX|O_BYTE,"addx.b",2,{RNDEC,RD},2,	{{0xb0,0xf8,RN },{0xa0,0xf8,RD }}},
{203,'b','E','I','E',O_BCLR|O_WORD,"bclr.w",2,{IMM4,RNDEC},2,	{{0xb8,0xf8,RN },{0xd0,0xf0,IMM4 }}},
{161,'b','E','I','E',O_BNOT|O_WORD,"bnot.w",2,{IMM4,RNDEC},2,	{{0xb8,0xf8,RN },{0xe0,0xf0,IMM4 }}},
{221,'a','I','E','E',O_ADD|O_WORD,"add:q.w",2,{QIM,RNDEC},2,	{{0xb8,0xf8,RN },{0x08,0xf8,QIM }}},
{224,'a','E','D','D',O_ADD|O_WORD,"add:g.w",2,{RNDEC,RD},2,	{{0xb8,0xf8,RN },{0x20,0xf8,RD }}},
{218,'-','E','D','D',O_ADDS|O_WORD,"adds.w",2,{RNDEC,RD},2,	{{0xb8,0xf8,RN },{0x28,0xf8,RD }}},
{212,'m','E','D','D',O_AND|O_WORD,"and.w",2,{RNDEC,RD},2,	{{0xb8,0xf8,RN },{0x50,0xf8,RD }}},
{203,'b','E','S','E',O_BCLR|O_WORD,"bclr.w",2,{RS,RNDEC},2,	{{0xb8,0xf8,RN },{0x58,0xf8,RS }}},
{161,'b','E','S','E',O_BNOT|O_WORD,"bnot.w",2,{RS,RNDEC},2,	{{0xb8,0xf8,RN },{0x68,0xf8,RS }}},
{215,'a','E','D','D',O_ADDX|O_WORD,"addx.w",2,{RNDEC,RD},2,	{{0xb8,0xf8,RN },{0xa0,0xf8,RD }}},
{225,'a','E','D','D',O_ADD|O_BYTE,"add:g.b",2,{RNINC,RD},2,	{{0xc0,0xf8,RN },{0x20,0xf8,RD }}},
{213,'m','E','D','D',O_AND|O_BYTE,"and.b",2,{RNINC,RD},2,	{{0xc0,0xf8,RN },{0x50,0xf8,RD }}},
{203,'b','E','I','E',O_BCLR|O_WORD,"bclr.w",2,{IMM4,RNINC},2,	{{0xc8,0xf8,RN },{0xd0,0xf0,IMM4 }}},
{204,'b','E','I','E',O_BCLR|O_BYTE,"bclr.b",2,{IMM4,RNINC},2,	{{0xc0,0xf8,RN },{0xd0,0xf0,IMM4 }}},
{162,'b','E','I','E',O_BNOT|O_BYTE,"bnot.b",2,{IMM4,RNINC},2,	{{0xc0,0xf8,RN },{0xe0,0xf0,IMM4 }}},
{219,'-','E','D','D',O_ADDS|O_BYTE,"adds.b",2,{RNINC,RD},2,	{{0xc0,0xf8,RN },{0x28,0xf8,RD }}},
{204,'b','E','S','E',O_BCLR|O_BYTE,"bclr.b",2,{RS,RNINC},2,	{{0xc0,0xf8,RN },{0x58,0xf8,RS }}},
{222,'a','I','E','E',O_ADD|O_BYTE,"add:q.b",2,{QIM,RNINC},2,	{{0xc0,0xf8,RN },{0x08,0xf8,QIM }}},
{162,'b','E','S','E',O_BNOT|O_BYTE,"bnot.b",2,{RS,RNINC},2,	{{0xc0,0xf8,RN },{0x68,0xf8,RS }}},
{216,'a','E','D','D',O_ADDX|O_BYTE,"addx.b",2,{RNINC,RD},2,	{{0xc0,0xf8,RN },{0xa0,0xf8,RD }}},
{161,'b','E','I','E',O_BNOT|O_WORD,"bnot.w",2,{IMM4,RNINC},2,	{{0xc8,0xf8,RN },{0xe0,0xf0,IMM4 }}},
{215,'a','E','D','D',O_ADDX|O_WORD,"addx.w",2,{RNINC,RD},2,	{{0xc8,0xf8,RN },{0xa0,0xf8,RD }}},
{161,'b','E','S','E',O_BNOT|O_WORD,"bnot.w",2,{RS,RNINC},2,	{{0xc8,0xf8,RN },{0x68,0xf8,RS }}},
{204,'b','E','I','E',O_BCLR|O_BYTE,"bclr.b",2,{IMM4,RNIND},2,	{{0xd0,0xf8,RN },{0xd0,0xf0,IMM4 }}},
{162,'b','E','I','E',O_BNOT|O_BYTE,"bnot.b",2,{IMM4,RNIND},2,	{{0xd0,0xf8,RN },{0xe0,0xf0,IMM4 }}},
{224,'a','E','D','D',O_ADD|O_WORD,"add:g.w",2,{RNINC,RD},2,	{{0xc8,0xf8,RN },{0x20,0xf8,RD }}},
{218,'-','E','D','D',O_ADDS|O_WORD,"adds.w",2,{RNINC,RD},2,	{{0xc8,0xf8,RN },{0x28,0xf8,RD }}},
{203,'b','E','S','E',O_BCLR|O_WORD,"bclr.w",2,{RS,RNINC},2,	{{0xc8,0xf8,RN },{0x58,0xf8,RS }}},
{212,'m','E','D','D',O_AND|O_WORD,"and.w",2,{RNINC,RD},2,	{{0xc8,0xf8,RN },{0x50,0xf8,RD }}},
{221,'a','I','E','E',O_ADD|O_WORD,"add:q.w",2,{QIM,RNINC},2,	{{0xc8,0xf8,RN },{0x08,0xf8,QIM }}},
{222,'a','I','E','E',O_ADD|O_BYTE,"add:q.b",2,{QIM,RNIND},2,	{{0xd0,0xf8,RN },{0x08,0xf8,QIM }}},
{225,'a','E','D','D',O_ADD|O_BYTE,"add:g.b",2,{RNIND,RD},2,	{{0xd0,0xf8,RN },{0x20,0xf8,RD }}},
{219,'-','E','D','D',O_ADDS|O_BYTE,"adds.b",2,{RNIND,RD},2,	{{0xd0,0xf8,RN },{0x28,0xf8,RD }}},
{213,'m','E','D','D',O_AND|O_BYTE,"and.b",2,{RNIND,RD},2,	{{0xd0,0xf8,RN },{0x50,0xf8,RD }}},
{204,'b','E','S','E',O_BCLR|O_BYTE,"bclr.b",2,{RS,RNIND},2,	{{0xd0,0xf8,RN },{0x58,0xf8,RS }}},
{162,'b','E','S','E',O_BNOT|O_BYTE,"bnot.b",2,{RS,RNIND},2,	{{0xd0,0xf8,RN },{0x68,0xf8,RS }}},
{216,'a','E','D','D',O_ADDX|O_BYTE,"addx.b",2,{RNIND,RD},2,	{{0xd0,0xf8,RN },{0xa0,0xf8,RD }}},
{203,'b','E','I','E',O_BCLR|O_WORD,"bclr.w",2,{IMM4,RNIND},2,	{{0xd8,0xf8,RN },{0xd0,0xf0,IMM4 }}},
{161,'b','E','I','E',O_BNOT|O_WORD,"bnot.w",2,{IMM4,RNIND},2,	{{0xd8,0xf8,RN },{0xe0,0xf0,IMM4 }}},
{221,'a','I','E','E',O_ADD|O_WORD,"add:q.w",2,{QIM,RNIND},2,	{{0xd8,0xf8,RN },{0x08,0xf8,QIM }}},
{224,'a','E','D','D',O_ADD|O_WORD,"add:g.w",2,{RNIND,RD},2,	{{0xd8,0xf8,RN },{0x20,0xf8,RD }}},
{218,'-','E','D','D',O_ADDS|O_WORD,"adds.w",2,{RNIND,RD},2,	{{0xd8,0xf8,RN },{0x28,0xf8,RD }}},
{212,'m','E','D','D',O_AND|O_WORD,"and.w",2,{RNIND,RD},2,	{{0xd8,0xf8,RN },{0x50,0xf8,RD }}},
{203,'b','E','S','E',O_BCLR|O_WORD,"bclr.w",2,{RS,RNIND},2,	{{0xd8,0xf8,RN },{0x58,0xf8,RS }}},
{161,'b','E','S','E',O_BNOT|O_WORD,"bnot.w",2,{RS,RNIND},2,	{{0xd8,0xf8,RN },{0x68,0xf8,RS }}},
{215,'a','E','D','D',O_ADDX|O_WORD,"addx.w",2,{RNIND,RD},2,	{{0xd8,0xf8,RN },{0xa0,0xf8,RD }}},
{204,'b','E','I','E',O_BCLR|O_BYTE,"bclr.b",2,{IMM4,RNIND_D8},3,	{{0xe0,0xf8,RN },{0x00,0x00,DISP8 },{0xd0,0xf0,IMM4 }}},
{162,'b','E','I','E',O_BNOT|O_BYTE,"bnot.b",2,{IMM4,RNIND_D8},3,	{{0xe0,0xf8,RN },{0x00,0x00,DISP8 },{0xe0,0xf0,IMM4 }}},
{222,'a','I','E','E',O_ADD|O_BYTE,"add:q.b",2,{QIM,RNIND_D8},3,	{{0xe0,0xf8,RN },{0x00,0x00,DISP8 },{0x08,0xf8,QIM }}},
{225,'a','E','D','D',O_ADD|O_BYTE,"add:g.b",2,{RNIND_D8,RD},3,	{{0xe0,0xf8,RN },{0x00,0x00,DISP8 },{0x20,0xf8,RD }}},
{219,'-','E','D','D',O_ADDS|O_BYTE,"adds.b",2,{RNIND_D8,RD},3,	{{0xe0,0xf8,RN },{0x00,0x00,DISP8 },{0x28,0xf8,RD }}},
{213,'m','E','D','D',O_AND|O_BYTE,"and.b",2,{RNIND_D8,RD},3,	{{0xe0,0xf8,RN },{0x00,0x00,DISP8 },{0x50,0xf8,RD }}},
{204,'b','E','S','E',O_BCLR|O_BYTE,"bclr.b",2,{RS,RNIND_D8},3,	{{0xe0,0xf8,RN },{0x00,0x00,DISP8 },{0x58,0xf8,RS }}},
{162,'b','E','S','E',O_BNOT|O_BYTE,"bnot.b",2,{RS,RNIND_D8},3,	{{0xe0,0xf8,RN },{0x00,0x00,DISP8 },{0x68,0xf8,RS }}},
{216,'a','E','D','D',O_ADDX|O_BYTE,"addx.b",2,{RNIND_D8,RD},3,	{{0xe0,0xf8,RN },{0x00,0x00,DISP8 },{0xa0,0xf8,RD }}},
{203,'b','E','I','E',O_BCLR|O_WORD,"bclr.w",2,{IMM4,RNIND_D8},3,	{{0xe8,0xf8,RN },{0x00,0x00,DISP8 },{0xd0,0xf0,IMM4 }}},
{161,'b','E','I','E',O_BNOT|O_WORD,"bnot.w",2,{IMM4,RNIND_D8},3,	{{0xe8,0xf8,RN },{0x00,0x00,DISP8 },{0xe0,0xf0,IMM4 }}},
{221,'a','I','E','E',O_ADD|O_WORD,"add:q.w",2,{QIM,RNIND_D8},3,	{{0xe8,0xf8,RN },{0x00,0x00,DISP8 },{0x08,0xf8,QIM }}},
{224,'a','E','D','D',O_ADD|O_WORD,"add:g.w",2,{RNIND_D8,RD},3,	{{0xe8,0xf8,RN },{0x00,0x00,DISP8 },{0x20,0xf8,RD }}},
{218,'-','E','D','D',O_ADDS|O_WORD,"adds.w",2,{RNIND_D8,RD},3,	{{0xe8,0xf8,RN },{0x00,0x00,DISP8 },{0x28,0xf8,RD }}},
{212,'m','E','D','D',O_AND|O_WORD,"and.w",2,{RNIND_D8,RD},3,	{{0xe8,0xf8,RN },{0x00,0x00,DISP8 },{0x50,0xf8,RD }}},
{203,'b','E','S','E',O_BCLR|O_WORD,"bclr.w",2,{RS,RNIND_D8},3,	{{0xe8,0xf8,RN },{0x00,0x00,DISP8 },{0x58,0xf8,RS }}},
{204,'b','E','I','E',O_BCLR|O_BYTE,"bclr.b",2,{IMM4,RNIND_D16},4,	{{0xf0,0xf8,RN },{0x00,0x00,DISP16 },{0x00,0x00,0},{0xd0,0xf0,IMM4 }}},
{222,'a','I','E','E',O_ADD|O_BYTE,"add:q.b",2,{QIM,RNIND_D16},4,	{{0xf0,0xf8,RN },{0x00,0x00,DISP16 },{0x00,0x00,0},{0x08,0xf8,QIM }}},
{204,'b','E','S','E',O_BCLR|O_BYTE,"bclr.b",2,{RS,RNIND_D16},4,	{{0xf0,0xf8,RN },{0x00,0x00,DISP16 },{0x00,0x00,0},{0x58,0xf8,RS }}},
{161,'b','E','S','E',O_BNOT|O_WORD,"bnot.w",2,{RS,RNIND_D8},3,	{{0xe8,0xf8,RN },{0x00,0x00,DISP8 },{0x68,0xf8,RS }}},
{216,'a','E','D','D',O_ADDX|O_BYTE,"addx.b",2,{RNIND_D16,RD},4,	{{0xf0,0xf8,RN },{0x00,0x00,DISP16 },{0x00,0x00,0},{0xa0,0xf8,RD }}},
{225,'a','E','D','D',O_ADD|O_BYTE,"add:g.b",2,{RNIND_D16,RD},4,	{{0xf0,0xf8,RN },{0x00,0x00,DISP16 },{0x00,0x00,0},{0x20,0xf8,RD }}},
{224,'a','E','D','D',O_ADD|O_WORD,"add:g.w",2,{RNIND_D16,RD},4,	{{0xf8,0xf8,RN },{0x00,0x00,DISP16 },{0x00,0x00,0},{0x20,0xf8,RD }}},
{215,'a','E','D','D',O_ADDX|O_WORD,"addx.w",2,{RNIND_D8,RD},3,	{{0xe8,0xf8,RN },{0x00,0x00,DISP8 },{0xa0,0xf8,RD }}},
{162,'b','E','I','E',O_BNOT|O_BYTE,"bnot.b",2,{IMM4,RNIND_D16},4,	{{0xf0,0xf8,RN },{0x00,0x00,DISP16 },{0x00,0x00,0},{0xe0,0xf0,IMM4 }}},
{219,'-','E','D','D',O_ADDS|O_BYTE,"adds.b",2,{RNIND_D16,RD},4,	{{0xf0,0xf8,RN },{0x00,0x00,DISP16 },{0x00,0x00,0},{0x28,0xf8,RD }}},
{213,'m','E','D','D',O_AND|O_BYTE,"and.b",2,{RNIND_D16,RD},4,	{{0xf0,0xf8,RN },{0x00,0x00,DISP16 },{0x00,0x00,0},{0x50,0xf8,RD }}},
{162,'b','E','S','E',O_BNOT|O_BYTE,"bnot.b",2,{RS,RNIND_D16},4,	{{0xf0,0xf8,RN },{0x00,0x00,DISP16 },{0x00,0x00,0},{0x68,0xf8,RS }}},
{203,'b','E','I','E',O_BCLR|O_WORD,"bclr.w",2,{IMM4,RNIND_D16},4,	{{0xf8,0xf8,RN },{0x00,0x00,DISP16 },{0x00,0x00,0},{0xd0,0xf0,IMM4 }}},
{161,'b','E','I','E',O_BNOT|O_WORD,"bnot.w",2,{IMM4,RNIND_D16},4,	{{0xf8,0xf8,RN },{0x00,0x00,DISP16 },{0x00,0x00,0},{0xe0,0xf0,IMM4 }}},
{221,'a','I','E','E',O_ADD|O_WORD,"add:q.w",2,{QIM,RNIND_D16},4,	{{0xf8,0xf8,RN },{0x00,0x00,DISP16 },{0x00,0x00,0},{0x08,0xf8,QIM }}},
{218,'-','E','D','D',O_ADDS|O_WORD,"adds.w",2,{RNIND_D16,RD},4,	{{0xf8,0xf8,RN },{0x00,0x00,DISP16 },{0x00,0x00,0},{0x28,0xf8,RD }}},
{212,'m','E','D','D',O_AND|O_WORD,"and.w",2,{RNIND_D16,RD},4,	{{0xf8,0xf8,RN },{0x00,0x00,DISP16 },{0x00,0x00,0},{0x50,0xf8,RD }}},
{203,'b','E','S','E',O_BCLR|O_WORD,"bclr.w",2,{RS,RNIND_D16},4,	{{0xf8,0xf8,RN },{0x00,0x00,DISP16 },{0x00,0x00,0},{0x58,0xf8,RS }}},
{161,'b','E','S','E',O_BNOT|O_WORD,"bnot.w",2,{RS,RNIND_D16},4,	{{0xf8,0xf8,RN },{0x00,0x00,DISP16 },{0x00,0x00,0},{0x68,0xf8,RS }}},
{215,'a','E','D','D',O_ADDX|O_WORD,"addx.w",2,{RNIND_D16,RD},4,	{{0xf8,0xf8,RN },{0x00,0x00,DISP16 },{0x00,0x00,0},{0xa0,0xf8,RD }}},
{225,'a','E','D','D',O_ADD|O_BYTE,"add:g.b",2,{IMM8,RD},3,	{{0x04,0xff,0},{0x00,0x00,IMM8 },{0x20,0xf8,RD }}},
{219,'-','E','D','D',O_ADDS|O_BYTE,"adds.b",2,{IMM8,RD},3,	{{0x04,0xff,0},{0x00,0x00,IMM8 },{0x28,0xf8,RD }}},
{213,'m','E','D','D',O_AND|O_BYTE,"and.b",2,{IMM8,RD},3,	{{0x04,0xff,0},{0x00,0x00,IMM8 },{0x50,0xf8,RD }}},
{210,'s','I','S','S',O_ANDC|O_BYTE,"andc.b",2,{IMM8,CRB},3,	{{0x04,0xff,0},{0x00,0x00,IMM8 },{0x58,0xf8,CRB }}},
{216,'a','E','D','D',O_ADDX|O_BYTE,"addx.b",2,{IMM8,RD},3,	{{0x04,0xff,0},{0x00,0x00,IMM8 },{0xa0,0xf8,RD }}},
{204,'b','E','I','E',O_BCLR|O_BYTE,"bclr.b",2,{IMM4,ABS8},3,	{{0x05,0xff,0},{0x00,0x00,ABS8 },{0xd0,0xf0,IMM4 }}},
{162,'b','E','I','E',O_BNOT|O_BYTE,"bnot.b",2,{IMM4,ABS8},3,	{{0x05,0xff,0},{0x00,0x00,ABS8 },{0xe0,0xf0,IMM4 }}},
{222,'a','I','E','E',O_ADD|O_BYTE,"add:q.b",2,{QIM,ABS8},3,	{{0x05,0xff,0},{0x00,0x00,ABS8 },{0x08,0xf8,QIM }}},
{225,'a','E','D','D',O_ADD|O_BYTE,"add:g.b",2,{ABS8,RD},3,	{{0x05,0xff,0},{0x00,0x00,ABS8 },{0x20,0xf8,RD }}},
{219,'-','E','D','D',O_ADDS|O_BYTE,"adds.b",2,{ABS8,RD},3,	{{0x05,0xff,0},{0x00,0x00,ABS8 },{0x28,0xf8,RD }}},
{213,'m','E','D','D',O_AND|O_BYTE,"and.b",2,{ABS8,RD},3,	{{0x05,0xff,0},{0x00,0x00,ABS8 },{0x50,0xf8,RD }}},
{204,'b','E','S','E',O_BCLR|O_BYTE,"bclr.b",2,{RS,ABS8},3,	{{0x05,0xff,0},{0x00,0x00,ABS8 },{0x58,0xf8,RS }}},
{162,'b','E','S','E',O_BNOT|O_BYTE,"bnot.b",2,{RS,ABS8},3,	{{0x05,0xff,0},{0x00,0x00,ABS8 },{0x68,0xf8,RS }}},
{216,'a','E','D','D',O_ADDX|O_BYTE,"addx.b",2,{ABS8,RD},3,	{{0x05,0xff,0},{0x00,0x00,ABS8 },{0xa0,0xf8,RD }}},
{224,'a','E','D','D',O_ADD|O_WORD,"add:g.w",2,{IMM16,RD},4,	{{0x0c,0xff,0},{0x00,0x00,IMM16 },{0x00,0x00,0},{0x20,0xf8,RD }}},
{218,'-','E','D','D',O_ADDS|O_WORD,"adds.w",2,{IMM16,RD},4,	{{0x0c,0xff,0},{0x00,0x00,IMM16 },{0x00,0x00,0},{0x28,0xf8,RD }}},
{212,'m','E','D','D',O_AND|O_WORD,"and.w",2,{IMM16,RD},4,	{{0x0c,0xff,0},{0x00,0x00,IMM16 },{0x00,0x00,0},{0x50,0xf8,RD }}},
{209,'s','I','S','S',O_ANDC|O_WORD,"andc.w",2,{IMM16,CRW},4,	{{0x0c,0xff,0},{0x00,0x00,IMM16 },{0x00,0x00,0},{0x58,0xf8,CRW }}},
{215,'a','E','D','D',O_ADDX|O_WORD,"addx.w",2,{IMM16,RD},4,	{{0x0c,0xff,0},{0x00,0x00,IMM16 },{0x00,0x00,0},{0xa0,0xf8,RD }}},
{161,'b','E','I','E',O_BNOT|O_WORD,"bnot.w",2,{IMM4,ABS8},3,	{{0x0d,0xff,0},{0x00,0x00,ABS8 },{0xe0,0xf0,IMM4 }}},
{221,'a','I','E','E',O_ADD|O_WORD,"add:q.w",2,{QIM,ABS8},3,	{{0x0d,0xff,0},{0x00,0x00,ABS8 },{0x08,0xf8,QIM }}},
{224,'a','E','D','D',O_ADD|O_WORD,"add:g.w",2,{ABS8,RD},3,	{{0x0d,0xff,0},{0x00,0x00,ABS8 },{0x20,0xf8,RD }}},
{218,'-','E','D','D',O_ADDS|O_WORD,"adds.w",2,{ABS8,RD},3,	{{0x0d,0xff,0},{0x00,0x00,ABS8 },{0x28,0xf8,RD }}},
{157,'-','!','!','!',O_BPT|O_UNSZ,"bpt",0,{0,0},1,	{{0x0b,0xff,0}}},
{203,'b','E','I','E',O_BCLR|O_WORD,"bclr.w",2,{IMM4,ABS8},3,	{{0x0d,0xff,0},{0x00,0x00,ABS8 },{0xd0,0xf0,IMM4 }}},
{212,'m','E','D','D',O_AND|O_WORD,"and.w",2,{ABS8,RD},3,	{{0x0d,0xff,0},{0x00,0x00,ABS8 },{0x50,0xf8,RD }}},
{203,'b','E','S','E',O_BCLR|O_WORD,"bclr.w",2,{RS,ABS8},3,	{{0x0d,0xff,0},{0x00,0x00,ABS8 },{0x58,0xf8,RS }}},
{161,'b','E','S','E',O_BNOT|O_WORD,"bnot.w",2,{RS,ABS8},3,	{{0x0d,0xff,0},{0x00,0x00,ABS8 },{0x68,0xf8,RS }}},
{215,'a','E','D','D',O_ADDX|O_WORD,"addx.w",2,{ABS8,RD},3,	{{0x0d,0xff,0},{0x00,0x00,ABS8 },{0xa0,0xf8,RD }}},
{204,'b','E','I','E',O_BCLR|O_BYTE,"bclr.b",2,{IMM4,ABS16},4,	{{0x15,0xff,0},{0x00,0x00,ABS16 },{0x00,0x00,0},{0xd0,0xf0,IMM4 }}},
{225,'a','E','D','D',O_ADD|O_BYTE,"add:g.b",2,{ABS16,RD},4,	{{0x15,0xff,0},{0x00,0x00,ABS16 },{0x00,0x00,0},{0x20,0xf8,RD }}},
{219,'-','E','D','D',O_ADDS|O_BYTE,"adds.b",2,{ABS16,RD},4,	{{0x15,0xff,0},{0x00,0x00,ABS16 },{0x00,0x00,0},{0x28,0xf8,RD }}},
{213,'m','E','D','D',O_AND|O_BYTE,"and.b",2,{ABS16,RD},4,	{{0x15,0xff,0},{0x00,0x00,ABS16 },{0x00,0x00,0},{0x50,0xf8,RD }}},
{204,'b','E','S','E',O_BCLR|O_BYTE,"bclr.b",2,{RS,ABS16},4,	{{0x15,0xff,0},{0x00,0x00,ABS16 },{0x00,0x00,0},{0x58,0xf8,RS }}},
{216,'a','E','D','D',O_ADDX|O_BYTE,"addx.b",2,{ABS16,RD},4,	{{0x15,0xff,0},{0x00,0x00,ABS16 },{0x00,0x00,0},{0xa0,0xf8,RD }}},
{203,'b','E','I','E',O_BCLR|O_WORD,"bclr.w",2,{IMM4,ABS16},4,	{{0x1d,0xff,0},{0x00,0x00,ABS16 },{0x00,0x00,0},{0xd0,0xf0,IMM4 }}},
{218,'-','E','D','D',O_ADDS|O_WORD,"adds.w",2,{ABS16,RD},4,	{{0x1d,0xff,0},{0x00,0x00,ABS16 },{0x00,0x00,0},{0x28,0xf8,RD }}},
{212,'m','E','D','D',O_AND|O_WORD,"and.w",2,{ABS16,RD},4,	{{0x1d,0xff,0},{0x00,0x00,ABS16 },{0x00,0x00,0},{0x50,0xf8,RD }}},
{203,'b','E','S','E',O_BCLR|O_WORD,"bclr.w",2,{RS,ABS16},4,	{{0x1d,0xff,0},{0x00,0x00,ABS16 },{0x00,0x00,0},{0x58,0xf8,RS }}},
{215,'a','E','D','D',O_ADDX|O_WORD,"addx.w",2,{ABS16,RD},4,	{{0x1d,0xff,0},{0x00,0x00,ABS16 },{0x00,0x00,0},{0xa0,0xf8,RD }}},
{155,'-','B','!','!',O_BRA|O_BYTE,"bra.b",1,{PCREL8,0},2,	{{0x20,0xff,0},{0x00,0x00,PCREL8 }}},
{162,'b','E','I','E',O_BNOT|O_BYTE,"bnot.b",2,{IMM4,ABS16},4,	{{0x15,0xff,0},{0x00,0x00,ABS16 },{0x00,0x00,0},{0xe0,0xf0,IMM4 }}},
{222,'a','I','E','E',O_ADD|O_BYTE,"add:q.b",2,{QIM,ABS16},4,	{{0x15,0xff,0},{0x00,0x00,ABS16 },{0x00,0x00,0},{0x08,0xf8,QIM }}},
{162,'b','E','S','E',O_BNOT|O_BYTE,"bnot.b",2,{RS,ABS16},4,	{{0x15,0xff,0},{0x00,0x00,ABS16 },{0x00,0x00,0},{0x68,0xf8,RS }}},
{161,'b','E','I','E',O_BNOT|O_WORD,"bnot.w",2,{IMM4,ABS16},4,	{{0x1d,0xff,0},{0x00,0x00,ABS16 },{0x00,0x00,0},{0xe0,0xf0,IMM4 }}},
{221,'a','I','E','E',O_ADD|O_WORD,"add:q.w",2,{QIM,ABS16},4,	{{0x1d,0xff,0},{0x00,0x00,ABS16 },{0x00,0x00,0},{0x08,0xf8,QIM }}},
{224,'a','E','D','D',O_ADD|O_WORD,"add:g.w",2,{ABS16,RD},4,	{{0x1d,0xff,0},{0x00,0x00,ABS16 },{0x00,0x00,0},{0x20,0xf8,RD }}},
{161,'b','E','S','E',O_BNOT|O_WORD,"bnot.w",2,{RS,ABS16},4,	{{0x1d,0xff,0},{0x00,0x00,ABS16 },{0x00,0x00,0},{0x68,0xf8,RS }}},
{152,'-','B','!','!',O_BRN|O_BYTE,"brn.b",1,{PCREL8,0},2,	{{0x21,0xff,0},{0x00,0x00,PCREL8 }}},
{186,'-','B','!','!',O_BHI|O_BYTE,"bhi.b",1,{PCREL8,0},2,	{{0x22,0xff,0},{0x00,0x00,PCREL8 }}},
{174,'-','B','!','!',O_BLS|O_BYTE,"bls.b",1,{PCREL8,0},2,	{{0x23,0xff,0},{0x00,0x00,PCREL8 }}},
{207,'-','B','!','!',O_BCC|O_BYTE,"bcc.b",1,{PCREL8,0},2,	{{0x24,0xff,0},{0x00,0x00,PCREL8 }}},
{201,'-','B','!','!',O_BCS|O_BYTE,"bcs.b",1,{PCREL8,0},2,	{{0x25,0xff,0},{0x00,0x00,PCREL8 }}},
{165,'-','B','!','!',O_BNE|O_BYTE,"bne.b",1,{PCREL8,0},2,	{{0x26,0xff,0},{0x00,0x00,PCREL8 }}},
{198,'-','B','!','!',O_BEQ|O_BYTE,"beq.b",1,{PCREL8,0},2,	{{0x27,0xff,0},{0x00,0x00,PCREL8 }}},
{159,'-','B','!','!',O_BPL|O_BYTE,"bpl.b",1,{PCREL8,0},2,	{{0x2a,0xff,0},{0x00,0x00,PCREL8 }}},
{168,'-','B','!','!',O_BMI|O_BYTE,"bmi.b",1,{PCREL8,0},2,	{{0x2b,0xff,0},{0x00,0x00,PCREL8 }}},
{192,'-','B','!','!',O_BGE|O_BYTE,"bge.b",1,{PCREL8,0},2,	{{0x2c,0xff,0},{0x00,0x00,PCREL8 }}},
{171,'-','B','!','!',O_BLT|O_BYTE,"blt.b",1,{PCREL8,0},2,	{{0x2d,0xff,0},{0x00,0x00,PCREL8 }}},
{189,'-','B','!','!',O_BGT|O_BYTE,"bgt.b",1,{PCREL8,0},2,	{{0x2e,0xff,0},{0x00,0x00,PCREL8 }}},
{180,'-','B','!','!',O_BLE|O_BYTE,"ble.b",1,{PCREL8,0},2,	{{0x2f,0xff,0},{0x00,0x00,PCREL8 }}},
{154,'-','B','!','!',O_BRA|O_WORD,"bra.w",1,{PCREL16,0},3,	{{0x30,0xff,0},{0x00,0x00,PCREL16 },{0x00,0x00,0}}},
{151,'-','B','!','!',O_BRN|O_WORD,"brn.w",1,{PCREL16,0},3,	{{0x31,0xff,0},{0x00,0x00,PCREL16 },{0x00,0x00,0}}},
{185,'-','B','!','!',O_BHI|O_WORD,"bhi.w",1,{PCREL16,0},3,	{{0x32,0xff,0},{0x00,0x00,PCREL16 },{0x00,0x00,0}}},
{173,'-','B','!','!',O_BLS|O_WORD,"bls.w",1,{PCREL16,0},3,	{{0x33,0xff,0},{0x00,0x00,PCREL16 },{0x00,0x00,0}}},
{206,'-','B','!','!',O_BCC|O_WORD,"bcc.w",1,{PCREL16,0},3,	{{0x34,0xff,0},{0x00,0x00,PCREL16 },{0x00,0x00,0}}},
{200,'-','B','!','!',O_BCS|O_WORD,"bcs.w",1,{PCREL16,0},3,	{{0x35,0xff,0},{0x00,0x00,PCREL16 },{0x00,0x00,0}}},
{164,'-','B','!','!',O_BNE|O_WORD,"bne.w",1,{PCREL16,0},3,	{{0x36,0xff,0},{0x00,0x00,PCREL16 },{0x00,0x00,0}}},
{197,'-','B','!','!',O_BEQ|O_WORD,"beq.w",1,{PCREL16,0},3,	{{0x37,0xff,0},{0x00,0x00,PCREL16 },{0x00,0x00,0}}},
{158,'-','B','!','!',O_BPL|O_WORD,"bpl.w",1,{PCREL16,0},3,	{{0x3a,0xff,0},{0x00,0x00,PCREL16 },{0x00,0x00,0}}},
{167,'-','B','!','!',O_BMI|O_WORD,"bmi.w",1,{PCREL16,0},3,	{{0x3b,0xff,0},{0x00,0x00,PCREL16 },{0x00,0x00,0}}},
{191,'-','B','!','!',O_BGE|O_WORD,"bge.w",1,{PCREL16,0},3,	{{0x3c,0xff,0},{0x00,0x00,PCREL16 },{0x00,0x00,0}}},
{170,'-','B','!','!',O_BLT|O_WORD,"blt.w",1,{PCREL16,0},3,	{{0x3d,0xff,0},{0x00,0x00,PCREL16 },{0x00,0x00,0}}},
{188,'-','B','!','!',O_BGT|O_WORD,"bgt.w",1,{PCREL16,0},3,	{{0x3e,0xff,0},{0x00,0x00,PCREL16 },{0x00,0x00,0}}},
{179,'-','B','!','!',O_BLE|O_WORD,"ble.w",1,{PCREL16,0},3,	{{0x3f,0xff,0},{0x00,0x00,PCREL16 },{0x00,0x00,0}}},
/*
RN,RD  'm','E','D','D'
CRB,RN  's','C','!','E'
RN,RD  'm','E','D','D'
RNDEC,RD  'm','E','D','D'
CRB,RNDEC  's','C','!','E'
RNDEC,RD  'm','E','D','D'
RNINC,RD  'm','E','D','D'
CRB,RNINC  's','C','!','E'
RNINC,RD  'm','E','D','D'
RNIND,RD  'm','E','D','D'
CRB,RNIND  's','C','!','E'
RNIND,RD  'm','E','D','D'
RNIND_D8,RD  'a','E','D','D'
RNIND_D8,RD  'm','E','D','D'
CRB,RNIND_D8  's','C','!','E'
RNIND_D8,RD  'm','E','D','D'
RNIND_D16,RD  'a','E','D','D'
RNIND_D16,RD  'm','E','D','D'
CRB,RNIND_D16  's','C','!','E'
RNIND_D16,RD  'm','E','D','D'
RN,RD  'm','E','D','D'
RNDEC,RD  'm','E','D','D'
RNIND,RD  'm','E','D','D'
RNINC,RD  'm','E','D','D'
RNIND_D8,RD  'm','E','D','D'
ABS8,RD  'm','E','D','D'
IMM16,RD  'm','E','D','D'
ABS16,RD  'm','E','D','D'
RNIND_D16,RD  'm','E','D','D'
RN,RD  'a','E','D','D'
RS,RD  '-','X','!','!'
RN,0  'a','E','!','!'
RS,RD  '-','X','!','!'
RN,0  'a','E','!','!'
RNDEC,RD  'a','E','D','D'
RNDEC,0  'a','E','!','!'
RNDEC,RD  'a','E','D','D'
RNDEC,0  'a','E','!','!'
RNINC,RD  'a','E','D','D'
RNINC,0  'a','E','!','!'
RNINC,0  'a','E','!','!'
RNIND,RD  'a','E','D','D'
RNIND,0  'a','E','!','!'
RNIND,RD  'a','E','D','D'
RNIND,0  'a','E','!','!'
RNIND_D8,0  'a','E','!','!'
RNIND_D8,RD  'a','E','D','D'
RNIND_D8,0  'a','E','!','!'
RNIND_D16,0  'a','E','!','!'
RNIND_D16,RD  'a','E','D','D'
RN,0  'a','E','!','!'
RNIND,0  'a','E','!','!'
RNDEC,0  'a','E','!','!'
RNINC,0  'a','E','!','!'
ABS8,0  'a','E','!','!'
RNIND_D8,0  'a','E','!','!'
RD,0  'm','D','!','D'
ABS16,0  'a','E','!','!'
RNIND_D16,0  'a','E','!','!'
RN,0  's','E','!','E'
RN,RD  'a','E','D','D'
RN,RD  'a','E','D','D'
RNDEC,0  's','E','!','E'
RNDEC,RD  'a','E','D','D'
RNINC,0  's','E','!','E'
RNINC,RD  'a','E','D','D'
RNIND,RD  '-','E','D','D'
RNIND,0  's','E','!','E'
RNIND,RD  'a','E','D','D'
RNIND_D8,0  's','E','!','E'
RN,0  's','E','!','E'
RNIND,0  's','E','!','E'
RNINC,0  's','E','!','E'
RNDEC,0  's','E','!','E'
IMM8,0  's','E','!','E'
ABS8,0  's','E','!','E'
RNIND_D8,0  's','E','!','E'
ABS16,0  's','E','!','E'
RNIND_D16,0  's','E','!','E'
RNIND_D8,RD  '-','E','D','D'
RD,0  'm','D','!','D'
RNIND_D16,RD  '-','E','D','D'
RNIND_D16,0  's','E','!','E'
IMM16,0  'a','E','!','!'
RN,RD  '-','E','D','D'
RN,RD  'a','E','D','D'
RN,RD  '-','E','D','D'
RNDEC,RD  '-','E','D','D'
RNDEC,RD  'a','E','D','D'
RNDEC,RD  '-','E','D','D'
RNINC,RD  '-','E','D','D'
RNINC,RD  'a','E','D','D'
RNINC,RD  '-','E','D','D'
RNINC,RD  'a','E','D','D'
RNIND,RD  'a','E','D','D'
RNIND_D8,RD  'a','E','D','D'
RNIND_D8,RD  '-','E','D','D'
RNIND_D16,RD  'a','E','D','D'
RNIND_D16,RD  '-','E','D','D'
RN,RD  'a','E','D','D'
RNDEC,RD  'a','E','D','D'
RNINC,RD  'a','E','D','D'
RNIND,RD  'a','E','D','D'
ABS8,RD  'a','E','D','D'
RNIND_D8,RD  'a','E','D','D'
IMM16,RD  'a','E','D','D'
ABS16,RD  'a','E','D','D'
RNIND_D16,RD  'a','E','D','D'
RNIND,RD  '-','E','D','D'
RNIND_D8,RD  'a','E','D','D'
RNIND_D16,RD  'a','E','D','D'
RNIND_D16,0  'a','E','!','!'
IMM8,RD  'a','E','D','D'
IMM8,RD  '-','E','D','D'
IMM8,RD  'm','E','D','D'
IMM8,CRB  's','E','C','C'
IMM8,RD  'a','E','D','D'
IMM8,0  'a','E','!','!'
IMM8,0  's','E','!','E'
ABS8,RD  'a','E','D','D'
ABS8,RD  '-','E','D','D'
ABS8,RD  'm','E','D','D'
CRB,ABS8  's','C','!','E'
ABS8,RD  'a','E','D','D'
ABS8,0  'a','E','!','!'
ABS8,0  's','E','!','E'
RN,RD  '-','E','D','D'
RNDEC,RD  '-','E','D','D'
RNIND,RD  '-','E','D','D'
RNINC,RD  '-','E','D','D'
ABS8,RD  '-','E','D','D'
RNIND_D8,RD  '-','E','D','D'
ABS16,RD  '-','E','D','D'
IMM16,RD  '-','E','D','D'
RNIND_D16,RD  '-','E','D','D'
IMM4,0  '-','I','!','!'
0,0  '-','B','!','!'
IMM16,RD  'a','E','D','D'
IMM16,RD  '-','E','D','D'
IMM16,RD  'm','E','D','D'
IMM16,CRW  's','E','C','C'
IMM16,RD  'a','E','D','D'
IMM16,0  'a','E','!','!'
ABS8,RD  'a','E','D','D'
ABS8,RD  '-','E','D','D'
ABS8,RD  'm','E','D','D'
ABS8,RD  'a','E','D','D'
ABS8,0  'a','E','!','!'
FP,0  '-','B','!','!'
ABS16,RD  'a','E','D','D'
ABS16,RD  '-','E','D','D'
ABS16,RD  'm','E','D','D'
CRB,ABS16  's','C','!','E'
RN,RD  'a','E','D','D'
RNIND,RD  'a','E','D','D'
RNINC,RD  'a','E','D','D'
RNDEC,RD  'a','E','D','D'
ABS8,RD  'a','E','D','D'
RNIND_D8,RD  'a','E','D','D'
IMM16,RD  'a','E','D','D'
ABS16,RD  'a','E','D','D'
RNIND_D16,RD  'a','E','D','D'
RLIST,SPDEC  '-','I','!','E'
CRW,RN  's','C','!','E'
CRW,RNDEC  's','C','!','E'
CRW,RNINC  's','C','!','E'
CRW,RNIND  's','C','!','E'
CRW,ABS8  's','C','!','E'
CRW,RNIND_D8  's','C','!','E'
CRW,ABS16  's','C','!','E'
CRW,RNIND_D16  's','C','!','E'
ABS16,RD  'a','E','D','D'
ABS16,0  'a','E','!','!'
ABS16,0  's','E','!','E'
ABS16,RD  'a','E','D','D'
ABS16,RD  '-','E','D','D'
ABS16,RD  'm','E','D','D'
ABS16,RD  'a','E','D','D'
ABS16,0  'a','E','!','!'
CRW,RN  's','C','!','E'
RNIND,0  'h','E','!','E'
CRB,RNDEC  's','C','!','E'
CRW,RNIND  's','C','!','E'
CRW,RNINC  's','C','!','E'
CRW,RNDEC  's','C','!','E'
CRB,RNIND  's','C','!','E'
CRB,RNINC  's','C','!','E'
CRW,RNIND_D8  's','C','!','E'
CRB,ABS8  's','C','!','E'
CRB,RNIND_D8  's','C','!','E'
CRW,ABS8  's','C','!','E'
CRW,RNIND_D16  's','C','!','E'
RNIND,0  'h','E','!','E'
CRB,ABS16  's','C','!','E'
CRB,RNIND_D16  's','C','!','E'
RN,0  'h','E','!','E'
RNINC,0  'h','E','!','E'
CRW,ABS16  's','C','!','E'
RN,0  'h','E','!','E'
RNDEC,0  'h','E','!','E'
RNDEC,0  'h','E','!','E'
RNINC,0  'h','E','!','E'
RNINC,0  'h','E','!','E'
CRB,RN  's','C','!','E'
RN,0  'h','E','!','E'
RN,0  'h','E','!','E'
RN,0  'h','E','!','E'
RNDEC,0  'h','E','!','E'
RNDEC,0  'h','E','!','E'
RNDEC,0  'h','E','!','E'
RNINC,0  'h','E','!','E'
RNINC,0  'h','E','!','E'
RNIND,0  'h','E','!','E'
RNIND,0  'h','E','!','E'
RN,0  'h','E','!','E'
RNIND,0  'h','E','!','E'
RNDEC,0  'h','E','!','E'
RNINC,0  'h','E','!','E'
ABS8,0  'h','E','!','E'
RNIND_D8,0  'h','E','!','E'
ABS16,0  'h','E','!','E'
IMM16,0  'h','E','!','E'
RNIND_D16,0  'h','E','!','E'
RN,0  'h','E','!','E'
RNDEC,0  'h','E','!','E'
RNINC,0  'h','E','!','E'
RNIND,0  'h','E','!','E'
RNIND_D8,0  'h','E','!','E'
RNIND_D8,0  'h','E','!','E'
RNIND_D8,0  'h','E','!','E'
RNIND_D16,0  'h','E','!','E'
RNIND_D16,0  'h','E','!','E'
RNIND_D16,0  'h','E','!','E'
IMM8,0  'h','E','!','E'
IMM8,0  'h','E','!','E'
ABS8,0  'h','E','!','E'
IMM16,0  'h','E','!','E'
ABS8,0  'h','E','!','E'
ABS16,0  'h','E','!','E'
ABS16,0  'h','E','!','E'
ABS16,0  'h','E','!','E'
RN,0  'h','E','!','E'
RNDEC,0  'h','E','!','E'
RNIND,0  'h','E','!','E'
RNINC,0  'h','E','!','E'
ABS8,0  'h','E','!','E'
RNIND_D8,0  'h','E','!','E'
IMM16,0  'h','E','!','E'
ABS16,0  'h','E','!','E'
RNIND_D16,0  'h','E','!','E'
RNIND,0  'h','E','!','E'
RNIND_D8,0  'h','E','!','E'
RNIND_D8,0  'h','E','!','E'
RNIND_D8,0  'h','E','!','E'
RNIND_D16,0  'h','E','!','E'
RNIND_D16,0  'h','E','!','E'
RNIND_D16,0  'h','E','!','E'
IMM8,0  'h','E','!','E'
ABS8,0  'h','E','!','E'
ABS8,0  'h','E','!','E'
IMM16,0  'h','E','!','E'
IMM16,0  'h','E','!','E'
ABS8,0  'h','E','!','E'
ABS8,0  'h','E','!','E'
ABS16,0  'h','E','!','E'
0,0  '-','!','!','!'
ABS16,0  'h','E','!','E'
ABS16,0  'h','E','!','E'
RN,0  'h','E','!','E'
RNDEC,0  'h','E','!','E'
RNIND,0  'h','E','!','E'
RNINC,0  'h','E','!','E'
ABS8,0  'h','E','!','E'
RNIND_D8,0  'h','E','!','E'
IMM16,0  'h','E','!','E'
ABS16,0  'h','E','!','E'
RNIND_D16,0  'h','E','!','E'
RN,0  'h','E','!','E'
RN,0  'h','E','!','E'
RNDEC,0  'h','E','!','E'
RNDEC,0  'h','E','!','E'
RNDEC,0  'h','E','!','E'
RNINC,0  'h','E','!','E'
RNINC,0  'h','E','!','E'
RNIND,0  'h','E','!','E'
RNIND,0  'h','E','!','E'
RNIND_D8,0  'h','E','!','E'
RNIND_D8,0  'h','E','!','E'
RNIND_D8,0  'h','E','!','E'
RNIND_D8,0  'h','E','!','E'
RNIND_D16,0  'h','E','!','E'
RNIND_D16,0  'h','E','!','E'
RNIND_D16,0  'h','E','!','E'
RNIND_D16,0  'h','E','!','E'
IMM8,0  'h','E','!','E'
RN,0  'h','E','!','E'
RNIND,0  'h','E','!','E'
RNDEC,0  'h','E','!','E'
RNDEC,0  'h','E','!','E'
ABS8,0  'h','E','!','E'
RNIND_D8,0  'h','E','!','E'
ABS16,0  'h','E','!','E'
IMM16,0  'h','E','!','E'
RNIND_D16,0  'h','E','!','E'
RNINC,0  'h','E','!','E'
RN,0  'h','E','!','E'
RN,0  'h','E','!','E'
RN,0  'h','E','!','E'
RN,0  'h','E','!','E'
RNDEC,0  'h','E','!','E'
RNDEC,0  'h','E','!','E'
RNINC,0  'h','E','!','E'
RNINC,0  'h','E','!','E'
RNINC,0  'h','E','!','E'
RNINC,0  'h','E','!','E'
RNIND,0  'h','E','!','E'
RNIND,0  'h','E','!','E'
RNIND,0  'h','E','!','E'
RNIND,0  'h','E','!','E'
RNIND_D8,0  'h','E','!','E'
RNIND_D8,0  'h','E','!','E'
RNIND_D16,0  'h','E','!','E'
RNIND_D16,0  'h','E','!','E'
RS,PCREL8  '-','B','S','S'
IMM8,0  'h','E','!','E'
IMM8,0  'h','E','!','E'
ABS8,0  'h','E','!','E'
IMM16,0  'h','E','!','E'
RN,0  'h','E','!','E'
RNDEC,0  'h','E','!','E'
RNINC,0  'h','E','!','E'
RNIND,0  'h','E','!','E'
ABS8,0  'h','E','!','E'
RNIND_D8,0  'h','E','!','E'
IMM16,0  'h','E','!','E'
ABS16,0  'h','E','!','E'
RNIND_D16,0  'h','E','!','E'
ABS8,0  'h','E','!','E'
ABS8,0  'h','E','!','E'
RS,PCREL8  '-','B','S','S'
RS,PCREL8  '-','B','S','S'
IMM16,0  'h','E','!','E'
IMM16,0  'h','E','!','E'
ABS8,0  'h','E','!','E'
ABS8,0  'h','E','!','E'
ABS8,0  'h','E','!','E'
IMM16,0  '-','B','!','!'
IMM8,0  '-','B','!','!'
ABS16,0  'h','E','!','E'
ABS16,0  'h','E','!','E'
ABS16,0  'h','E','!','E'
0,0  '-','B','!','!'
ABS16,0  'h','E','!','E'
ABS16,0  'h','E','!','E'
ABS16,0  'h','E','!','E'
RN,0  'h','E','!','E'
RNIND,0  'h','E','!','E'
RNINC,0  'h','E','!','E'
RNDEC,0  'h','E','!','E'
ABS8,0  'h','E','!','E'
IMM8,RD  'm','I','!','D'
ABS16,0  'h','E','!','E'
IMM16,0  'h','E','!','E'
RNIND_D16,0  'h','E','!','E'
FPIND_D8,RD  'm','E','!','D'
FPIND_D8,RD  'm','E','!','D'
RS,FPIND_D8  'm','S','!','E'
RS,FPIND_D8  'm','S','!','E'
RN,RD  'm','E','!','D'
RN,0  'h','E','!','E'
RS,RNDEC  'm','S','!','E'
RNIND_D8,0  'h','E','!','E'
IMM16,RD  'm','I','!','D'
ABS8,RD  'm','E','!','D'
ABS8,RD  'm','E','!','D'
RS,ABS8  'm','S','!','E'
RS,ABS8  'm','S','!','E'
RN,RD  '-','E','!','D'
RN,0  'h','E','!','E'
RN,RD  'm','E','!','D'
RNDEC,RD  'm','E','!','D'
RNDEC,RD  '-','E','!','D'
RN,0  'h','E','!','E'
RNDEC,RD  'p','E','D','D'
RNIND,0  'h','E','!','E'
RNINC,0  'h','E','!','E'
ABS8,0  'h','E','!','E'
RNIND_D8,0  'h','E','!','E'
ABS16,0  'h','E','!','E'
IMM16,0  'h','E','!','E'
RNIND_D16,0  'h','E','!','E'
RS,RN  '-','S','!','E'
RN,0  'h','E','!','E'
RNDEC,0  'h','E','!','E'
RN,RD  'm','E','D','D'
RN,RD  'p','E','D','D'
RN,0  'a','E','!','E'
RN,0  'm','E','!','E'
RN,0  'h','E','!','E'
RN,RD  'm','E','D','D'
RN,RD  'p','E','D','D'
RN,0  'a','E','!','E'
RN,0  'm','E','!','E'
RNDEC,RD  'm','E','D','D'
RS,RNDEC  '-','S','!','E'
RNDEC,0  'h','E','!','E'
RNDEC,0  'a','E','!','E'
RNINC,RD  'm','E','D','D'
RNINC,0  'h','E','!','E'
RN,0  'h','E','!','E'
RNDEC,0  'h','E','!','E'
RNIND,0  'h','E','!','E'
RNINC,0  'h','E','!','E'
RNIND_D8,0  'h','E','!','E'
ABS8,0  'h','E','!','E'
IMM16,0  'h','E','!','E'
ABS16,0  'h','E','!','E'
RNIND_D16,0  'h','E','!','E'
RNDEC,0  'a','E','!','E'
RNDEC,0  'm','E','!','E'
RNDEC,RD  'm','E','D','D'
RNDEC,0  'm','E','!','E'
RNDEC,0  'h','E','!','E'
RNINC,0  'a','E','!','E'
RNINC,0  'm','E','!','E'
RNINC,RD  'm','E','D','D'
RNINC,0  'a','E','!','E'
IMM8,CRB  's','I','C','C'
IMM16,CRW  's','I','C','C'
RNINC,0  'm','E','!','E'
RNINC,0  'h','E','!','E'
RNIND,RD  'm','E','D','D'
RNIND,0  'a','E','!','E'
RNIND,0  'm','E','!','E'
RNIND,0  'h','E','!','E'
RNIND,RD  'm','E','D','D'
RNIND,0  'a','E','!','E'
RNIND,0  'm','E','!','E'
RNIND,0  'h','E','!','E'
RNIND_D8,RD  'm','E','D','D'
RNIND_D8,0  'a','E','!','E'
RNIND_D8,0  'm','E','!','E'
RNIND_D8,0  'h','E','!','E'
RNIND_D8,RD  'm','E','D','D'
RNIND_D8,0  'a','E','!','E'
RNIND_D8,0  'm','E','!','E'
RNIND_D8,0  'h','E','!','E'
RNIND_D16,RD  'm','E','D','D'
RNIND,RD  'm','E','D','D'
RNDEC,RD  'm','E','D','D'
RNINC,RD  'm','E','D','D'
ABS8,RD  'm','E','D','D'
RNIND_D8,RD  'm','E','D','D'
ABS16,RD  'm','E','D','D'
IMM16,RD  'm','E','D','D'
RNIND_D16,RD  'm','E','D','D'
RNIND_D16,RD  'm','E','D','D'
RNIND_D16,0  'a','E','!','E'
RNIND_D16,0  'm','E','!','E'
RNIND_D16,0  'h','E','!','E'
ABS24,0  '-','J','!','!'
IMM8,RD  'm','E','D','D'
IMM8,CRB  's','I','C','C'
IMM8,0  'a','E','!','E'
IMM8,0  'h','E','!','E'
ABS8,RD  'm','E','D','D'
RN,RD  'm','E','D','D'
RNIND_D16,RD  'm','E','!','D'
RNIND_D16,0  'a','E','!','E'
RNIND_D16,0  'm','E','!','E'
RNIND_D16,0  'h','E','!','E'
IMM8,0  'm','E','!','E'
ABS8,0  'a','E','!','E'
ABS8,0  'm','E','!','E'
RN,0  'm','E','!','E'
RNIND,0  'm','E','!','E'
RNDEC,0  'm','E','!','E'
RNINC,0  'm','E','!','E'
ABS8,0  'm','E','!','E'
RNIND_D8,0  'm','E','!','E'
ABS16,0  'm','E','!','E'
IMM16,0  'm','E','!','E'
RNIND_D16,0  'm','E','!','E'
IMM8,RNIND_D16  'm','I','!','E'
0,0  '-','!','!','!'
ABS8,0  'h','E','!','E'
IMM16,RD  'm','E','D','D'
IMM16,CRW  's','I','C','C'
IMM16,0  'a','E','!','E'
IMM16,0  'm','E','!','E'
IMM16,0  'h','E','!','E'
ABS8,RD  'm','E','D','D'
ABS8,0  'a','E','!','E'
ABS8,0  'm','E','!','E'
ABS8,0  'h','E','!','E'
RDIND,0  '-','J','!','!'
RDIND,0  '-','J','!','!'
IMM8,0  '-','B','!','!'
0,0  '-','B','!','!'
IMM16,0  '-','B','!','!'
ABS24,0  '-','J','!','!'
ABS16,RD  'm','E','D','D'
RN,0  'a','E','!','E'
RNDEC,0  'a','E','!','E'
RNINC,0  'a','E','!','E'
RNIND,0  'a','E','!','E'
ABS8,0  'a','E','!','E'
RNIND_D8,0  'a','E','!','E'
ABS16,0  'a','E','!','E'
IMM16,0  'a','E','!','E'
RNIND_D16,0  'a','E','!','E'
RNDEC,RD  'p','E','D','D'
RNDEC,0  'h','E','!','E'
RNINC,RD  'p','E','D','D'
RNINC,RD  'p','E','D','D'
RNIND,RD  'p','E','D','D'
RNIND,RD  'p','E','D','D'
RNIND_D8,RD  'p','E','D','D'
RNIND_D8,RD  'p','E','D','D'
RNIND_D8,0  'h','E','!','E'
RNIND_D16,RD  'p','E','D','D'
RNIND_D16,RD  'p','E','D','D'
IMM8,RD  'p','E','D','D'
ABS8,RD  'p','E','D','D'
IMM16,RD  'p','E','D','D'
ABS8,RD  'p','E','D','D'
ABS8,0  'h','E','!','E'
RS,ABS16  'm','S','!','E'
ABS16,RD  'p','E','D','D'
RN,RD  'p','E','D','D'
RNIND,RD  'p','E','D','D'
RNDEC,RD  'p','E','D','D'
RNINC,RD  'p','E','D','D'
RNIND_D8,RD  'p','E','D','D'
ABS8,RD  'p','E','D','D'
IMM16,RD  'p','E','D','D'
ABS16,RD  'p','E','D','D'
RNIND_D16,RD  'p','E','D','D'
RS,RNINC  '-','S','!','E'
RNINC,0  'h','E','!','E'
RS,RNIND  '-','S','!','E'
RNIND,0  'h','E','!','E'
RS,RNIND_D8  '-','S','!','E'
RS,RNIND_D16  'm','S','!','E'
RS,RNIND_D16  '-','S','!','E'
RS,ABS8  '-','S','!','E'
RS,RN  '-','S','!','E'
RS,RNDEC  '-','S','!','E'
RS,RNIND  '-','S','!','E'
RS,RNINC  '-','S','!','E'
RS,ABS8  '-','S','!','E'
RNIND,RD  '-','E','!','D'
RS,ABS16  '-','S','!','E'
RS,RNIND_D16  '-','S','!','E'
RNINC,RD  '-','E','!','D'
RNIND_D16,0  'h','E','!','E'
ABS16,RD  'm','E','!','D'
RS,RNIND_D8  '-','S','!','E'
RNIND_D8,RD  '-','E','!','D'
IMM8,RNIND_D8  'm','I','!','E'
RNIND_D16,RD  '-','E','!','D'
IMM8,RD  '-','E','!','D'
ABS8,RD  '-','E','!','D'
RN,RD  '-','E','!','D'
RNINC,0  'h','E','!','E'
RNIND,RD  '-','E','!','D'
RNDEC,RD  '-','E','!','D'
IMM8,RD  '-','E','!','D'
ABS8,RD  '-','E','!','D'
RNIND_D8,RD  '-','E','!','D'
ABS16,RD  '-','E','!','D'
RNIND_D16,RD  '-','E','!','D'
RNIND,0  'h','E','!','E'
RNIND_D8,0  'h','E','!','E'
RS,ABS8  'm','S','!','E'
RNDEC,RD  'm','E','!','D'
RNINC,RD  'm','E','!','D'
ABS8,RD  'm','E','!','D'
RNDEC,0  'h','E','!','E'
IMM16,RD  'm','I','!','D'
RS,RNDEC  'm','S','!','E'
RS,RNINC  'm','S','!','E'
RNIND,RD  'm','E','!','D'
RS,RNIND  'm','S','!','E'
RNIND_D8,RD  'm','E','!','D'
RS,RNIND_D8  'm','S','!','E'
IMM8,0  'h','E','!','E'
ABS8,0  'h','E','!','E'
RS,ABS8  'm','S','!','E'
RNINC,RD  '-','E','!','D'
IMM8,RNDEC  'm','I','!','E'
IMM16,RNDEC  'm','I','!','E'
RNINC,RD  'm','E','!','D'
RS,RNINC  'm','S','!','E'
IMM8,RNINC  'm','I','!','E'
IMM16,RNINC  'm','I','!','E'
RNIND,RD  'm','E','!','D'
RS,RNIND  'm','S','!','E'
IMM8,RNIND  'm','I','!','E'
IMM16,RNIND  'm','I','!','E'
RNIND_D8,RD  'm','E','!','D'
RS,RNIND_D8  'm','S','!','E'
IMM16,RNIND_D8  'm','I','!','E'
IMM16,RNIND_D16  'm','I','!','E'
RNIND_D16,RD  'm','E','!','D'
RS,RNIND_D16  'm','S','!','E'
RNIND_D16,0  'h','E','!','E'
IMM8,RD  'm','E','!','D'
ABS8,RD  'm','E','!','D'
RS,ABS8  'm','S','!','E'
IMM8,ABS8  'm','I','!','E'
IMM16,ABS8  'm','I','!','E'
IMM16,RD  'm','E','!','D'
IMM16,0  'h','E','!','E'
ABS8,RD  'm','E','!','D'
ABS16,RD  '-','E','!','D'
RS,ABS16  '-','S','!','E'
IMM8,ABS16  'm','I','!','E'
IMM16,ABS16  'm','I','!','E'
ABS16,0  'a','E','!','E'
ABS16,0  'h','E','!','E'
ABS16,RD  'm','E','!','D'
RS,ABS16  'm','S','!','E'
ABS16,RD  'p','E','D','D'
ABS16,0  'a','E','!','E'
ABS16,0  'h','E','!','E'
RS,RNINC  'm','S','!','E'
RN,RD  'm','E','!','D'
RS,RNIND  'm','S','!','E'
RNIND,RD  'm','E','!','D'
RS,RNDEC  'm','S','!','E'
RNINC,RD  'm','E','!','D'
RNDEC,RD  'm','E','!','D'
RS,RNIND_D8  'm','S','!','E'
RS,ABS8  'm','S','!','E'
RNIND_D8,RD  'm','E','!','D'
IMM8,RNIND  'm','I','!','E'
ABS8,RD  'm','E','!','D'
IMM8,RNDEC  'm','I','!','E'
IMM8,RNINC  'm','I','!','E'
IMM8,RNIND_D8  'm','I','!','E'
IMM8,ABS8  'm','I','!','E'
RS,RNIND_D16  'm','S','!','E'
IMM16,RD  'm','E','!','D'
IMM16,RNIND  'm','I','!','E'
IMM16,RNINC  'm','I','!','E'
IMM16,RNDEC  'm','I','!','E'
RS,ABS16  'm','S','!','E'
RNIND_D16,RD  'm','E','!','D'
ABS16,RD  'm','E','!','D'
IMM16,RNIND_D8  'm','I','!','E'
IMM8,ABS16  'm','I','!','E'
IMM16,ABS8  'm','I','!','E'
IMM8,RNIND_D16  'm','I','!','E'
IMM16,ABS16  'm','I','!','E'
IMM16,RNIND_D16  'm','I','!','E'
ABS16,0  'm','E','!','E'
ABS16,RD  'm','E','D','D'
ABS16,0  'm','E','!','E'
ABS16,0  'h','E','!','E'
RS,FPIND_D8  'm','S','!','E'
FPIND_D8,RD  'm','E','!','D'
ABS16,0  'h','E','!','E'
IMM8,RD  'm','I','!','D'
RS,FPIND_D8  'm','S','!','E'
RS,ABS8  'm','S','!','E'
ABS8,RD  'm','E','!','D'
RS,RNIND  'm','S','!','E'
RS,RNINC  'm','S','!','E'
RS,RNDEC  'm','S','!','E'
RNIND,RD  'm','E','!','D'
FPIND_D8,RD  'm','E','!','D'
RNINC,RD  'm','E','!','D'
RN,RD  'm','E','!','D'
RNDEC,RD  'm','E','!','D'
RS,ABS8  'm','S','!','E'
RNIND_D8,RD  'm','E','!','D'
RS,RNIND_D8  'm','S','!','E'
IMM16,RD  'm','I','!','D'
ABS8,RD  'm','E','!','D'
IMM16,RNINC  'm','I','!','E'
IMM16,RNDEC  'm','I','!','E'
IMM16,RNIND  'm','I','!','E'
RS,RNIND_D16  'm','S','!','E'
RS,ABS16  'm','S','!','E'
ABS16,RD  'm','E','!','D'
IMM16,RD  'm','E','!','D'
RNIND_D16,RD  'm','E','!','D'
IMM16,RNIND_D8  'm','I','!','E'
IMM16,ABS8  'm','I','!','E'
IMM16,RNIND_D16  'm','I','!','E'
IMM16,ABS16  'm','I','!','E'
FPIND_D8,RD  'm','E','!','D'
RS,ABS8  'm','S','!','E'
RNINC,RD  'm','E','!','D'
RS,RNIND  'm','S','!','E'
RS,RNINC  'm','S','!','E'
RS,RNDEC  'm','S','!','E'
RNDEC,RD  'm','E','!','D'
RS,FPIND_D8  'm','S','!','E'
RNIND,RD  'm','E','!','D'
RN,RD  'm','E','!','D'
ABS8,RD  'm','E','!','D'
IMM8,RD  'm','I','!','D'
IMM8,RD  'm','E','!','D'
RS,ABS8  'm','S','!','E'
IMM8,RNIND  'm','I','!','E'
IMM8,RNINC  'm','I','!','E'
IMM8,RNDEC  'm','I','!','E'
RNIND_D8,RD  'm','E','!','D'
ABS8,RD  'm','E','!','D'
RS,RNIND_D8  'm','S','!','E'
IMM8,RNIND_D8  'm','I','!','E'
ABS16,RD  'm','E','!','D'
IMM8,ABS8  'm','I','!','E'
RS,ABS16  'm','S','!','E'
RS,RNIND_D16  'm','S','!','E'
RNIND_D16,RD  'm','E','!','D'
IMM8,ABS16  'm','I','!','E'
IMM8,RNIND_D16  'm','I','!','E'
ABS8,RD  'm','E','!','D'
RS,ABS8  'm','S','!','E'
RNIND,RD  'm','E','!','D'
RS,RNIND  'm','S','!','E'
RS,RNINC  'm','S','!','E'
RS,RNDEC  'm','S','!','E'
RN,RD  'm','E','!','D'
RS,FPIND_D8  'm','S','!','E'
RNINC,RD  'm','E','!','D'
FPIND_D8,RD  'm','E','!','D'
IMM8,RD  'm','I','!','D'
RNDEC,RD  'm','E','!','D'
RS,RNIND_D8  'm','S','!','E'
IMM8,RNIND  'm','I','!','E'
IMM8,RNINC  'm','I','!','E'
IMM8,RNDEC  'm','I','!','E'
RNIND_D8,RD  'm','E','!','D'
RS,ABS8  'm','S','!','E'
ABS8,RD  'm','E','!','D'
IMM16,RD  'm','I','!','D'
IMM8,ABS8  'm','I','!','E'
RS,RNIND_D16  'm','S','!','E'
IMM16,RNIND  'm','I','!','E'
IMM16,RNINC  'm','I','!','E'
IMM16,RNDEC  'm','I','!','E'
RS,ABS16  'm','S','!','E'
IMM16,RD  'm','E','!','D'
IMM8,RNIND_D8  'm','I','!','E'
RNIND_D16,RD  'm','E','!','D'
ABS16,RD  'm','E','!','D'
IMM16,RNIND_D8  'm','I','!','E'
IMM8,ABS16  'm','I','!','E'
IMM16,ABS8  'm','I','!','E'
IMM8,RNIND_D16  'm','I','!','E'
IMM16,ABS16  'm','I','!','E'
IMM16,RNIND_D16  'm','I','!','E'
IMM8,RD  'a','D','I','!'
RN,RD  'a','D','E','!'
IMM8,RN  'a','E','I','!'
RN,CRW  's','E','!','C'
RNIND,CRW  's','E','!','C'
RNINC,CRW  's','E','!','C'
RNDEC,CRW  's','E','!','C'
RN,RD  'a','D','E','!'
RNIND_D8,CRW  's','E','!','C'
IMM16,RN  'a','E','I','!'
ABS16,CRW  's','E','!','C'
RNIND_D16,CRW  's','E','!','C'
RN,CRB  's','E','!','C'
ABS8,CRW  's','E','!','C'
IMM16,RD  'a','D','I','!'
RN,RD  's','E','D','D'
RS,RD  's','D','!','!'
RS,RD  's','D','!','!'
RD,0  's','D','!','D'
RD,0  's','D','!','D'
RN,RD  's','E','D','D'
RNIND_D8,CRB  's','E','!','C'
RN,CRB  's','E','!','C'
RNINC,CRW  's','E','!','C'
RNIND,CRB  's','E','!','C'
RNDEC,CRW  's','E','!','C'
RNIND,CRW  's','E','!','C'
RNDEC,CRB  's','E','!','C'
RNINC,CRB  's','E','!','C'
ABS8,CRW  's','E','!','C'
ABS8,CRB  's','E','!','C'
IMM8,CRB  's','E','!','C'
RNIND_D8,CRW  's','E','!','C'
RNIND_D8,CRB  's','E','!','C'
ABS16,CRB  's','E','!','C'
ABS16,CRW  's','E','!','C'
IMM16,CRW  's','E','!','C'
RNIND_D16,CRW  's','E','!','C'
RNIND_D16,CRB  's','E','!','C'
RNIND_D16,CRB  's','E','!','C'
ABS16,0  '-','B','!','!'
RDIND,0  '-','B','!','!'
RDIND,0  '-','B','!','!'
RDIND_D8,0  '-','B','!','!'
RDIND_D8,0  '-','B','!','!'
RDIND_D16,0  '-','B','!','!'
RDIND_D16,0  '-','B','!','!'
ABS16,0  '-','B','!','!'
RD,0  's','D','!','D'
ABS16,CRB  's','E','!','C'
RD,0  's','D','!','D'
RN,CRW  's','E','!','C'
RNDEC,RD  's','E','D','D'
RNDEC,RD  's','E','D','D'
RNINC,CRB  's','E','!','C'
RNINC,RD  's','E','D','D'
RNINC,RD  's','E','D','D'
RNIND,CRB  's','E','!','C'
RNIND,RD  's','E','D','D'
RNIND,RD  's','E','D','D'
RNIND_D8,RD  's','E','D','D'
RNIND_D8,RD  's','E','D','D'
RNIND_D16,RD  's','E','D','D'
RNIND_D16,RD  's','E','D','D'
IMM8,CRB  's','E','!','C'
IMM8,RD  's','E','D','D'
ABS8,RD  's','E','D','D'
IMM16,RD  's','E','D','D'
ABS16,RD  's','E','D','D'
ABS16,RD  's','E','D','D'
RN,RD  's','E','D','D'
RNINC,RD  's','E','D','D'
RNDEC,RD  's','E','D','D'
RNIND,RD  's','E','D','D'
ABS8,RD  's','E','D','D'
RNIND_D8,RD  's','E','D','D'
IMM16,RD  's','E','D','D'
ABS16,RD  's','E','D','D'
RNIND_D16,RD  's','E','D','D'
ABS8,CRB  's','E','!','C'
ABS8,RD  's','E','D','D'
IMM16,RD  'a','D','I','!'
RNDEC,RD  'a','D','E','!'
RNDEC,CRB  's','E','!','C'
IMM8,RNDEC  'a','E','I','!'
IMM16,RNDEC  'a','E','I','!'
RNDEC,RD  'a','D','E','!'
RNINC,RD  'a','D','E','!'
IMM8,RNINC  'a','E','I','!'
IMM16,RNINC  'a','E','I','!'
RNINC,RD  'a','D','E','!'
RNIND,RD  'a','D','E','!'
IMM8,RNIND  'a','E','I','!'
IMM16,RNIND  'a','E','I','!'
RNIND,RD  'a','D','E','!'
RNIND_D8,RD  'a','D','E','!'
IMM8,RNIND_D8  'a','E','I','!'
IMM16,RNIND_D8  'a','E','I','!'
RNIND_D8,RD  'a','D','E','!'
RNIND_D16,RD  'a','D','E','!'
IMM8,RNIND_D16  'a','E','I','!'
IMM16,RNIND_D16  'a','E','I','!'
RNIND_D16,RD  'a','D','E','!'
SPINC,RLIST  '-','E','!','C'
IMM8,RD  'a','D','E','!'
ABS8,RD  'a','D','E','!'
IMM8,ABS8  'a','E','I','!'
IMM16,ABS8  'a','E','I','!'
IMM16,RD  'a','D','E','!'
IMM16,CRW  's','E','!','C'
ABS8,RD  'a','D','E','!'
ABS16,RD  'a','D','E','!'
IMM8,ABS16  'a','E','I','!'
IMM16,ABS16  'a','E','I','!'
ABS16,RD  'a','D','E','!'
FP,IMM16  '-','S','I','!'
RN,RD  'a','D','E','!'
RNIND,RD  'a','D','E','!'
RNINC,RD  'a','D','E','!'
RNDEC,RD  'a','D','E','!'
RNIND_D8,RD  'a','D','E','!'
ABS8,RD  'a','D','E','!'
IMM16,RNINC  'a','E','I','!'
IMM16,RNIND  'a','E','I','!'
IMM16,RN  'a','E','I','!'
IMM16,RNDEC  'a','E','I','!'
IMM16,RD  'a','D','E','!'
ABS16,RD  'a','D','E','!'
RNIND_D16,RD  'a','D','E','!'
IMM16,RNIND_D8  'a','E','I','!'
IMM16,ABS8  'a','E','I','!'
IMM16,RNIND_D16  'a','E','I','!'
IMM16,ABS16  'a','E','I','!'
FP,IMM8  '-','S','I','!'
IMM8,RD  'a','D','I','!'
RN,RD  'a','D','E','!'
RNDEC,RD  'a','D','E','!'
RNINC,RD  'a','D','E','!'
RNIND,RD  'a','D','E','!'
IMM16,RD  'a','D','I','!'
RNIND_D8,RD  'a','D','E','!'
ABS8,RD  'a','D','E','!'
IMM16,RNINC  'a','E','I','!'
IMM16,RNDEC  'a','E','I','!'
IMM16,RNIND  'a','E','I','!'
RNIND_D16,RD  'a','D','E','!'
ABS16,RD  'a','D','E','!'
IMM16,RD  'a','D','E','!'
IMM16,RN  'a','E','I','!'
IMM16,RNIND_D8  'a','E','I','!'
IMM16,ABS8  'a','E','I','!'
IMM16,RNIND_D16  'a','E','I','!'
IMM16,ABS16  'a','E','I','!'
RN,RD  'a','D','E','!'
RNDEC,RD  'a','D','E','!'
RNINC,RD  'a','D','E','!'
IMM8,RD  'a','D','I','!'
RNIND,RD  'a','D','E','!'
IMM8,RN  'a','E','I','!'
IMM8,RNIND  'a','E','I','!'
IMM8,RNINC  'a','E','I','!'
IMM8,RNDEC  'a','E','I','!'
ABS8,RD  'a','D','E','!'
RNIND_D8,RD  'a','D','E','!'
IMM8,RD  'a','D','E','!'
IMM8,ABS8  'a','E','I','!'
ABS16,RD  'a','D','E','!'
IMM8,RNIND_D8  'a','E','I','!'
RNIND_D16,RD  'a','D','E','!'
IMM8,ABS16  'a','E','I','!'
IMM8,RNIND_D16  'a','E','I','!'
RN,RD  'a','D','E','!'
IMM8,RD  'a','D','I','!'
RNINC,RD  'a','D','E','!'
RNIND,RD  'a','D','E','!'
RNDEC,RD  'a','D','E','!'
IMM16,RD  'a','D','I','!'
RNIND_D8,RD  'a','D','E','!'
ABS8,RD  'a','D','E','!'
IMM16,RN  'a','E','I','!'
IMM16,RNDEC  'a','E','I','!'
IMM16,RNIND  'a','E','I','!'
RNIND_D16,RD  'a','D','E','!'
IMM16,RNINC  'a','E','I','!'
ABS16,RD  'a','D','E','!'
IMM16,RD  'a','D','E','!'
IMM16,ABS8  'a','E','I','!'
IMM16,RNIND_D8  'a','E','I','!'
IMM16,ABS16  'a','E','I','!'
IMM16,RNIND_D16  'a','E','I','!'
IMM4,RN  'b','E','I','E'
IMM4,RN  'b','E','I','E'
RS,RN  'b','E','S','E'
RS,RN  'b','E','S','E'
RN,0  'c','!','!','E'
IMM4,RN  'b','E','I','E'
IMM4,RN  'b','E','I','E'
RNDEC,0  'c','!','!','E'
RNINC,0  'c','!','!','E'
RNIND,0  'c','!','!','E'
RS,RNIND  'b','E','S','E'
RNIND_D8,0  'c','!','!','E'
IMM4,RNIND_D16  'b','E','I','E'
RS,RNIND_D16  'b','E','S','E'
RNIND_D16,0  'c','!','!','E'
RNIND_D16,0  'c','!','!','E'
IMM8,0  'c','!','!','E'
ABS8,0  'c','!','!','E'
RN,0  'c','!','!','E'
RNIND,0  'c','!','!','E'
RNINC,0  'c','!','!','E'
RNDEC,0  'c','!','!','E'
ABS8,0  'c','!','!','E'
RNIND_D8,0  'c','!','!','E'
IMM16,0  'c','!','!','E'
ABS16,0  'c','!','!','E'
RNIND_D16,0  'c','!','!','E'
IMM4,ABS8  'b','E','I','E'
RS,ABS8  'b','E','S','E'
PCREL8,0  '-','B','!','!'
PCREL16,0  '-','B','!','!'
RS,RN  'b','E','S','E'
IMM4,RNDEC  'b','E','I','E'
PCREL8,0  '-','B','!','!'
PCREL16,0  '-','B','!','!'
RS,RNDEC  'b','E','S','E'
IMM4,RNDEC  'b','E','I','E'
RS,RNDEC  'b','E','S','E'
RNDEC,0  'c','!','!','E'
IMM4,RNINC  'b','E','I','E'
RS,RNINC  'b','E','S','E'
IMM4,RNINC  'b','E','I','E'
RS,RNINC  'b','E','S','E'
IMM4,RNIND  'b','E','I','E'
RS,RNIND  'b','E','S','E'
IMM4,RNIND  'b','E','I','E'
IMM4,RNIND_D8  'b','E','I','E'
RS,RNIND_D8  'b','E','S','E'
IMM4,RNIND_D8  'b','E','I','E'
RS,RNIND_D8  'b','E','S','E'
RNIND_D8,0  'c','!','!','E'
IMM4,RNIND_D16  'b','E','I','E'
RS,RNIND_D16  'b','E','S','E'
IMM16,0  'c','!','!','E'
IMM4,ABS8  'b','E','I','E'
RS,ABS8  'b','E','S','E'
ABS8,0  'c','!','!','E'
IMM4,ABS16  'b','E','I','E'
RS,ABS16  'b','E','S','E'
ABS16,0  'c','!','!','E'
IMM4,ABS16  'b','E','I','E'
RS,ABS16  'b','E','S','E'
ABS16,0  'c','!','!','E'
PCREL8,0  '-','B','!','!'
PCREL8,0  '-','B','!','!'
PCREL16,0  '-','B','!','!'
PCREL16,0  '-','B','!','!'
RS,RN  'b','E','S','E'
IMM4,RNDEC  'b','E','I','E'
IMM4,RNIND  'b','E','I','E'
RS,RNIND  'b','E','S','E'
RS,RNINC  'b','E','S','E'
RS,RNDEC  'b','E','S','E'
IMM4,RN  'b','E','I','E'
IMM4,RNINC  'b','E','I','E'
RS,RNIND_D8  'b','E','S','E'
IMM4,ABS8  'b','E','I','E'
RS,ABS8  'b','E','S','E'
IMM4,RNIND_D8  'b','E','I','E'
IMM4,ABS16  'b','E','I','E'
RS,RNIND_D16  'b','E','S','E'
RS,ABS16  'b','E','S','E'
IMM4,RNDEC  'b','E','I','E'
PCREL16,0  '-','B','!','!'
PCREL8,0  '-','B','!','!'
PCREL8,0  '-','B','!','!'
PCREL16,0  '-','B','!','!'
IMM4,RNIND_D16  'b','E','I','E'
RS,RNDEC  'b','E','S','E'
PCREL8,0  '-','B','!','!'
PCREL16,0  '-','B','!','!'
RS,RN  'b','E','S','E'
RN,0  'c','!','!','E'
IMM4,RNDEC  'b','E','I','E'
RS,RNDEC  'b','E','S','E'
IMM4,RNINC  'b','E','I','E'
RS,RNINC  'b','E','S','E'
IMM4,RNINC  'b','E','I','E'
RS,RNINC  'b','E','S','E'
RNINC,0  'c','!','!','E'
IMM4,RNIND  'b','E','I','E'
RS,RNIND  'b','E','S','E'
IMM4,RNIND  'b','E','I','E'
RS,RNIND  'b','E','S','E'
RNIND,0  'c','!','!','E'
IMM4,RNIND_D8  'b','E','I','E'
RS,RNIND_D8  'b','E','S','E'
IMM4,RNIND_D8  'b','E','I','E'
RS,RNIND_D8  'b','E','S','E'
IMM4,RNIND_D16  'b','E','I','E'
RS,RNIND_D16  'b','E','S','E'
IMM4,RNIND_D16  'b','E','I','E'
RS,RNIND_D16  'b','E','S','E'
IMM4,ABS8  'b','E','I','E'
RS,ABS8  'b','E','S','E'
IMM4,ABS8  'b','E','I','E'
RS,ABS8  'b','E','S','E'
PCREL8,0  '-','B','!','!'
IMM4,ABS16  'b','E','I','E'
RS,ABS16  'b','E','S','E'
IMM4,ABS16  'b','E','I','E'
RS,ABS16  'b','E','S','E'
PCREL16,0  '-','B','!','!'
RS,RN  'b','E','S','E'
IMM4,RN  'b','E','I','E'
IMM4,RNIND  'b','E','I','E'
RS,RNIND  'b','E','S','E'
RS,RNINC  'b','E','S','E'
RS,RNDEC  'b','E','S','E'
IMM4,RNINC  'b','E','I','E'
IMM4,RNDEC  'b','E','I','E'
RS,RNIND_D8  'b','E','S','E'
IMM4,RNIND_D8  'b','E','I','E'
RS,ABS8  'b','E','S','E'
IMM4,ABS8  'b','E','I','E'
IMM4,ABS16  'b','E','I','E'
RS,RNIND_D16  'b','E','S','E'
RS,ABS16  'b','E','S','E'
IMM4,RNIND_D16  'b','E','I','E'
IMM4,RN  'b','E','I','E'
IMM4,RN  'b','E','I','E'
PCREL8,0  '-','B','!','!'
PCREL16,0  '-','B','!','!'
QIM,RN  'a','I','E','E'
RN,RD  '-','E','D','D'
PCREL8,0  '-','B','!','!'
PCREL16,0  '-','B','!','!'
RN,RD  'a','E','D','D'
RN,RD  'm','E','D','D'
RS,RN  'b','E','S','E'
PCREL8,0  '-','B','!','!'
PCREL16,0  '-','B','!','!'
RS,RN  'b','E','S','E'
RN,RD  'a','E','D','D'
IMM4,RN  'b','E','I','E'
IMM4,RN  'b','E','I','E'
QIM,RN  'a','I','E','E'
RN,RD  'a','E','D','D'
RN,RD  '-','E','D','D'
RN,RD  'm','E','D','D'
RS,RN  'b','E','S','E'
RS,RN  'b','E','S','E'
RN,RD  'a','E','D','D'
IMM4,RNDEC  'b','E','I','E'
IMM4,RNDEC  'b','E','I','E'
QIM,RNDEC  'a','I','E','E'
RNDEC,RD  'a','E','D','D'
RNDEC,RD  '-','E','D','D'
RNDEC,RD  'm','E','D','D'
RS,RNDEC  'b','E','S','E'
RS,RNDEC  'b','E','S','E'
RNDEC,RD  'a','E','D','D'
IMM4,RNDEC  'b','E','I','E'
IMM4,RNDEC  'b','E','I','E'
QIM,RNDEC  'a','I','E','E'
RNDEC,RD  'a','E','D','D'
RNDEC,RD  '-','E','D','D'
RNDEC,RD  'm','E','D','D'
RS,RNDEC  'b','E','S','E'
RS,RNDEC  'b','E','S','E'
RNDEC,RD  'a','E','D','D'
RNINC,RD  'a','E','D','D'
RNINC,RD  'm','E','D','D'
IMM4,RNINC  'b','E','I','E'
RS,RN  'b','E','S','E'
RS,RNIND  'b','E','S','E'
IMM4,RNIND  'b','E','I','E'
IMM4,RN  'b','E','I','E'
RS,RNINC  'b','E','S','E'
RS,RNDEC  'b','E','S','E'
IMM4,RNDEC  'b','E','I','E'
IMM4,RNINC  'b','E','I','E'
RS,ABS8  'b','E','S','E'
IMM4,ABS8  'b','E','I','E'
RS,RNIND_D8  'b','E','S','E'
IMM4,RNIND_D8  'b','E','I','E'
RS,RNIND_D16  'b','E','S','E'
IMM4,ABS16  'b','E','I','E'
RS,ABS16  'b','E','S','E'
IMM4,RNIND_D16  'b','E','I','E'
IMM4,RNINC  'b','E','I','E'
IMM4,RNINC  'b','E','I','E'
PCREL8,0  '-','B','!','!'
PCREL16,0  '-','B','!','!'
RNINC,RD  '-','E','D','D'
RS,RNINC  'b','E','S','E'
PCREL8,0  '-','B','!','!'
PCREL16,0  '-','B','!','!'
QIM,RNINC  'a','I','E','E'
RS,RNINC  'b','E','S','E'
PCREL8,0  '-','B','!','!'
PCREL16,0  '-','B','!','!'
RNINC,RD  'a','E','D','D'
IMM4,RNINC  'b','E','I','E'
RNINC,RD  'a','E','D','D'
PCREL16,0  '-','B','!','!'
PCREL16,0  '-','B','!','!'
PCREL8,0  '-','B','!','!'
PCREL8,0  '-','B','!','!'
PCREL16,0  '-','B','!','!'
RS,RNINC  'b','E','S','E'
IMM4,RNIND  'b','E','I','E'
PCREL8,0  '-','B','!','!'
PCREL16,0  '-','B','!','!'
PCREL16,0  '-','B','!','!'
PCREL8,0  '-','B','!','!'
PCREL8,0  '-','B','!','!'
PCREL16,0  '-','B','!','!'
IMM4,RNIND  'b','E','I','E'
PCREL8,0  '-','B','!','!'
RNINC,RD  'a','E','D','D'
PCREL16,0  '-','B','!','!'
RNINC,RD  '-','E','D','D'
RS,RNINC  'b','E','S','E'
PCREL8,0  '-','B','!','!'
PCREL16,0  '-','B','!','!'
RNINC,RD  'm','E','D','D'
PCREL8,0  '-','B','!','!'
PCREL8,0  '-','B','!','!'
PCREL16,0  '-','B','!','!'
PCREL16,0  '-','B','!','!'
PCREL8,0  '-','B','!','!'
PCREL8,0  '-','B','!','!'
PCREL16,0  '-','B','!','!'
QIM,RNINC  'a','I','E','E'
QIM,RNIND  'a','I','E','E'
PCREL8,0  '-','B','!','!'
PCREL16,0  '-','B','!','!'
RNIND,RD  'a','E','D','D'
RNIND,RD  '-','E','D','D'
PCREL8,0  '-','B','!','!'
PCREL16,0  '-','B','!','!'
RNIND,RD  'm','E','D','D'
RS,RNIND  'b','E','S','E'
RS,RNIND  'b','E','S','E'
RNIND,RD  'a','E','D','D'
IMM4,RNIND  'b','E','I','E'
IMM4,RNIND  'b','E','I','E'
QIM,RNIND  'a','I','E','E'
RNIND,RD  'a','E','D','D'
RNIND,RD  '-','E','D','D'
RNIND,RD  'm','E','D','D'
RS,RNIND  'b','E','S','E'
RS,RNIND  'b','E','S','E'
RNIND,RD  'a','E','D','D'
IMM4,RNIND_D8  'b','E','I','E'
IMM4,RNIND_D8  'b','E','I','E'
QIM,RNIND_D8  'a','I','E','E'
RNIND_D8,RD  'a','E','D','D'
RNIND_D8,RD  '-','E','D','D'
RNIND_D8,RD  'm','E','D','D'
RS,RNIND_D8  'b','E','S','E'
RS,RNIND_D8  'b','E','S','E'
RNIND_D8,RD  'a','E','D','D'
IMM4,RNIND_D8  'b','E','I','E'
IMM4,RNIND_D8  'b','E','I','E'
QIM,RNIND_D8  'a','I','E','E'
RNIND_D8,RD  'a','E','D','D'
RNIND_D8,RD  '-','E','D','D'
RNIND_D8,RD  'm','E','D','D'
RS,RNIND_D8  'b','E','S','E'
IMM4,RNIND_D16  'b','E','I','E'
QIM,RNIND_D16  'a','I','E','E'
RS,RNIND_D16  'b','E','S','E'
RS,RN  'b','E','S','E'
IMM4,RNDEC  'b','E','I','E'
IMM4,RNINC  'b','E','I','E'
RS,RNIND  'b','E','S','E'
RS,RNINC  'b','E','S','E'
RS,RNDEC  'b','E','S','E'
IMM4,RNIND  'b','E','I','E'
IMM4,RN  'b','E','I','E'
RS,RNIND_D8  'b','E','S','E'
IMM4,ABS8  'b','E','I','E'
RS,ABS8  'b','E','S','E'
IMM4,RNIND_D8  'b','E','I','E'
IMM4,ABS16  'b','E','I','E'
RS,RNIND_D16  'b','E','S','E'
RS,ABS16  'b','E','S','E'
IMM4,RNIND_D16  'b','E','I','E'
RS,RNIND_D8  'b','E','S','E'
RNIND_D16,RD  'a','E','D','D'
PCREL8,0  '-','B','!','!'
PCREL16,0  '-','B','!','!'
RNIND_D16,RD  'a','E','D','D'
RNIND_D16,RD  'a','E','D','D'
IMM8,CRB  's','I','S','S'
IMM16,CRW  's','I','S','S'
RNIND_D8,RD  'a','E','D','D'
IMM4,RNIND_D16  'b','E','I','E'
RNIND_D16,RD  '-','E','D','D'
RNIND_D16,RD  'm','E','D','D'
RS,RNIND_D16  'b','E','S','E'
IMM4,RNIND_D16  'b','E','I','E'
IMM4,RNIND_D16  'b','E','I','E'
QIM,RNIND_D16  'a','I','E','E'
RNIND_D16,RD  '-','E','D','D'
RNIND_D16,RD  'm','E','D','D'
RS,RNIND_D16  'b','E','S','E'
RS,RNIND_D16  'b','E','S','E'
RNIND_D16,RD  'a','E','D','D'
IMM8,RD  'a','E','D','D'
IMM8,RD  '-','E','D','D'
IMM8,RD  'm','E','D','D'
IMM8,CRB  's','I','S','S'
IMM8,RD  'a','E','D','D'
RN,RD  'm','E','D','D'
RNDEC,RD  'm','E','D','D'
RNINC,RD  'm','E','D','D'
RNIND,RD  'm','E','D','D'
ABS8,RD  'm','E','D','D'
RNIND_D8,RD  'm','E','D','D'
IMM16,RD  'm','E','D','D'
ABS16,RD  'm','E','D','D'
RNIND_D16,RD  'm','E','D','D'
IMM4,ABS8  'b','E','I','E'
IMM4,ABS8  'b','E','I','E'
QIM,ABS8  'a','I','E','E'
ABS8,RD  'a','E','D','D'
ABS8,RD  '-','E','D','D'
ABS8,RD  'm','E','D','D'
RS,ABS8  'b','E','S','E'
RS,ABS8  'b','E','S','E'
ABS8,RD  'a','E','D','D'
IMM16,RD  'a','E','D','D'
IMM16,RD  '-','E','D','D'
IMM16,RD  'm','E','D','D'
IMM16,CRW  's','I','S','S'
IMM16,RD  'a','E','D','D'
IMM4,ABS8  'b','E','I','E'
QIM,ABS8  'a','I','E','E'
ABS8,RD  'a','E','D','D'
ABS8,RD  '-','E','D','D'
RN,RD  'a','E','D','D'
RNINC,RD  'a','E','D','D'
RNIND,RD  'a','E','D','D'
RNDEC,RD  'a','E','D','D'
ABS8,RD  'a','E','D','D'
RNIND_D8,RD  'a','E','D','D'
ABS16,RD  'a','E','D','D'
IMM16,RD  'a','E','D','D'
RNIND_D16,RD  'a','E','D','D'
0,0  '-','!','!','!'
IMM4,ABS8  'b','E','I','E'
ABS8,RD  'm','E','D','D'
RS,ABS8  'b','E','S','E'
RS,ABS8  'b','E','S','E'
ABS8,RD  'a','E','D','D'
IMM4,ABS16  'b','E','I','E'
ABS16,RD  'a','E','D','D'
ABS16,RD  '-','E','D','D'
ABS16,RD  'm','E','D','D'
RS,ABS16  'b','E','S','E'
ABS16,RD  'a','E','D','D'
IMM4,ABS16  'b','E','I','E'
ABS16,RD  '-','E','D','D'
ABS16,RD  'm','E','D','D'
RS,ABS16  'b','E','S','E'
ABS16,RD  'a','E','D','D'
PCREL8,0  '-','B','!','!'
RN,RD  '-','E','D','D'
RNIND,RD  '-','E','D','D'
RNINC,RD  '-','E','D','D'
RNDEC,RD  '-','E','D','D'
ABS8,RD  '-','E','D','D'
RNIND_D8,RD  '-','E','D','D'
ABS16,RD  '-','E','D','D'
IMM16,RD  '-','E','D','D'
RNIND_D16,RD  '-','E','D','D'
IMM4,ABS16  'b','E','I','E'
QIM,ABS16  'a','I','E','E'
RS,ABS16  'b','E','S','E'
IMM4,ABS16  'b','E','I','E'
QIM,ABS16  'a','I','E','E'
ABS16,RD  'a','E','D','D'
RS,ABS16  'b','E','S','E'
PCREL8,0  '-','B','!','!'
PCREL8,0  '-','B','!','!'
PCREL8,0  '-','B','!','!'
PCREL8,0  '-','B','!','!'
PCREL8,0  '-','B','!','!'
PCREL8,0  '-','B','!','!'
PCREL8,0  '-','B','!','!'
PCREL8,0  '-','B','!','!'
PCREL8,0  '-','B','!','!'
QIM,RN  'a','I','E','E'
QIM,RNDEC  'a','I','E','E'
QIM,RNINC  'a','I','E','E'
QIM,RNIND  'a','I','E','E'
QIM,ABS8  'a','I','E','E'
QIM,RNIND_D8  'a','I','E','E'
QIM,ABS16  'a','I','E','E'
QIM,RNIND_D16  'a','I','E','E'
PCREL8,0  '-','B','!','!'
PCREL8,0  '-','B','!','!'
PCREL8,0  '-','B','!','!'
PCREL8,0  '-','B','!','!'
PCREL16,0  '-','B','!','!'
PCREL16,0  '-','B','!','!'
PCREL16,0  '-','B','!','!'
PCREL16,0  '-','B','!','!'
PCREL16,0  '-','B','!','!'
PCREL16,0  '-','B','!','!'
PCREL16,0  '-','B','!','!'
PCREL16,0  '-','B','!','!'
PCREL16,0  '-','B','!','!'
PCREL16,0  '-','B','!','!'
PCREL16,0  '-','B','!','!'
PCREL16,0  '-','B','!','!'
PCREL16,0  '-','B','!','!'
PCREL16,0  '-','B','!','!'
RN,RD  'a','E','D','D'
RNDEC,RD  'a','E','D','D'
RNINC,RD  'a','E','D','D'
RNIND,RD  'a','E','D','D'
ABS8,RD  'a','E','D','D'
RNIND_D8,RD  'a','E','D','D'
ABS16,RD  'a','E','D','D'
IMM16,RD  'a','E','D','D'
RNIND_D16,RD  'a','E','D','D'
RN,RD  'a','E','D','D'
QIM,RN  'a','I','E','E'
QIM,RNIND  'a','I','E','E'
RNDEC,RD  'a','E','D','D'
QIM,RNDEC  'a','I','E','E'
QIM,RNINC  'a','I','E','E'
RNIND,RD  'a','E','D','D'
RNINC,RD  'a','E','D','D'
QIM,ABS8  'a','I','E','E'
QIM,RNIND_D8  'a','I','E','E'
ABS8,RD  'a','E','D','D'
RNIND_D8,RD  'a','E','D','D'
ABS16,RD  'a','E','D','D'
QIM,RNIND_D16  'a','I','E','E'
IMM16,RD  'a','E','D','D'
QIM,ABS16  'a','I','E','E'
RNIND_D16,RD  'a','E','D','D'
RN,RD  'a','E','D','D'
QIM,RN  'a','I','E','E'
QIM,RNINC  'a','I','E','E'
RNDEC,RD  'a','E','D','D'
QIM,RNIND  'a','I','E','E'
RNINC,RD  'a','E','D','D'
QIM,RNDEC  'a','I','E','E'
RNIND,RD  'a','E','D','D'
QIM,RNIND_D8  'a','I','E','E'
IMM8,RD  'a','E','D','D'
QIM,ABS8  'a','I','E','E'
ABS8,RD  'a','E','D','D'
RNIND_D8,RD  'a','E','D','D'
QIM,RNIND_D16  'a','I','E','E'
QIM,ABS16  'a','I','E','E'
ABS16,RD  'a','E','D','D'
RNIND_D16,RD  'a','E','D','D'
RN,RD  'a','E','D','D'
QIM,RN  'a','I','E','E'
QIM,RNDEC  'a','I','E','E'
RNDEC,RD  'a','E','D','D'
QIM,RNIND  'a','I','E','E'
QIM,RNINC  'a','I','E','E'
RNINC,RD  'a','E','D','D'
RNIND,RD  'a','E','D','D'
QIM,ABS8  'a','I','E','E'
QIM,RNIND_D8  'a','I','E','E'
RNIND_D8,RD  'a','E','D','D'
ABS8,RD  'a','E','D','D'
ABS16,RD  'a','E','D','D'
QIM,RNIND_D16  'a','I','E','E'
IMM16,RD  'a','E','D','D'
QIM,ABS16  'a','I','E','E'
RNIND_D16,RD  'a','E','D','D'
*/
{0,0,0,0,0,0,NULL,0,{0,0},0,{}}}
#endif
;
#endif
