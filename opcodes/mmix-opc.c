/* mmix-opc.c -- MMIX opcode table
   Copyright (C) 2001, 2003 Free Software Foundation, Inc.
   Written by Hans-Peter Nilsson (hp@bitrange.com)

This file is part of GDB, GAS, and the GNU binutils.

GDB, GAS, and the GNU binutils are free software; you can redistribute
them and/or modify them under the terms of the GNU General Public
License as published by the Free Software Foundation; either version 2,
or (at your option) any later version.

GDB, GAS, and the GNU binutils are distributed in the hope that they
will be useful, but WITHOUT ANY WARRANTY; without even the implied
warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See
the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this file; see the file COPYING.  If not, write to the Free
Software Foundation, 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.  */

#include <stdio.h>
#include "opcode/mmix.h"
#include "symcat.h"

/* Register-name-table for special registers.  */
const struct mmix_spec_reg mmix_spec_regs[] =
 {
   /* Keep rJ at top; it's the most frequently used one.  */
   {"rJ", 4},
   {"rA", 21},
   {"rB", 0},
   {"rC", 8},
   {"rD", 1},
   {"rE", 2},
   {"rF", 22},
   {"rG", 19},
   {"rH", 3},
   {"rI", 12},
   {"rK", 15},
   {"rL", 20},
   {"rM", 5},
   {"rN", 9},
   {"rO", 10},
   {"rP", 23},
   {"rQ", 16},
   {"rR", 6},
   {"rS", 11},
   {"rT", 13},
   {"rU", 17},
   {"rV", 18},
   {"rW", 24},
   {"rX", 25},
   {"rY", 26},
   {"rZ", 27},
   {"rBB", 7},
   {"rTT", 14},
   {"rWW", 28},
   {"rXX", 29},
   {"rYY", 30},
   {"rZZ", 31},
   {NULL, 0}
 };

/* Opcode-table.  In order to cut down on redundant contents, we use helper
   macros.  */

/* All bits in the opcode-byte are significant.  Add "| ..." expressions
   to add zero-bits.  */
#undef O
#define O(m) ((m) << 24), ((~(m) & 255) << 24)

/* Bits 7..1 of the opcode are significant.  */
#undef Z
#define Z(m) ((m) << 24), ((~(m) & 254) << 24)

/* For easier overview of the table.  */
#define N mmix_type_normal
#define B mmix_type_branch
#define C mmix_type_condbranch
#define MB mmix_type_memaccess_byte
#define MW mmix_type_memaccess_wyde
#define MT mmix_type_memaccess_tetra
#define MO mmix_type_memaccess_octa
#define M mmix_type_memaccess_block
#define J mmix_type_jsr
#define P mmix_type_pseudo

#define OP(y) XCONCAT2 (mmix_operands_,y)

/* Groups of instructions specified here must, if all are matching the
   same instruction, be consecutive, in order more-specific to
   less-specific match.  */

const struct mmix_opcode mmix_opcodes[] =
 {
   {"trap",	O (0),		OP (xyz_opt),		J},
   {"fcmp",	O (1),		OP (regs),		N},
   {"flot",	Z (8),		OP (roundregs_z),	N},

   {"fun",	O (2),		OP (regs),		N},
   {"feql",	O (3),		OP (regs),		N},
   {"flotu",	Z (10),		OP (roundregs_z),	N},

   {"fadd",	O (4),		OP (regs),		N},
   {"fix",	O (5),		OP (roundregs),		N},
   {"sflot",	Z (12),		OP (roundregs_z),	N},

   {"fsub",	O (6),		OP (regs),		N},
   {"fixu",	O (7),		OP (roundregs),		N},
   {"sflotu",	Z (14),		OP (roundregs_z),	N},

   {"fmul",	O (16),		OP (regs),		N},
   {"fcmpe",	O (17),		OP (regs),		N},
   {"mul",	Z (24),		OP (regs_z),		N},

   {"fune",	O (18),		OP (regs),		N},
   {"feqle",	O (19),		OP (regs),		N},
   {"mulu",	Z (26),		OP (regs_z),		N},

   {"fdiv",	O (20),		OP (regs),		N},
   {"fsqrt",	O (21),		OP (roundregs),		N},
   {"div",	Z (28),		OP (regs_z),		N},

   {"frem",	O (22),		OP (regs),		N},
   {"fint",	O (23),		OP (roundregs),		N},
   {"divu",	Z (30),		OP (regs_z),		N},

   {"add",	Z (0x20),	OP (regs_z),		N},
   {"2addu",	Z (0x28),	OP (regs_z),		N},

   {"addu",	Z (0x22),	OP (regs_z),		N},
   /* Synonym for ADDU.  Put after ADDU, since we don't prefer it for
      disassembly.  It's supposed to be used for addresses, so we make it
      a memory block reference for purposes of assembly.  */
   {"lda",	Z (0x22),	OP (regs_z_opt),	M},
   {"4addu",	Z (0x2a),	OP (regs_z),		N},

   {"sub",	Z (0x24),	OP (regs_z),		N},
   {"8addu",	Z (0x2c),	OP (regs_z),		N},

   {"subu",	Z (0x26),	OP (regs_z),		N},
   {"16addu",	Z (0x2e),	OP (regs_z),		N},

   {"cmp",	Z (0x30),	OP (regs_z),		N},
   {"sl",	Z (0x38),	OP (regs_z),		N},

   {"cmpu",	Z (0x32),	OP (regs_z),		N},
   {"slu",	Z (0x3a),	OP (regs_z),		N},

   {"neg",	Z (0x34),	OP (neg),		N},
   {"sr",	Z (0x3c),	OP (regs_z),		N},

   {"negu",	Z (0x36),	OP (neg),		N},
   {"sru",	Z (0x3e),	OP (regs_z),		N},

   {"bn",	Z (0x40),	OP (regaddr),		C},
   {"bnn",	Z (0x48),	OP (regaddr),		C},

   {"bz",	Z (0x42),	OP (regaddr),		C},
   {"bnz",	Z (0x4a),	OP (regaddr),		C},

   {"bp",	Z (0x44),	OP (regaddr),		C},
   {"bnp",	Z (0x4c),	OP (regaddr),		C},

   {"bod",	Z (0x46),	OP (regaddr),		C},
   {"bev",	Z (0x4e),	OP (regaddr),		C},

   {"pbn",	Z (0x50),	OP (regaddr),		C},
   {"pbnn",	Z (0x58),	OP (regaddr),		C},

   {"pbz",	Z (0x52),	OP (regaddr),		C},
   {"pbnz",	Z (0x5a),	OP (regaddr),		C},

   {"pbp",	Z (0x54),	OP (regaddr),		C},
   {"pbnp",	Z (0x5c),	OP (regaddr),		C},

   {"pbod",	Z (0x56),	OP (regaddr),		C},
   {"pbev",	Z (0x5e),	OP (regaddr),		C},

   {"csn",	Z (0x60),	OP (regs_z),		N},
   {"csnn",	Z (0x68),	OP (regs_z),		N},

   {"csz",	Z (0x62),	OP (regs_z),		N},
   {"csnz",	Z (0x6a),	OP (regs_z),		N},

   {"csp",	Z (0x64),	OP (regs_z),		N},
   {"csnp",	Z (0x6c),	OP (regs_z),		N},

   {"csod",	Z (0x66),	OP (regs_z),		N},
   {"csev",	Z (0x6e),	OP (regs_z),		N},

   {"zsn",	Z (0x70),	OP (regs_z),		N},
   {"zsnn",	Z (0x78),	OP (regs_z),		N},

   {"zsz",	Z (0x72),	OP (regs_z),		N},
   {"zsnz",	Z (0x7a),	OP (regs_z),		N},

   {"zsp",	Z (0x74),	OP (regs_z),		N},
   {"zsnp",	Z (0x7c),	OP (regs_z),		N},

   {"zsod",	Z (0x76),	OP (regs_z),		N},
   {"zsev",	Z (0x7e),	OP (regs_z),		N},

   {"ldb",	Z (0x80),	OP (regs_z_opt),	MB},
   {"ldt",	Z (0x88),	OP (regs_z_opt),	MT},

   {"ldbu",	Z (0x82),	OP (regs_z_opt),	MB},
   {"ldtu",	Z (0x8a),	OP (regs_z_opt),	MT},

   {"ldw",	Z (0x84),	OP (regs_z_opt),	MW},
   {"ldo",	Z (0x8c),	OP (regs_z_opt),	MO},

   {"ldwu",	Z (0x86),	OP (regs_z_opt),	MW},
   {"ldou",	Z (0x8e),	OP (regs_z_opt),	MO},

   {"ldsf",	Z (0x90),	OP (regs_z_opt),	MT},

   /* This doesn't seem to access memory, just the TLB.  */
   {"ldvts",	Z (0x98),	OP (regs_z_opt),	M},

   {"ldht",	Z (0x92),	OP (regs_z_opt),	MT},

   /* Neither does this per-se.  */
   {"preld",	Z (0x9a),	OP (x_regs_z),		N},

   {"cswap",	Z (0x94),	OP (regs_z_opt),	MO},
   {"prego",	Z (0x9c),	OP (x_regs_z),		N},

   {"ldunc",	Z (0x96),	OP (regs_z_opt),	MO},
   {"go",	Z (GO_INSN_BYTE),
				OP (regs_z_opt),	B},

   {"stb",	Z (0xa0),	OP (regs_z_opt),	MB},
   {"stt",	Z (0xa8),	OP (regs_z_opt),	MT},

   {"stbu",	Z (0xa2),	OP (regs_z_opt),	MB},
   {"sttu",	Z (0xaa),	OP (regs_z_opt),	MT},

   {"stw",	Z (0xa4),	OP (regs_z_opt),	MW},
   {"sto",	Z (0xac),	OP (regs_z_opt),	MO},

   {"stwu",	Z (0xa6),	OP (regs_z_opt),	MW},
   {"stou",	Z (0xae),	OP (regs_z_opt),	MO},

   {"stsf",	Z (0xb0),	OP (regs_z_opt),	MT},
   {"syncd",	Z (0xb8),	OP (x_regs_z),		M},

   {"stht",	Z (0xb2),	OP (regs_z_opt),	MT},
   {"prest",	Z (0xba),	OP (x_regs_z),		M},

   {"stco",	Z (0xb4),	OP (x_regs_z),		MO},
   {"syncid",	Z (0xbc),	OP (x_regs_z),		M},

   {"stunc",	Z (0xb6),	OP (regs_z_opt),	MO},
   {"pushgo",	Z (PUSHGO_INSN_BYTE),
				OP (pushgo),		J},

   /* Synonym for OR with a zero Z.  */
   {"set",	O (0xc1)
		  | 0xff,	OP (set),		N},

   {"or",	Z (0xc0),	OP (regs_z),		N},
   {"and",	Z (0xc8),	OP (regs_z),		N},

   {"orn",	Z (0xc2),	OP (regs_z),		N},
   {"andn",	Z (0xca),	OP (regs_z),		N},

   {"nor",	Z (0xc4),	OP (regs_z),		N},
   {"nand",	Z (0xcc),	OP (regs_z),		N},

   {"xor",	Z (0xc6),	OP (regs_z),		N},
   {"nxor",	Z (0xce),	OP (regs_z),		N},

   {"bdif",	Z (0xd0),	OP (regs_z),		N},
   {"mux",	Z (0xd8),	OP (regs_z),		N},

   {"wdif",	Z (0xd2),	OP (regs_z),		N},
   {"sadd",	Z (0xda),	OP (regs_z),		N},

   {"tdif",	Z (0xd4),	OP (regs_z),		N},
   {"mor",	Z (0xdc),	OP (regs_z),		N},

   {"odif",	Z (0xd6),	OP (regs_z),		N},
   {"mxor",	Z (0xde),	OP (regs_z),		N},

   {"seth",	O (0xe0),	OP (reg_yz),		N},
   {"setmh",	O (0xe1),	OP (reg_yz),		N},
   {"orh",	O (0xe8),	OP (reg_yz),		N},
   {"ormh",	O (0xe9),	OP (reg_yz),		N},

   {"setml",	O (0xe2),	OP (reg_yz),		N},
   {"setl",	O (SETL_INSN_BYTE),
				OP (reg_yz),		N},
   {"orml",	O (0xea),	OP (reg_yz),		N},
   {"orl",	O (0xeb),	OP (reg_yz),		N},

   {"inch",	O (INCH_INSN_BYTE),
				OP (reg_yz),		N},
   {"incmh",	O (INCMH_INSN_BYTE),
				OP (reg_yz),		N},
   {"andnh",	O (0xec),	OP (reg_yz),		N},
   {"andnmh",	O (0xed),	OP (reg_yz),		N},

   {"incml",	O (INCML_INSN_BYTE),
				OP (reg_yz),		N},
   {"incl",	O (0xe7),	OP (reg_yz),		N},
   {"andnml",	O (0xee),	OP (reg_yz),		N},
   {"andnl",	O (0xef),	OP (reg_yz),		N},

   {"jmp",	Z (0xf0),	OP (jmp),		B},
   {"pop",	O (0xf8),	OP (pop),		B},
   {"resume",	O (0xf9)
		  | 0xffff00,	OP (resume),		B},

   {"pushj",	Z (0xf2),	OP (pushj),		J},
   {"save",	O (0xfa)
		  | 0xffff,	OP (save),		M},
   {"unsave",	O (0xfb)
		  | 0xffff00,	OP (unsave),		M},

   {"geta",	Z (0xf4),	OP (regaddr),		N},
   {"sync",	O (0xfc),	OP (sync),		N},
   {"swym",	O (SWYM_INSN_BYTE),
				OP (xyz_opt),		N},

   {"put", Z (0xf6) | 0xff00,	OP (put),		N},
   {"get", O (0xfe) | 0xffe0,	OP (get),		N},
   {"trip",	O (0xff),	OP (xyz_opt),		J},

   /* We have mmixal pseudos in the ordinary instruction table so we can
      avoid the "set" vs. ".set" ambiguity that would be the effect if we
      had pseudos handled "normally" and defined NO_PSEUDO_DOT.

      Note that IS and GREG are handled fully by md_start_line_hook, so
      they're not here.  */
   {"loc",	~0, ~0,		OP (loc),		P},
   {"prefix",	~0, ~0,		OP (prefix),		P},
   {"byte",	~0, ~0,		OP (byte),		P},
   {"wyde",	~0, ~0,		OP (wyde),		P},
   {"tetra",	~0, ~0,		OP (tetra),		P},
   {"octa",	~0, ~0,		OP (octa),		P},
   {"local",	~0, ~0,		OP (local),		P},
   {"bspec",	~0, ~0,		OP (bspec),		P},
   {"espec",	~0, ~0,		OP (espec),		P},

   {NULL, ~0, ~0, OP (none), N}
 };
