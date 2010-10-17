/* Xtensa configuration-specific ISA information.
   Copyright 2003 Free Software Foundation, Inc.

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

#include <xtensa-isa.h>
#include "xtensa-isa-internal.h"
#include "ansidecl.h"

#define BPW 32
#define WINDEX(_n) ((_n) / BPW)
#define BINDEX(_n) ((_n) %% BPW)

static uint32 tie_do_reloc_l (uint32, uint32) ATTRIBUTE_UNUSED;
static uint32 tie_undo_reloc_l (uint32, uint32) ATTRIBUTE_UNUSED;

static uint32
tie_do_reloc_l (uint32 addr, uint32 pc)
{
  return (addr - pc);
}

static uint32
tie_undo_reloc_l (uint32 offset, uint32 pc)
{
  return (pc + offset);
}

xtensa_opcode_internal** get_opcodes (void);
int get_num_opcodes (void);
int decode_insn (const xtensa_insnbuf);
int interface_version (void);

uint32 get_bbi_field (const xtensa_insnbuf);
void set_bbi_field (xtensa_insnbuf, uint32);
uint32 get_bbi4_field (const xtensa_insnbuf);
void set_bbi4_field (xtensa_insnbuf, uint32);
uint32 get_i_field (const xtensa_insnbuf);
void set_i_field (xtensa_insnbuf, uint32);
uint32 get_imm12_field (const xtensa_insnbuf);
void set_imm12_field (xtensa_insnbuf, uint32);
uint32 get_imm12b_field (const xtensa_insnbuf);
void set_imm12b_field (xtensa_insnbuf, uint32);
uint32 get_imm16_field (const xtensa_insnbuf);
void set_imm16_field (xtensa_insnbuf, uint32);
uint32 get_imm4_field (const xtensa_insnbuf);
void set_imm4_field (xtensa_insnbuf, uint32);
uint32 get_imm6_field (const xtensa_insnbuf);
void set_imm6_field (xtensa_insnbuf, uint32);
uint32 get_imm6hi_field (const xtensa_insnbuf);
void set_imm6hi_field (xtensa_insnbuf, uint32);
uint32 get_imm6lo_field (const xtensa_insnbuf);
void set_imm6lo_field (xtensa_insnbuf, uint32);
uint32 get_imm7_field (const xtensa_insnbuf);
void set_imm7_field (xtensa_insnbuf, uint32);
uint32 get_imm7hi_field (const xtensa_insnbuf);
void set_imm7hi_field (xtensa_insnbuf, uint32);
uint32 get_imm7lo_field (const xtensa_insnbuf);
void set_imm7lo_field (xtensa_insnbuf, uint32);
uint32 get_imm8_field (const xtensa_insnbuf);
void set_imm8_field (xtensa_insnbuf, uint32);
uint32 get_m_field (const xtensa_insnbuf);
void set_m_field (xtensa_insnbuf, uint32);
uint32 get_mn_field (const xtensa_insnbuf);
void set_mn_field (xtensa_insnbuf, uint32);
uint32 get_n_field (const xtensa_insnbuf);
void set_n_field (xtensa_insnbuf, uint32);
uint32 get_none_field (const xtensa_insnbuf);
void set_none_field (xtensa_insnbuf, uint32);
uint32 get_offset_field (const xtensa_insnbuf);
void set_offset_field (xtensa_insnbuf, uint32);
uint32 get_op0_field (const xtensa_insnbuf);
void set_op0_field (xtensa_insnbuf, uint32);
uint32 get_op1_field (const xtensa_insnbuf);
void set_op1_field (xtensa_insnbuf, uint32);
uint32 get_op2_field (const xtensa_insnbuf);
void set_op2_field (xtensa_insnbuf, uint32);
uint32 get_r_field (const xtensa_insnbuf);
void set_r_field (xtensa_insnbuf, uint32);
uint32 get_s_field (const xtensa_insnbuf);
void set_s_field (xtensa_insnbuf, uint32);
uint32 get_sa4_field (const xtensa_insnbuf);
void set_sa4_field (xtensa_insnbuf, uint32);
uint32 get_sae_field (const xtensa_insnbuf);
void set_sae_field (xtensa_insnbuf, uint32);
uint32 get_sae4_field (const xtensa_insnbuf);
void set_sae4_field (xtensa_insnbuf, uint32);
uint32 get_sal_field (const xtensa_insnbuf);
void set_sal_field (xtensa_insnbuf, uint32);
uint32 get_sar_field (const xtensa_insnbuf);
void set_sar_field (xtensa_insnbuf, uint32);
uint32 get_sas_field (const xtensa_insnbuf);
void set_sas_field (xtensa_insnbuf, uint32);
uint32 get_sas4_field (const xtensa_insnbuf);
void set_sas4_field (xtensa_insnbuf, uint32);
uint32 get_sr_field (const xtensa_insnbuf);
void set_sr_field (xtensa_insnbuf, uint32);
uint32 get_t_field (const xtensa_insnbuf);
void set_t_field (xtensa_insnbuf, uint32);
uint32 get_thi3_field (const xtensa_insnbuf);
void set_thi3_field (xtensa_insnbuf, uint32);
uint32 get_z_field (const xtensa_insnbuf);
void set_z_field (xtensa_insnbuf, uint32);


uint32
get_bbi_field (const xtensa_insnbuf insn)
{
  return ((insn[0] & 0xf0000) >> 16) |
         ((insn[0] & 0x100) >> 4);
}

void
set_bbi_field (xtensa_insnbuf insn, uint32 val)
{
  insn[0] = (insn[0] & 0xfff0ffff) | ((val << 16) & 0xf0000);
  insn[0] = (insn[0] & 0xfffffeff) | ((val << 4) & 0x100);
}

uint32
get_bbi4_field (const xtensa_insnbuf insn)
{
  return ((insn[0] & 0x100) >> 8);
}

void
set_bbi4_field (xtensa_insnbuf insn, uint32 val)
{
  insn[0] = (insn[0] & 0xfffffeff) | ((val << 8) & 0x100);
}

uint32
get_i_field (const xtensa_insnbuf insn)
{
  return ((insn[0] & 0x80000) >> 19);
}

void
set_i_field (xtensa_insnbuf insn, uint32 val)
{
  insn[0] = (insn[0] & 0xfff7ffff) | ((val << 19) & 0x80000);
}

uint32
get_imm12_field (const xtensa_insnbuf insn)
{
  return ((insn[0] & 0xfff));
}

void
set_imm12_field (xtensa_insnbuf insn, uint32 val)
{
  insn[0] = (insn[0] & 0xfffff000) | (val & 0xfff);
}

uint32
get_imm12b_field (const xtensa_insnbuf insn)
{
  return ((insn[0] & 0xff)) |
         ((insn[0] & 0xf000) >> 4);
}

void
set_imm12b_field (xtensa_insnbuf insn, uint32 val)
{
  insn[0] = (insn[0] & 0xffffff00) | (val & 0xff);
  insn[0] = (insn[0] & 0xffff0fff) | ((val << 4) & 0xf000);
}

uint32
get_imm16_field (const xtensa_insnbuf insn)
{
  return ((insn[0] & 0xffff));
}

void
set_imm16_field (xtensa_insnbuf insn, uint32 val)
{
  insn[0] = (insn[0] & 0xffff0000) | (val & 0xffff);
}

uint32
get_imm4_field (const xtensa_insnbuf insn)
{
  return ((insn[0] & 0xf00) >> 8);
}

void
set_imm4_field (xtensa_insnbuf insn, uint32 val)
{
  insn[0] = (insn[0] & 0xfffff0ff) | ((val << 8) & 0xf00);
}

uint32
get_imm6_field (const xtensa_insnbuf insn)
{
  return ((insn[0] & 0xf00) >> 8) |
         ((insn[0] & 0x30000) >> 12);
}

void
set_imm6_field (xtensa_insnbuf insn, uint32 val)
{
  insn[0] = (insn[0] & 0xfffff0ff) | ((val << 8) & 0xf00);
  insn[0] = (insn[0] & 0xfffcffff) | ((val << 12) & 0x30000);
}

uint32
get_imm6hi_field (const xtensa_insnbuf insn)
{
  return ((insn[0] & 0x30000) >> 16);
}

void
set_imm6hi_field (xtensa_insnbuf insn, uint32 val)
{
  insn[0] = (insn[0] & 0xfffcffff) | ((val << 16) & 0x30000);
}

uint32
get_imm6lo_field (const xtensa_insnbuf insn)
{
  return ((insn[0] & 0xf00) >> 8);
}

void
set_imm6lo_field (xtensa_insnbuf insn, uint32 val)
{
  insn[0] = (insn[0] & 0xfffff0ff) | ((val << 8) & 0xf00);
}

uint32
get_imm7_field (const xtensa_insnbuf insn)
{
  return ((insn[0] & 0xf00) >> 8) |
         ((insn[0] & 0x70000) >> 12);
}

void
set_imm7_field (xtensa_insnbuf insn, uint32 val)
{
  insn[0] = (insn[0] & 0xfffff0ff) | ((val << 8) & 0xf00);
  insn[0] = (insn[0] & 0xfff8ffff) | ((val << 12) & 0x70000);
}

uint32
get_imm7hi_field (const xtensa_insnbuf insn)
{
  return ((insn[0] & 0x70000) >> 16);
}

void
set_imm7hi_field (xtensa_insnbuf insn, uint32 val)
{
  insn[0] = (insn[0] & 0xfff8ffff) | ((val << 16) & 0x70000);
}

uint32
get_imm7lo_field (const xtensa_insnbuf insn)
{
  return ((insn[0] & 0xf00) >> 8);
}

void
set_imm7lo_field (xtensa_insnbuf insn, uint32 val)
{
  insn[0] = (insn[0] & 0xfffff0ff) | ((val << 8) & 0xf00);
}

uint32
get_imm8_field (const xtensa_insnbuf insn)
{
  return ((insn[0] & 0xff));
}

void
set_imm8_field (xtensa_insnbuf insn, uint32 val)
{
  insn[0] = (insn[0] & 0xffffff00) | (val & 0xff);
}

uint32
get_m_field (const xtensa_insnbuf insn)
{
  return ((insn[0] & 0x30000) >> 16);
}

void
set_m_field (xtensa_insnbuf insn, uint32 val)
{
  insn[0] = (insn[0] & 0xfffcffff) | ((val << 16) & 0x30000);
}

uint32
get_mn_field (const xtensa_insnbuf insn)
{
  return ((insn[0] & 0x30000) >> 16) |
         ((insn[0] & 0xc0000) >> 16);
}

void
set_mn_field (xtensa_insnbuf insn, uint32 val)
{
  insn[0] = (insn[0] & 0xfffcffff) | ((val << 16) & 0x30000);
  insn[0] = (insn[0] & 0xfff3ffff) | ((val << 16) & 0xc0000);
}

uint32
get_n_field (const xtensa_insnbuf insn)
{
  return ((insn[0] & 0xc0000) >> 18);
}

void
set_n_field (xtensa_insnbuf insn, uint32 val)
{
  insn[0] = (insn[0] & 0xfff3ffff) | ((val << 18) & 0xc0000);
}

uint32
get_none_field (const xtensa_insnbuf insn)
{
  return ((insn[0] & 0x0));
}

void
set_none_field (xtensa_insnbuf insn, uint32 val)
{
  insn[0] = (insn[0] & 0xffffffff) | (val & 0x0);
}

uint32
get_offset_field (const xtensa_insnbuf insn)
{
  return ((insn[0] & 0x3ffff));
}

void
set_offset_field (xtensa_insnbuf insn, uint32 val)
{
  insn[0] = (insn[0] & 0xfffc0000) | (val & 0x3ffff);
}

uint32
get_op0_field (const xtensa_insnbuf insn)
{
  return ((insn[0] & 0xf00000) >> 20);
}

void
set_op0_field (xtensa_insnbuf insn, uint32 val)
{
  insn[0] = (insn[0] & 0xff0fffff) | ((val << 20) & 0xf00000);
}

uint32
get_op1_field (const xtensa_insnbuf insn)
{
  return ((insn[0] & 0xf0) >> 4);
}

void
set_op1_field (xtensa_insnbuf insn, uint32 val)
{
  insn[0] = (insn[0] & 0xffffff0f) | ((val << 4) & 0xf0);
}

uint32
get_op2_field (const xtensa_insnbuf insn)
{
  return ((insn[0] & 0xf));
}

void
set_op2_field (xtensa_insnbuf insn, uint32 val)
{
  insn[0] = (insn[0] & 0xfffffff0) | (val & 0xf);
}

uint32
get_r_field (const xtensa_insnbuf insn)
{
  return ((insn[0] & 0xf00) >> 8);
}

void
set_r_field (xtensa_insnbuf insn, uint32 val)
{
  insn[0] = (insn[0] & 0xfffff0ff) | ((val << 8) & 0xf00);
}

uint32
get_s_field (const xtensa_insnbuf insn)
{
  return ((insn[0] & 0xf000) >> 12);
}

void
set_s_field (xtensa_insnbuf insn, uint32 val)
{
  insn[0] = (insn[0] & 0xffff0fff) | ((val << 12) & 0xf000);
}

uint32
get_sa4_field (const xtensa_insnbuf insn)
{
  return ((insn[0] & 0x1));
}

void
set_sa4_field (xtensa_insnbuf insn, uint32 val)
{
  insn[0] = (insn[0] & 0xfffffffe) | (val & 0x1);
}

uint32
get_sae_field (const xtensa_insnbuf insn)
{
  return ((insn[0] & 0xf000) >> 12) |
         ((insn[0] & 0x10));
}

void
set_sae_field (xtensa_insnbuf insn, uint32 val)
{
  insn[0] = (insn[0] & 0xffff0fff) | ((val << 12) & 0xf000);
  insn[0] = (insn[0] & 0xffffffef) | (val & 0x10);
}

uint32
get_sae4_field (const xtensa_insnbuf insn)
{
  return ((insn[0] & 0x10) >> 4);
}

void
set_sae4_field (xtensa_insnbuf insn, uint32 val)
{
  insn[0] = (insn[0] & 0xffffffef) | ((val << 4) & 0x10);
}

uint32
get_sal_field (const xtensa_insnbuf insn)
{
  return ((insn[0] & 0xf0000) >> 16) |
         ((insn[0] & 0x1) << 4);
}

void
set_sal_field (xtensa_insnbuf insn, uint32 val)
{
  insn[0] = (insn[0] & 0xfff0ffff) | ((val << 16) & 0xf0000);
  insn[0] = (insn[0] & 0xfffffffe) | ((val >> 4) & 0x1);
}

uint32
get_sar_field (const xtensa_insnbuf insn)
{
  return ((insn[0] & 0xf000) >> 12) |
         ((insn[0] & 0x1) << 4);
}

void
set_sar_field (xtensa_insnbuf insn, uint32 val)
{
  insn[0] = (insn[0] & 0xffff0fff) | ((val << 12) & 0xf000);
  insn[0] = (insn[0] & 0xfffffffe) | ((val >> 4) & 0x1);
}

uint32
get_sas_field (const xtensa_insnbuf insn)
{
  return ((insn[0] & 0xf000) >> 12) |
         ((insn[0] & 0x10000) >> 12);
}

void
set_sas_field (xtensa_insnbuf insn, uint32 val)
{
  insn[0] = (insn[0] & 0xffff0fff) | ((val << 12) & 0xf000);
  insn[0] = (insn[0] & 0xfffeffff) | ((val << 12) & 0x10000);
}

uint32
get_sas4_field (const xtensa_insnbuf insn)
{
  return ((insn[0] & 0x10000) >> 16);
}

void
set_sas4_field (xtensa_insnbuf insn, uint32 val)
{
  insn[0] = (insn[0] & 0xfffeffff) | ((val << 16) & 0x10000);
}

uint32
get_sr_field (const xtensa_insnbuf insn)
{
  return ((insn[0] & 0xf00) >> 8) |
         ((insn[0] & 0xf000) >> 8);
}

void
set_sr_field (xtensa_insnbuf insn, uint32 val)
{
  insn[0] = (insn[0] & 0xfffff0ff) | ((val << 8) & 0xf00);
  insn[0] = (insn[0] & 0xffff0fff) | ((val << 8) & 0xf000);
}

uint32
get_t_field (const xtensa_insnbuf insn)
{
  return ((insn[0] & 0xf0000) >> 16);
}

void
set_t_field (xtensa_insnbuf insn, uint32 val)
{
  insn[0] = (insn[0] & 0xfff0ffff) | ((val << 16) & 0xf0000);
}

uint32
get_thi3_field (const xtensa_insnbuf insn)
{
  return ((insn[0] & 0xe0000) >> 17);
}

void
set_thi3_field (xtensa_insnbuf insn, uint32 val)
{
  insn[0] = (insn[0] & 0xfff1ffff) | ((val << 17) & 0xe0000);
}

uint32
get_z_field (const xtensa_insnbuf insn)
{
  return ((insn[0] & 0x40000) >> 18);
}

void
set_z_field (xtensa_insnbuf insn, uint32 val)
{
  insn[0] = (insn[0] & 0xfffbffff) | ((val << 18) & 0x40000);
}

uint32 decode_b4constu (uint32);
xtensa_encode_result encode_b4constu (uint32 *);
uint32 decode_simm8x256 (uint32);
xtensa_encode_result encode_simm8x256 (uint32 *);
uint32 decode_soffset (uint32);
xtensa_encode_result encode_soffset (uint32 *);
uint32 decode_imm4 (uint32);
xtensa_encode_result encode_imm4 (uint32 *);
uint32 decode_op0 (uint32);
xtensa_encode_result encode_op0 (uint32 *);
uint32 decode_op1 (uint32);
xtensa_encode_result encode_op1 (uint32 *);
uint32 decode_imm6 (uint32);
xtensa_encode_result encode_imm6 (uint32 *);
uint32 decode_op2 (uint32);
xtensa_encode_result encode_op2 (uint32 *);
uint32 decode_imm7 (uint32);
xtensa_encode_result encode_imm7 (uint32 *);
uint32 decode_simm4 (uint32);
xtensa_encode_result encode_simm4 (uint32 *);
uint32 decode_ai4const (uint32);
xtensa_encode_result encode_ai4const (uint32 *);
uint32 decode_imm8 (uint32);
xtensa_encode_result encode_imm8 (uint32 *);
uint32 decode_sae (uint32);
xtensa_encode_result encode_sae (uint32 *);
uint32 decode_imm7lo (uint32);
xtensa_encode_result encode_imm7lo (uint32 *);
uint32 decode_simm7 (uint32);
xtensa_encode_result encode_simm7 (uint32 *);
uint32 decode_simm8 (uint32);
xtensa_encode_result encode_simm8 (uint32 *);
uint32 decode_uimm12x8 (uint32);
xtensa_encode_result encode_uimm12x8 (uint32 *);
uint32 decode_sal (uint32);
xtensa_encode_result encode_sal (uint32 *);
uint32 decode_uimm6 (uint32);
xtensa_encode_result encode_uimm6 (uint32 *);
uint32 decode_sas4 (uint32);
xtensa_encode_result encode_sas4 (uint32 *);
uint32 decode_uimm8 (uint32);
xtensa_encode_result encode_uimm8 (uint32 *);
uint32 decode_uimm16x4 (uint32);
xtensa_encode_result encode_uimm16x4 (uint32 *);
uint32 decode_sar (uint32);
xtensa_encode_result encode_sar (uint32 *);
uint32 decode_sa4 (uint32);
xtensa_encode_result encode_sa4 (uint32 *);
uint32 decode_sas (uint32);
xtensa_encode_result encode_sas (uint32 *);
uint32 decode_imm6hi (uint32);
xtensa_encode_result encode_imm6hi (uint32 *);
uint32 decode_bbi (uint32);
xtensa_encode_result encode_bbi (uint32 *);
uint32 decode_uimm8x2 (uint32);
xtensa_encode_result encode_uimm8x2 (uint32 *);
uint32 decode_uimm8x4 (uint32);
xtensa_encode_result encode_uimm8x4 (uint32 *);
uint32 decode_msalp32 (uint32);
xtensa_encode_result encode_msalp32 (uint32 *);
uint32 decode_bbi4 (uint32);
xtensa_encode_result encode_bbi4 (uint32 *);
uint32 decode_op2p1 (uint32);
xtensa_encode_result encode_op2p1 (uint32 *);
uint32 decode_soffsetx4 (uint32);
xtensa_encode_result encode_soffsetx4 (uint32 *);
uint32 decode_imm6lo (uint32);
xtensa_encode_result encode_imm6lo (uint32 *);
uint32 decode_imm12 (uint32);
xtensa_encode_result encode_imm12 (uint32 *);
uint32 decode_b4const (uint32);
xtensa_encode_result encode_b4const (uint32 *);
uint32 decode_i (uint32);
xtensa_encode_result encode_i (uint32 *);
uint32 decode_imm16 (uint32);
xtensa_encode_result encode_imm16 (uint32 *);
uint32 decode_mn (uint32);
xtensa_encode_result encode_mn (uint32 *);
uint32 decode_m (uint32);
xtensa_encode_result encode_m (uint32 *);
uint32 decode_n (uint32);
xtensa_encode_result encode_n (uint32 *);
uint32 decode_none (uint32);
xtensa_encode_result encode_none (uint32 *);
uint32 decode_imm12b (uint32);
xtensa_encode_result encode_imm12b (uint32 *);
uint32 decode_r (uint32);
xtensa_encode_result encode_r (uint32 *);
uint32 decode_s (uint32);
xtensa_encode_result encode_s (uint32 *);
uint32 decode_t (uint32);
xtensa_encode_result encode_t (uint32 *);
uint32 decode_thi3 (uint32);
xtensa_encode_result encode_thi3 (uint32 *);
uint32 decode_sae4 (uint32);
xtensa_encode_result encode_sae4 (uint32 *);
uint32 decode_offset (uint32);
xtensa_encode_result encode_offset (uint32 *);
uint32 decode_imm7hi (uint32);
xtensa_encode_result encode_imm7hi (uint32 *);
uint32 decode_uimm4x16 (uint32);
xtensa_encode_result encode_uimm4x16 (uint32 *);
uint32 decode_simm12b (uint32);
xtensa_encode_result encode_simm12b (uint32 *);
uint32 decode_lsi4x4 (uint32);
xtensa_encode_result encode_lsi4x4 (uint32 *);
uint32 decode_z (uint32);
xtensa_encode_result encode_z (uint32 *);
uint32 decode_simm12 (uint32);
xtensa_encode_result encode_simm12 (uint32 *);
uint32 decode_sr (uint32);
xtensa_encode_result encode_sr (uint32 *);
uint32 decode_nimm4x2 (uint32);
xtensa_encode_result encode_nimm4x2 (uint32 *);


static const uint32 b4constu_table[] = {
  32768,
  65536,
  2,
  3,
  4,
  5,
  6,
  7,
  8,
  10,
  12,
  16,
  32,
  64,
  128,
  256
};

uint32
decode_b4constu (uint32 val)
{
  val = b4constu_table[val];
  return val;
}

xtensa_encode_result
encode_b4constu (uint32 *valp)
{
  uint32 val = *valp;
  unsigned i;
  for (i = 0; i < (1 << 4); i += 1)
    if (b4constu_table[i] == val) goto found;
  return xtensa_encode_result_not_in_table;
 found:
  val = i;
  *valp = val;
  return xtensa_encode_result_ok;
}

uint32
decode_simm8x256 (uint32 val)
{
  val = (val ^ 0x80) - 0x80;
  val <<= 8;
  return val;
}

xtensa_encode_result
encode_simm8x256 (uint32 *valp)
{
  uint32 val = *valp;
  if ((val & ((1 << 8) - 1)) != 0)
    return xtensa_encode_result_align;
  val = (signed int) val >> 8;
  if (((val + (1 << 7)) >> 8) != 0)
    {
      if ((signed int) val > 0)
        return xtensa_encode_result_too_high;
      else
        return xtensa_encode_result_too_low;
    }
  *valp = val;
  return xtensa_encode_result_ok;
}

uint32
decode_soffset (uint32 val)
{
  val = (val ^ 0x20000) - 0x20000;
  return val;
}

xtensa_encode_result
encode_soffset (uint32 *valp)
{
  uint32 val = *valp;
  if (((val + (1 << 17)) >> 18) != 0)
    {
      if ((signed int) val > 0)
        return xtensa_encode_result_too_high;
      else
        return xtensa_encode_result_too_low;
    }
  *valp = val;
  return xtensa_encode_result_ok;
}

uint32
decode_imm4 (uint32 val)
{
  return val;
}

xtensa_encode_result
encode_imm4 (uint32 *valp)
{
  uint32 val = *valp;
  if ((val >> 4) != 0)
    return xtensa_encode_result_too_high;
  *valp = val;
  return xtensa_encode_result_ok;
}

uint32
decode_op0 (uint32 val)
{
  return val;
}

xtensa_encode_result
encode_op0 (uint32 *valp)
{
  uint32 val = *valp;
  if ((val >> 4) != 0)
    return xtensa_encode_result_too_high;
  *valp = val;
  return xtensa_encode_result_ok;
}

uint32
decode_op1 (uint32 val)
{
  return val;
}

xtensa_encode_result
encode_op1 (uint32 *valp)
{
  uint32 val = *valp;
  if ((val >> 4) != 0)
    return xtensa_encode_result_too_high;
  *valp = val;
  return xtensa_encode_result_ok;
}

uint32
decode_imm6 (uint32 val)
{
  return val;
}

xtensa_encode_result
encode_imm6 (uint32 *valp)
{
  uint32 val = *valp;
  if ((val >> 6) != 0)
    return xtensa_encode_result_too_high;
  *valp = val;
  return xtensa_encode_result_ok;
}

uint32
decode_op2 (uint32 val)
{
  return val;
}

xtensa_encode_result
encode_op2 (uint32 *valp)
{
  uint32 val = *valp;
  if ((val >> 4) != 0)
    return xtensa_encode_result_too_high;
  *valp = val;
  return xtensa_encode_result_ok;
}

uint32
decode_imm7 (uint32 val)
{
  return val;
}

xtensa_encode_result
encode_imm7 (uint32 *valp)
{
  uint32 val = *valp;
  if ((val >> 7) != 0)
    return xtensa_encode_result_too_high;
  *valp = val;
  return xtensa_encode_result_ok;
}

uint32
decode_simm4 (uint32 val)
{
  val = (val ^ 0x8) - 0x8;
  return val;
}

xtensa_encode_result
encode_simm4 (uint32 *valp)
{
  uint32 val = *valp;
  if (((val + (1 << 3)) >> 4) != 0)
    {
      if ((signed int) val > 0)
        return xtensa_encode_result_too_high;
      else
        return xtensa_encode_result_too_low;
    }
  *valp = val;
  return xtensa_encode_result_ok;
}

static const uint32 ai4const_table[] = {
  -1,
  1,
  2,
  3,
  4,
  5,
  6,
  7,
  8,
  9,
  10,
  11,
  12,
  13,
  14,
  15
};

uint32
decode_ai4const (uint32 val)
{
  val = ai4const_table[val];
  return val;
}

xtensa_encode_result
encode_ai4const (uint32 *valp)
{
  uint32 val = *valp;
  unsigned i;
  for (i = 0; i < (1 << 4); i += 1)
    if (ai4const_table[i] == val) goto found;
  return xtensa_encode_result_not_in_table;
 found:
  val = i;
  *valp = val;
  return xtensa_encode_result_ok;
}

uint32
decode_imm8 (uint32 val)
{
  return val;
}

xtensa_encode_result
encode_imm8 (uint32 *valp)
{
  uint32 val = *valp;
  if ((val >> 8) != 0)
    return xtensa_encode_result_too_high;
  *valp = val;
  return xtensa_encode_result_ok;
}

uint32
decode_sae (uint32 val)
{
  return val;
}

xtensa_encode_result
encode_sae (uint32 *valp)
{
  uint32 val = *valp;
  if ((val >> 5) != 0)
    return xtensa_encode_result_too_high;
  *valp = val;
  return xtensa_encode_result_ok;
}

uint32
decode_imm7lo (uint32 val)
{
  return val;
}

xtensa_encode_result
encode_imm7lo (uint32 *valp)
{
  uint32 val = *valp;
  if ((val >> 4) != 0)
    return xtensa_encode_result_too_high;
  *valp = val;
  return xtensa_encode_result_ok;
}

uint32
decode_simm7 (uint32 val)
{
  if (val > 95)
      val |= -32;
  return val;
}

xtensa_encode_result
encode_simm7 (uint32 *valp)
{
  uint32 val = *valp;
  if ((signed int) val < -32)
    return xtensa_encode_result_too_low;
  if ((signed int) val > 95)
    return xtensa_encode_result_too_high;
  *valp = val;
  return xtensa_encode_result_ok;
}

uint32
decode_simm8 (uint32 val)
{
  val = (val ^ 0x80) - 0x80;
  return val;
}

xtensa_encode_result
encode_simm8 (uint32 *valp)
{
  uint32 val = *valp;
  if (((val + (1 << 7)) >> 8) != 0)
    {
      if ((signed int) val > 0)
        return xtensa_encode_result_too_high;
      else
        return xtensa_encode_result_too_low;
    }
  *valp = val;
  return xtensa_encode_result_ok;
}

uint32
decode_uimm12x8 (uint32 val)
{
  val <<= 3;
  return val;
}

xtensa_encode_result
encode_uimm12x8 (uint32 *valp)
{
  uint32 val = *valp;
  if ((val & ((1 << 3) - 1)) != 0)
    return xtensa_encode_result_align;
  val = (signed int) val >> 3;
  if ((val >> 12) != 0)
    return xtensa_encode_result_too_high;
  *valp = val;
  return xtensa_encode_result_ok;
}

uint32
decode_sal (uint32 val)
{
  return val;
}

xtensa_encode_result
encode_sal (uint32 *valp)
{
  uint32 val = *valp;
  if ((val >> 5) != 0)
    return xtensa_encode_result_too_high;
  *valp = val;
  return xtensa_encode_result_ok;
}

uint32
decode_uimm6 (uint32 val)
{
  return val;
}

xtensa_encode_result
encode_uimm6 (uint32 *valp)
{
  uint32 val = *valp;
  if ((val >> 6) != 0)
    return xtensa_encode_result_too_high;
  *valp = val;
  return xtensa_encode_result_ok;
}

uint32
decode_sas4 (uint32 val)
{
  return val;
}

xtensa_encode_result
encode_sas4 (uint32 *valp)
{
  uint32 val = *valp;
  if ((val >> 1) != 0)
    return xtensa_encode_result_too_high;
  *valp = val;
  return xtensa_encode_result_ok;
}

uint32
decode_uimm8 (uint32 val)
{
  return val;
}

xtensa_encode_result
encode_uimm8 (uint32 *valp)
{
  uint32 val = *valp;
  if ((val >> 8) != 0)
    return xtensa_encode_result_too_high;
  *valp = val;
  return xtensa_encode_result_ok;
}

uint32
decode_uimm16x4 (uint32 val)
{
  val |= -1 << 16;
  val <<= 2;
  return val;
}

xtensa_encode_result
encode_uimm16x4 (uint32 *valp)
{
  uint32 val = *valp;
  if ((val & ((1 << 2) - 1)) != 0)
    return xtensa_encode_result_align;
  val = (signed int) val >> 2;
  if ((signed int) val >> 16 != -1)
    {
      if ((signed int) val >= 0)
        return xtensa_encode_result_too_high;
      else
        return xtensa_encode_result_too_low;
    }
  *valp = val;
  return xtensa_encode_result_ok;
}

uint32
decode_sar (uint32 val)
{
  return val;
}

xtensa_encode_result
encode_sar (uint32 *valp)
{
  uint32 val = *valp;
  if ((val >> 5) != 0)
    return xtensa_encode_result_too_high;
  *valp = val;
  return xtensa_encode_result_ok;
}

uint32
decode_sa4 (uint32 val)
{
  return val;
}

xtensa_encode_result
encode_sa4 (uint32 *valp)
{
  uint32 val = *valp;
  if ((val >> 1) != 0)
    return xtensa_encode_result_too_high;
  *valp = val;
  return xtensa_encode_result_ok;
}

uint32
decode_sas (uint32 val)
{
  return val;
}

xtensa_encode_result
encode_sas (uint32 *valp)
{
  uint32 val = *valp;
  if ((val >> 5) != 0)
    return xtensa_encode_result_too_high;
  *valp = val;
  return xtensa_encode_result_ok;
}

uint32
decode_imm6hi (uint32 val)
{
  return val;
}

xtensa_encode_result
encode_imm6hi (uint32 *valp)
{
  uint32 val = *valp;
  if ((val >> 2) != 0)
    return xtensa_encode_result_too_high;
  *valp = val;
  return xtensa_encode_result_ok;
}

uint32
decode_bbi (uint32 val)
{
  return val;
}

xtensa_encode_result
encode_bbi (uint32 *valp)
{
  uint32 val = *valp;
  if ((val >> 5) != 0)
    return xtensa_encode_result_too_high;
  *valp = val;
  return xtensa_encode_result_ok;
}

uint32
decode_uimm8x2 (uint32 val)
{
  val <<= 1;
  return val;
}

xtensa_encode_result
encode_uimm8x2 (uint32 *valp)
{
  uint32 val = *valp;
  if ((val & ((1 << 1) - 1)) != 0)
    return xtensa_encode_result_align;
  val = (signed int) val >> 1;
  if ((val >> 8) != 0)
    return xtensa_encode_result_too_high;
  *valp = val;
  return xtensa_encode_result_ok;
}

uint32
decode_uimm8x4 (uint32 val)
{
  val <<= 2;
  return val;
}

xtensa_encode_result
encode_uimm8x4 (uint32 *valp)
{
  uint32 val = *valp;
  if ((val & ((1 << 2) - 1)) != 0)
    return xtensa_encode_result_align;
  val = (signed int) val >> 2;
  if ((val >> 8) != 0)
    return xtensa_encode_result_too_high;
  *valp = val;
  return xtensa_encode_result_ok;
}

static const uint32 mip32const_table[] = {
  32,
  31,
  30,
  29,
  28,
  27,
  26,
  25,
  24,
  23,
  22,
  21,
  20,
  19,
  18,
  17,
  16,
  15,
  14,
  13,
  12,
  11,
  10,
  9,
  8,
  7,
  6,
  5,
  4,
  3,
  2,
  1
};

uint32
decode_msalp32 (uint32 val)
{
  val = mip32const_table[val];
  return val;
}

xtensa_encode_result
encode_msalp32 (uint32 *valp)
{
  uint32 val = *valp;
  unsigned i;
  for (i = 0; i < (1 << 5); i += 1)
    if (mip32const_table[i] == val) goto found;
  return xtensa_encode_result_not_in_table;
 found:
  val = i;
  *valp = val;
  return xtensa_encode_result_ok;
}

uint32
decode_bbi4 (uint32 val)
{
  return val;
}

xtensa_encode_result
encode_bbi4 (uint32 *valp)
{
  uint32 val = *valp;
  if ((val >> 1) != 0)
    return xtensa_encode_result_too_high;
  *valp = val;
  return xtensa_encode_result_ok;
}

static const uint32 i4p1const_table[] = {
  1,
  2,
  3,
  4,
  5,
  6,
  7,
  8,
  9,
  10,
  11,
  12,
  13,
  14,
  15,
  16
};

uint32
decode_op2p1 (uint32 val)
{
  val = i4p1const_table[val];
  return val;
}

xtensa_encode_result
encode_op2p1 (uint32 *valp)
{
  uint32 val = *valp;
  unsigned i;
  for (i = 0; i < (1 << 4); i += 1)
    if (i4p1const_table[i] == val) goto found;
  return xtensa_encode_result_not_in_table;
 found:
  val = i;
  *valp = val;
  return xtensa_encode_result_ok;
}

uint32
decode_soffsetx4 (uint32 val)
{
  val = (val ^ 0x20000) - 0x20000;
  val <<= 2;
  return val;
}

xtensa_encode_result
encode_soffsetx4 (uint32 *valp)
{
  uint32 val = *valp;
  if ((val & ((1 << 2) - 1)) != 0)
    return xtensa_encode_result_align;
  val = (signed int) val >> 2;
  if (((val + (1 << 17)) >> 18) != 0)
    {
      if ((signed int) val > 0)
        return xtensa_encode_result_too_high;
      else
        return xtensa_encode_result_too_low;
    }
  *valp = val;
  return xtensa_encode_result_ok;
}

uint32
decode_imm6lo (uint32 val)
{
  return val;
}

xtensa_encode_result
encode_imm6lo (uint32 *valp)
{
  uint32 val = *valp;
  if ((val >> 4) != 0)
    return xtensa_encode_result_too_high;
  *valp = val;
  return xtensa_encode_result_ok;
}

uint32
decode_imm12 (uint32 val)
{
  return val;
}

xtensa_encode_result
encode_imm12 (uint32 *valp)
{
  uint32 val = *valp;
  if ((val >> 12) != 0)
    return xtensa_encode_result_too_high;
  *valp = val;
  return xtensa_encode_result_ok;
}

static const uint32 b4const_table[] = {
  -1,
  1,
  2,
  3,
  4,
  5,
  6,
  7,
  8,
  10,
  12,
  16,
  32,
  64,
  128,
  256
};

uint32
decode_b4const (uint32 val)
{
  val = b4const_table[val];
  return val;
}

xtensa_encode_result
encode_b4const (uint32 *valp)
{
  uint32 val = *valp;
  unsigned i;
  for (i = 0; i < (1 << 4); i += 1)
    if (b4const_table[i] == val) goto found;
  return xtensa_encode_result_not_in_table;
 found:
  val = i;
  *valp = val;
  return xtensa_encode_result_ok;
}

uint32
decode_i (uint32 val)
{
  return val;
}

xtensa_encode_result
encode_i (uint32 *valp)
{
  uint32 val = *valp;
  if ((val >> 1) != 0)
    return xtensa_encode_result_too_high;
  *valp = val;
  return xtensa_encode_result_ok;
}

uint32
decode_imm16 (uint32 val)
{
  return val;
}

xtensa_encode_result
encode_imm16 (uint32 *valp)
{
  uint32 val = *valp;
  if ((val >> 16) != 0)
    return xtensa_encode_result_too_high;
  *valp = val;
  return xtensa_encode_result_ok;
}

uint32
decode_mn (uint32 val)
{
  return val;
}

xtensa_encode_result
encode_mn (uint32 *valp)
{
  uint32 val = *valp;
  if ((val >> 4) != 0)
    return xtensa_encode_result_too_high;
  *valp = val;
  return xtensa_encode_result_ok;
}

uint32
decode_m (uint32 val)
{
  return val;
}

xtensa_encode_result
encode_m (uint32 *valp)
{
  uint32 val = *valp;
  if ((val >> 2) != 0)
    return xtensa_encode_result_too_high;
  *valp = val;
  return xtensa_encode_result_ok;
}

uint32
decode_n (uint32 val)
{
  return val;
}

xtensa_encode_result
encode_n (uint32 *valp)
{
  uint32 val = *valp;
  if ((val >> 2) != 0)
    return xtensa_encode_result_too_high;
  *valp = val;
  return xtensa_encode_result_ok;
}

uint32
decode_none (uint32 val)
{
  return val;
}

xtensa_encode_result
encode_none (uint32 *valp)
{
  uint32 val = *valp;
  if ((val >> 0) != 0)
    return xtensa_encode_result_too_high;
  *valp = val;
  return xtensa_encode_result_ok;
}

uint32
decode_imm12b (uint32 val)
{
  return val;
}

xtensa_encode_result
encode_imm12b (uint32 *valp)
{
  uint32 val = *valp;
  if ((val >> 12) != 0)
    return xtensa_encode_result_too_high;
  *valp = val;
  return xtensa_encode_result_ok;
}

uint32
decode_r (uint32 val)
{
  return val;
}

xtensa_encode_result
encode_r (uint32 *valp)
{
  uint32 val = *valp;
  if ((val >> 4) != 0)
    return xtensa_encode_result_too_high;
  *valp = val;
  return xtensa_encode_result_ok;
}

uint32
decode_s (uint32 val)
{
  return val;
}

xtensa_encode_result
encode_s (uint32 *valp)
{
  uint32 val = *valp;
  if ((val >> 4) != 0)
    return xtensa_encode_result_too_high;
  *valp = val;
  return xtensa_encode_result_ok;
}

uint32
decode_t (uint32 val)
{
  return val;
}

xtensa_encode_result
encode_t (uint32 *valp)
{
  uint32 val = *valp;
  if ((val >> 4) != 0)
    return xtensa_encode_result_too_high;
  *valp = val;
  return xtensa_encode_result_ok;
}

uint32
decode_thi3 (uint32 val)
{
  return val;
}

xtensa_encode_result
encode_thi3 (uint32 *valp)
{
  uint32 val = *valp;
  if ((val >> 3) != 0)
    return xtensa_encode_result_too_high;
  *valp = val;
  return xtensa_encode_result_ok;
}

uint32
decode_sae4 (uint32 val)
{
  return val;
}

xtensa_encode_result
encode_sae4 (uint32 *valp)
{
  uint32 val = *valp;
  if ((val >> 1) != 0)
    return xtensa_encode_result_too_high;
  *valp = val;
  return xtensa_encode_result_ok;
}

uint32
decode_offset (uint32 val)
{
  return val;
}

xtensa_encode_result
encode_offset (uint32 *valp)
{
  uint32 val = *valp;
  if ((val >> 18) != 0)
    return xtensa_encode_result_too_high;
  *valp = val;
  return xtensa_encode_result_ok;
}

uint32
decode_imm7hi (uint32 val)
{
  return val;
}

xtensa_encode_result
encode_imm7hi (uint32 *valp)
{
  uint32 val = *valp;
  if ((val >> 3) != 0)
    return xtensa_encode_result_too_high;
  *valp = val;
  return xtensa_encode_result_ok;
}

uint32
decode_uimm4x16 (uint32 val)
{
  val <<= 4;
  return val;
}

xtensa_encode_result
encode_uimm4x16 (uint32 *valp)
{
  uint32 val = *valp;
  if ((val & ((1 << 4) - 1)) != 0)
    return xtensa_encode_result_align;
  val = (signed int) val >> 4;
  if ((val >> 4) != 0)
    return xtensa_encode_result_too_high;
  *valp = val;
  return xtensa_encode_result_ok;
}

uint32
decode_simm12b (uint32 val)
{
  val = (val ^ 0x800) - 0x800;
  return val;
}

xtensa_encode_result
encode_simm12b (uint32 *valp)
{
  uint32 val = *valp;
  if (((val + (1 << 11)) >> 12) != 0)
    {
      if ((signed int) val > 0)
        return xtensa_encode_result_too_high;
      else
        return xtensa_encode_result_too_low;
    }
  *valp = val;
  return xtensa_encode_result_ok;
}

uint32
decode_lsi4x4 (uint32 val)
{
  val <<= 2;
  return val;
}

xtensa_encode_result
encode_lsi4x4 (uint32 *valp)
{
  uint32 val = *valp;
  if ((val & ((1 << 2) - 1)) != 0)
    return xtensa_encode_result_align;
  val = (signed int) val >> 2;
  if ((val >> 4) != 0)
    return xtensa_encode_result_too_high;
  *valp = val;
  return xtensa_encode_result_ok;
}

uint32
decode_z (uint32 val)
{
  return val;
}

xtensa_encode_result
encode_z (uint32 *valp)
{
  uint32 val = *valp;
  if ((val >> 1) != 0)
    return xtensa_encode_result_too_high;
  *valp = val;
  return xtensa_encode_result_ok;
}

uint32
decode_simm12 (uint32 val)
{
  val = (val ^ 0x800) - 0x800;
  return val;
}

xtensa_encode_result
encode_simm12 (uint32 *valp)
{
  uint32 val = *valp;
  if (((val + (1 << 11)) >> 12) != 0)
    {
      if ((signed int) val > 0)
        return xtensa_encode_result_too_high;
      else
        return xtensa_encode_result_too_low;
    }
  *valp = val;
  return xtensa_encode_result_ok;
}

uint32
decode_sr (uint32 val)
{
  return val;
}

xtensa_encode_result
encode_sr (uint32 *valp)
{
  uint32 val = *valp;
  if ((val >> 8) != 0)
    return xtensa_encode_result_too_high;
  *valp = val;
  return xtensa_encode_result_ok;
}

uint32
decode_nimm4x2 (uint32 val)
{
  val |= -1 << 4;
  val <<= 2;
  return val;
}

xtensa_encode_result
encode_nimm4x2 (uint32 *valp)
{
  uint32 val = *valp;
  if ((val & ((1 << 2) - 1)) != 0)
    return xtensa_encode_result_align;
  val = (signed int) val >> 2;
  if ((signed int) val >> 4 != -1)
    {
      if ((signed int) val >= 0)
        return xtensa_encode_result_too_high;
      else
        return xtensa_encode_result_too_low;
    }
  *valp = val;
  return xtensa_encode_result_ok;
}



uint32 do_reloc_l (uint32, uint32);
uint32 undo_reloc_l (uint32, uint32);
uint32 do_reloc_L (uint32, uint32);
uint32 undo_reloc_L (uint32, uint32);
uint32 do_reloc_r (uint32, uint32);
uint32 undo_reloc_r (uint32, uint32);


uint32
do_reloc_l (uint32 addr, uint32 pc)
{
  return addr - pc - 4;
}

uint32
undo_reloc_l (uint32 offset, uint32 pc)
{
  return pc + offset + 4;
}

uint32
do_reloc_L (uint32 addr, uint32 pc)
{
  return addr - (pc & -4) - 4;
}

uint32
undo_reloc_L (uint32 offset, uint32 pc)
{
  return (pc & -4) + offset + 4;
}

uint32
do_reloc_r (uint32 addr, uint32 pc)
{
  return addr - ((pc+3) & -4);
}

uint32
undo_reloc_r (uint32 offset, uint32 pc)
{
  return ((pc+3) & -4) + offset;
}

static xtensa_operand_internal iib4const_operand = {
  "i",
  '<',
  0,
  get_r_field,
  set_r_field,
  encode_b4const,
  decode_b4const,
  0,
  0
};

static xtensa_operand_internal iiuimm8_operand = {
  "i",
  '<',
  0,
  get_imm8_field,
  set_imm8_field,
  encode_uimm8,
  decode_uimm8,
  0,
  0
};

static xtensa_operand_internal lisoffsetx4_operand = {
  "L",
  '<',
  1,
  get_offset_field,
  set_offset_field,
  encode_soffsetx4,
  decode_soffsetx4,
  do_reloc_L,
  undo_reloc_L,
};

static xtensa_operand_internal iisimm8x256_operand = {
  "i",
  '<',
  0,
  get_imm8_field,
  set_imm8_field,
  encode_simm8x256,
  decode_simm8x256,
  0,
  0
};

static xtensa_operand_internal lisimm12_operand = {
  "l",
  '<',
  1,
  get_imm12_field,
  set_imm12_field,
  encode_simm12,
  decode_simm12,
  do_reloc_l,
  undo_reloc_l,
};

static xtensa_operand_internal iiop2p1_operand = {
  "i",
  '<',
  0,
  get_op2_field,
  set_op2_field,
  encode_op2p1,
  decode_op2p1,
  0,
  0
};

static xtensa_operand_internal iisae_operand = {
  "i",
  '<',
  0,
  get_sae_field,
  set_sae_field,
  encode_sae,
  decode_sae,
  0,
  0
};

static xtensa_operand_internal iis_operand = {
  "i",
  '<',
  0,
  get_s_field,
  set_s_field,
  encode_s,
  decode_s,
  0,
  0
};

static xtensa_operand_internal iit_operand = {
  "i",
  '<',
  0,
  get_t_field,
  set_t_field,
  encode_t,
  decode_t,
  0,
  0
};

static xtensa_operand_internal iisimm12b_operand = {
  "i",
  '<',
  0,
  get_imm12b_field,
  set_imm12b_field,
  encode_simm12b,
  decode_simm12b,
  0,
  0
};

static xtensa_operand_internal iinimm4x2_operand = {
  "i",
  '<',
  0,
  get_imm4_field,
  set_imm4_field,
  encode_nimm4x2,
  decode_nimm4x2,
  0,
  0
};

static xtensa_operand_internal iiuimm4x16_operand = {
  "i",
  '<',
  0,
  get_op2_field,
  set_op2_field,
  encode_uimm4x16,
  decode_uimm4x16,
  0,
  0
};

static xtensa_operand_internal abs_operand = {
  "a",
  '=',
  0,
  get_s_field,
  set_s_field,
  encode_s,
  decode_s,
  0,
  0
};

static xtensa_operand_internal iisar_operand = {
  "i",
  '<',
  0,
  get_sar_field,
  set_sar_field,
  encode_sar,
  decode_sar,
  0,
  0
};

static xtensa_operand_internal abt_operand = {
  "a",
  '=',
  0,
  get_t_field,
  set_t_field,
  encode_t,
  decode_t,
  0,
  0
};

static xtensa_operand_internal iisas_operand = {
  "i",
  '<',
  0,
  get_sas_field,
  set_sas_field,
  encode_sas,
  decode_sas,
  0,
  0
};

static xtensa_operand_internal amr_operand = {
  "a",
  '=',
  0,
  get_r_field,
  set_r_field,
  encode_r,
  decode_r,
  0,
  0
};

static xtensa_operand_internal iib4constu_operand = {
  "i",
  '<',
  0,
  get_r_field,
  set_r_field,
  encode_b4constu,
  decode_b4constu,
  0,
  0
};

static xtensa_operand_internal iisr_operand = {
  "i",
  '<',
  0,
  get_sr_field,
  set_sr_field,
  encode_sr,
  decode_sr,
  0,
  0
};

static xtensa_operand_internal iibbi_operand = {
  "i",
  '<',
  0,
  get_bbi_field,
  set_bbi_field,
  encode_bbi,
  decode_bbi,
  0,
  0
};

static xtensa_operand_internal iiai4const_operand = {
  "i",
  '<',
  0,
  get_t_field,
  set_t_field,
  encode_ai4const,
  decode_ai4const,
  0,
  0
};

static xtensa_operand_internal iiuimm12x8_operand = {
  "i",
  '<',
  0,
  get_imm12_field,
  set_imm12_field,
  encode_uimm12x8,
  decode_uimm12x8,
  0,
  0
};

static xtensa_operand_internal riuimm16x4_operand = {
  "r",
  '<',
  1,
  get_imm16_field,
  set_imm16_field,
  encode_uimm16x4,
  decode_uimm16x4,
  do_reloc_r,
  undo_reloc_r,
};

static xtensa_operand_internal lisimm8_operand = {
  "l",
  '<',
  1,
  get_imm8_field,
  set_imm8_field,
  encode_simm8,
  decode_simm8,
  do_reloc_l,
  undo_reloc_l,
};

static xtensa_operand_internal iilsi4x4_operand = {
  "i",
  '<',
  0,
  get_r_field,
  set_r_field,
  encode_lsi4x4,
  decode_lsi4x4,
  0,
  0
};

static xtensa_operand_internal iiuimm8x2_operand = {
  "i",
  '<',
  0,
  get_imm8_field,
  set_imm8_field,
  encode_uimm8x2,
  decode_uimm8x2,
  0,
  0
};

static xtensa_operand_internal iisimm4_operand = {
  "i",
  '<',
  0,
  get_mn_field,
  set_mn_field,
  encode_simm4,
  decode_simm4,
  0,
  0
};

static xtensa_operand_internal iimsalp32_operand = {
  "i",
  '<',
  0,
  get_sal_field,
  set_sal_field,
  encode_msalp32,
  decode_msalp32,
  0,
  0
};

static xtensa_operand_internal liuimm6_operand = {
  "l",
  '<',
  1,
  get_imm6_field,
  set_imm6_field,
  encode_uimm6,
  decode_uimm6,
  do_reloc_l,
  undo_reloc_l,
};

static xtensa_operand_internal iiuimm8x4_operand = {
  "i",
  '<',
  0,
  get_imm8_field,
  set_imm8_field,
  encode_uimm8x4,
  decode_uimm8x4,
  0,
  0
};

static xtensa_operand_internal lisoffset_operand = {
  "l",
  '<',
  1,
  get_offset_field,
  set_offset_field,
  encode_soffset,
  decode_soffset,
  do_reloc_l,
  undo_reloc_l,
};

static xtensa_operand_internal iisimm7_operand = {
  "i",
  '<',
  0,
  get_imm7_field,
  set_imm7_field,
  encode_simm7,
  decode_simm7,
  0,
  0
};

static xtensa_operand_internal ais_operand = {
  "a",
  '<',
  0,
  get_s_field,
  set_s_field,
  encode_s,
  decode_s,
  0,
  0
};

static xtensa_operand_internal liuimm8_operand = {
  "l",
  '<',
  1,
  get_imm8_field,
  set_imm8_field,
  encode_uimm8,
  decode_uimm8,
  do_reloc_l,
  undo_reloc_l,
};

static xtensa_operand_internal ait_operand = {
  "a",
  '<',
  0,
  get_t_field,
  set_t_field,
  encode_t,
  decode_t,
  0,
  0
};

static xtensa_operand_internal iisimm8_operand = {
  "i",
  '<',
  0,
  get_imm8_field,
  set_imm8_field,
  encode_simm8,
  decode_simm8,
  0,
  0
};

static xtensa_operand_internal aor_operand = {
  "a",
  '>',
  0,
  get_r_field,
  set_r_field,
  encode_r,
  decode_r,
  0,
  0
};

static xtensa_operand_internal aos_operand = {
  "a",
  '>',
  0,
  get_s_field,
  set_s_field,
  encode_s,
  decode_s,
  0,
  0
};

static xtensa_operand_internal aot_operand = {
  "a",
  '>',
  0,
  get_t_field,
  set_t_field,
  encode_t,
  decode_t,
  0,
  0
};

static xtensa_iclass_internal nopn_iclass = {
  0,
  0
};

static xtensa_operand_internal *movi_operand_list[] = {
  &aot_operand,
  &iisimm12b_operand
};

static xtensa_iclass_internal movi_iclass = {
  2,
  &movi_operand_list[0]
};

static xtensa_operand_internal *bsi8u_operand_list[] = {
  &ais_operand,
  &iib4constu_operand,
  &lisimm8_operand
};

static xtensa_iclass_internal bsi8u_iclass = {
  3,
  &bsi8u_operand_list[0]
};

static xtensa_operand_internal *itlb_operand_list[] = {
  &ais_operand
};

static xtensa_iclass_internal itlb_iclass = {
  1,
  &itlb_operand_list[0]
};

static xtensa_operand_internal *shiftst_operand_list[] = {
  &aor_operand,
  &ais_operand,
  &ait_operand
};

static xtensa_iclass_internal shiftst_iclass = {
  3,
  &shiftst_operand_list[0]
};

static xtensa_operand_internal *l32r_operand_list[] = {
  &aot_operand,
  &riuimm16x4_operand
};

static xtensa_iclass_internal l32r_iclass = {
  2,
  &l32r_operand_list[0]
};

static xtensa_iclass_internal rfe_iclass = {
  0,
  0
};

static xtensa_operand_internal *wait_operand_list[] = {
  &iis_operand
};

static xtensa_iclass_internal wait_iclass = {
  1,
  &wait_operand_list[0]
};

static xtensa_operand_internal *rfi_operand_list[] = {
  &iis_operand
};

static xtensa_iclass_internal rfi_iclass = {
  1,
  &rfi_operand_list[0]
};

static xtensa_operand_internal *movz_operand_list[] = {
  &amr_operand,
  &ais_operand,
  &ait_operand
};

static xtensa_iclass_internal movz_iclass = {
  3,
  &movz_operand_list[0]
};

static xtensa_operand_internal *callx_operand_list[] = {
  &ais_operand
};

static xtensa_iclass_internal callx_iclass = {
  1,
  &callx_operand_list[0]
};

static xtensa_operand_internal *mov_n_operand_list[] = {
  &aot_operand,
  &ais_operand
};

static xtensa_iclass_internal mov_n_iclass = {
  2,
  &mov_n_operand_list[0]
};

static xtensa_operand_internal *loadi4_operand_list[] = {
  &aot_operand,
  &ais_operand,
  &iilsi4x4_operand
};

static xtensa_iclass_internal loadi4_iclass = {
  3,
  &loadi4_operand_list[0]
};

static xtensa_operand_internal *exti_operand_list[] = {
  &aor_operand,
  &ait_operand,
  &iisae_operand,
  &iiop2p1_operand
};

static xtensa_iclass_internal exti_iclass = {
  4,
  &exti_operand_list[0]
};

static xtensa_operand_internal *break_operand_list[] = {
  &iis_operand,
  &iit_operand
};

static xtensa_iclass_internal break_iclass = {
  2,
  &break_operand_list[0]
};

static xtensa_operand_internal *slli_operand_list[] = {
  &aor_operand,
  &ais_operand,
  &iimsalp32_operand
};

static xtensa_iclass_internal slli_iclass = {
  3,
  &slli_operand_list[0]
};

static xtensa_operand_internal *s16i_operand_list[] = {
  &ait_operand,
  &ais_operand,
  &iiuimm8x2_operand
};

static xtensa_iclass_internal s16i_iclass = {
  3,
  &s16i_operand_list[0]
};

static xtensa_operand_internal *call_operand_list[] = {
  &lisoffsetx4_operand
};

static xtensa_iclass_internal call_iclass = {
  1,
  &call_operand_list[0]
};

static xtensa_operand_internal *shifts_operand_list[] = {
  &aor_operand,
  &ais_operand
};

static xtensa_iclass_internal shifts_iclass = {
  2,
  &shifts_operand_list[0]
};

static xtensa_operand_internal *shiftt_operand_list[] = {
  &aor_operand,
  &ait_operand
};

static xtensa_iclass_internal shiftt_iclass = {
  2,
  &shiftt_operand_list[0]
};

static xtensa_operand_internal *rotw_operand_list[] = {
  &iisimm4_operand
};

static xtensa_iclass_internal rotw_iclass = {
  1,
  &rotw_operand_list[0]
};

static xtensa_operand_internal *addsub_operand_list[] = {
  &aor_operand,
  &ais_operand,
  &ait_operand
};

static xtensa_iclass_internal addsub_iclass = {
  3,
  &addsub_operand_list[0]
};

static xtensa_operand_internal *l8i_operand_list[] = {
  &aot_operand,
  &ais_operand,
  &iiuimm8_operand
};

static xtensa_iclass_internal l8i_iclass = {
  3,
  &l8i_operand_list[0]
};

static xtensa_operand_internal *sari_operand_list[] = {
  &iisas_operand
};

static xtensa_iclass_internal sari_iclass = {
  1,
  &sari_operand_list[0]
};

static xtensa_operand_internal *xsr_operand_list[] = {
  &abt_operand,
  &iisr_operand
};

static xtensa_iclass_internal xsr_iclass = {
  2,
  &xsr_operand_list[0]
};

static xtensa_operand_internal *rsil_operand_list[] = {
  &aot_operand,
  &iis_operand
};

static xtensa_iclass_internal rsil_iclass = {
  2,
  &rsil_operand_list[0]
};

static xtensa_operand_internal *bst8_operand_list[] = {
  &ais_operand,
  &ait_operand,
  &lisimm8_operand
};

static xtensa_iclass_internal bst8_iclass = {
  3,
  &bst8_operand_list[0]
};

static xtensa_operand_internal *addi_operand_list[] = {
  &aot_operand,
  &ais_operand,
  &iisimm8_operand
};

static xtensa_iclass_internal addi_iclass = {
  3,
  &addi_operand_list[0]
};

static xtensa_operand_internal *callx12_operand_list[] = {
  &ais_operand
};

static xtensa_iclass_internal callx12_iclass = {
  1,
  &callx12_operand_list[0]
};

static xtensa_operand_internal *bsi8_operand_list[] = {
  &ais_operand,
  &iib4const_operand,
  &lisimm8_operand
};

static xtensa_iclass_internal bsi8_iclass = {
  3,
  &bsi8_operand_list[0]
};

static xtensa_operand_internal *jumpx_operand_list[] = {
  &ais_operand
};

static xtensa_iclass_internal jumpx_iclass = {
  1,
  &jumpx_operand_list[0]
};

static xtensa_iclass_internal retn_iclass = {
  0,
  0
};

static xtensa_operand_internal *nsa_operand_list[] = {
  &aot_operand,
  &ais_operand
};

static xtensa_iclass_internal nsa_iclass = {
  2,
  &nsa_operand_list[0]
};

static xtensa_operand_internal *storei4_operand_list[] = {
  &ait_operand,
  &ais_operand,
  &iilsi4x4_operand
};

static xtensa_iclass_internal storei4_iclass = {
  3,
  &storei4_operand_list[0]
};

static xtensa_operand_internal *wtlb_operand_list[] = {
  &ait_operand,
  &ais_operand
};

static xtensa_iclass_internal wtlb_iclass = {
  2,
  &wtlb_operand_list[0]
};

static xtensa_operand_internal *dce_operand_list[] = {
  &ais_operand,
  &iiuimm4x16_operand
};

static xtensa_iclass_internal dce_iclass = {
  2,
  &dce_operand_list[0]
};

static xtensa_operand_internal *l16i_operand_list[] = {
  &aot_operand,
  &ais_operand,
  &iiuimm8x2_operand
};

static xtensa_iclass_internal l16i_iclass = {
  3,
  &l16i_operand_list[0]
};

static xtensa_operand_internal *callx4_operand_list[] = {
  &ais_operand
};

static xtensa_iclass_internal callx4_iclass = {
  1,
  &callx4_operand_list[0]
};

static xtensa_operand_internal *callx8_operand_list[] = {
  &ais_operand
};

static xtensa_iclass_internal callx8_iclass = {
  1,
  &callx8_operand_list[0]
};

static xtensa_operand_internal *movsp_operand_list[] = {
  &aot_operand,
  &ais_operand
};

static xtensa_iclass_internal movsp_iclass = {
  2,
  &movsp_operand_list[0]
};

static xtensa_operand_internal *wsr_operand_list[] = {
  &ait_operand,
  &iisr_operand
};

static xtensa_iclass_internal wsr_iclass = {
  2,
  &wsr_operand_list[0]
};

static xtensa_operand_internal *call12_operand_list[] = {
  &lisoffsetx4_operand
};

static xtensa_iclass_internal call12_iclass = {
  1,
  &call12_operand_list[0]
};

static xtensa_operand_internal *call4_operand_list[] = {
  &lisoffsetx4_operand
};

static xtensa_iclass_internal call4_iclass = {
  1,
  &call4_operand_list[0]
};

static xtensa_operand_internal *addmi_operand_list[] = {
  &aot_operand,
  &ais_operand,
  &iisimm8x256_operand
};

static xtensa_iclass_internal addmi_iclass = {
  3,
  &addmi_operand_list[0]
};

static xtensa_operand_internal *bit_operand_list[] = {
  &aor_operand,
  &ais_operand,
  &ait_operand
};

static xtensa_iclass_internal bit_iclass = {
  3,
  &bit_operand_list[0]
};

static xtensa_operand_internal *call8_operand_list[] = {
  &lisoffsetx4_operand
};

static xtensa_iclass_internal call8_iclass = {
  1,
  &call8_operand_list[0]
};

static xtensa_iclass_internal itlba_iclass = {
  0,
  0
};

static xtensa_operand_internal *break_n_operand_list[] = {
  &iis_operand
};

static xtensa_iclass_internal break_n_iclass = {
  1,
  &break_n_operand_list[0]
};

static xtensa_operand_internal *sar_operand_list[] = {
  &ais_operand
};

static xtensa_iclass_internal sar_iclass = {
  1,
  &sar_operand_list[0]
};

static xtensa_operand_internal *s32e_operand_list[] = {
  &ait_operand,
  &ais_operand,
  &iinimm4x2_operand
};

static xtensa_iclass_internal s32e_iclass = {
  3,
  &s32e_operand_list[0]
};

static xtensa_operand_internal *bz6_operand_list[] = {
  &ais_operand,
  &liuimm6_operand
};

static xtensa_iclass_internal bz6_iclass = {
  2,
  &bz6_operand_list[0]
};

static xtensa_operand_internal *loop_operand_list[] = {
  &ais_operand,
  &liuimm8_operand
};

static xtensa_iclass_internal loop_iclass = {
  2,
  &loop_operand_list[0]
};

static xtensa_operand_internal *rsr_operand_list[] = {
  &aot_operand,
  &iisr_operand
};

static xtensa_iclass_internal rsr_iclass = {
  2,
  &rsr_operand_list[0]
};

static xtensa_operand_internal *icache_operand_list[] = {
  &ais_operand,
  &iiuimm8x4_operand
};

static xtensa_iclass_internal icache_iclass = {
  2,
  &icache_operand_list[0]
};

static xtensa_operand_internal *s8i_operand_list[] = {
  &ait_operand,
  &ais_operand,
  &iiuimm8_operand
};

static xtensa_iclass_internal s8i_iclass = {
  3,
  &s8i_operand_list[0]
};

static xtensa_iclass_internal return_iclass = {
  0,
  0
};

static xtensa_operand_internal *dcache_operand_list[] = {
  &ais_operand,
  &iiuimm8x4_operand
};

static xtensa_iclass_internal dcache_iclass = {
  2,
  &dcache_operand_list[0]
};

static xtensa_operand_internal *s32i_operand_list[] = {
  &ait_operand,
  &ais_operand,
  &iiuimm8x4_operand
};

static xtensa_iclass_internal s32i_iclass = {
  3,
  &s32i_operand_list[0]
};

static xtensa_operand_internal *jump_operand_list[] = {
  &lisoffset_operand
};

static xtensa_iclass_internal jump_iclass = {
  1,
  &jump_operand_list[0]
};

static xtensa_operand_internal *addi_n_operand_list[] = {
  &aor_operand,
  &ais_operand,
  &iiai4const_operand
};

static xtensa_iclass_internal addi_n_iclass = {
  3,
  &addi_n_operand_list[0]
};

static xtensa_iclass_internal sync_iclass = {
  0,
  0
};

static xtensa_operand_internal *neg_operand_list[] = {
  &aor_operand,
  &ait_operand
};

static xtensa_iclass_internal neg_iclass = {
  2,
  &neg_operand_list[0]
};

static xtensa_iclass_internal syscall_iclass = {
  0,
  0
};

static xtensa_operand_internal *bsz12_operand_list[] = {
  &ais_operand,
  &lisimm12_operand
};

static xtensa_iclass_internal bsz12_iclass = {
  2,
  &bsz12_operand_list[0]
};

static xtensa_iclass_internal excw_iclass = {
  0,
  0
};

static xtensa_operand_internal *movi_n_operand_list[] = {
  &aos_operand,
  &iisimm7_operand
};

static xtensa_iclass_internal movi_n_iclass = {
  2,
  &movi_n_operand_list[0]
};

static xtensa_operand_internal *rtlb_operand_list[] = {
  &aot_operand,
  &ais_operand
};

static xtensa_iclass_internal rtlb_iclass = {
  2,
  &rtlb_operand_list[0]
};

static xtensa_operand_internal *actl_operand_list[] = {
  &aot_operand,
  &ais_operand
};

static xtensa_iclass_internal actl_iclass = {
  2,
  &actl_operand_list[0]
};

static xtensa_operand_internal *srli_operand_list[] = {
  &aor_operand,
  &ait_operand,
  &iis_operand
};

static xtensa_iclass_internal srli_iclass = {
  3,
  &srli_operand_list[0]
};

static xtensa_operand_internal *bsi8b_operand_list[] = {
  &ais_operand,
  &iibbi_operand,
  &lisimm8_operand
};

static xtensa_iclass_internal bsi8b_iclass = {
  3,
  &bsi8b_operand_list[0]
};

static xtensa_operand_internal *acts_operand_list[] = {
  &ait_operand,
  &ais_operand
};

static xtensa_iclass_internal acts_iclass = {
  2,
  &acts_operand_list[0]
};

static xtensa_operand_internal *add_n_operand_list[] = {
  &aor_operand,
  &ais_operand,
  &ait_operand
};

static xtensa_iclass_internal add_n_iclass = {
  3,
  &add_n_operand_list[0]
};

static xtensa_operand_internal *srai_operand_list[] = {
  &aor_operand,
  &ait_operand,
  &iisar_operand
};

static xtensa_iclass_internal srai_iclass = {
  3,
  &srai_operand_list[0]
};

static xtensa_operand_internal *entry_operand_list[] = {
  &abs_operand,
  &iiuimm12x8_operand
};

static xtensa_iclass_internal entry_iclass = {
  2,
  &entry_operand_list[0]
};

static xtensa_operand_internal *l32e_operand_list[] = {
  &aot_operand,
  &ais_operand,
  &iinimm4x2_operand
};

static xtensa_iclass_internal l32e_iclass = {
  3,
  &l32e_operand_list[0]
};

static xtensa_operand_internal *dpf_operand_list[] = {
  &ais_operand,
  &iiuimm8x4_operand
};

static xtensa_iclass_internal dpf_iclass = {
  2,
  &dpf_operand_list[0]
};

static xtensa_operand_internal *l32i_operand_list[] = {
  &aot_operand,
  &ais_operand,
  &iiuimm8x4_operand
};

static xtensa_iclass_internal l32i_iclass = {
  3,
  &l32i_operand_list[0]
};

static xtensa_insnbuf abs_template (void);
static xtensa_insnbuf add_template (void);
static xtensa_insnbuf add_n_template (void);
static xtensa_insnbuf addi_template (void);
static xtensa_insnbuf addi_n_template (void);
static xtensa_insnbuf addmi_template (void);
static xtensa_insnbuf addx2_template (void);
static xtensa_insnbuf addx4_template (void);
static xtensa_insnbuf addx8_template (void);
static xtensa_insnbuf and_template (void);
static xtensa_insnbuf ball_template (void);
static xtensa_insnbuf bany_template (void);
static xtensa_insnbuf bbc_template (void);
static xtensa_insnbuf bbci_template (void);
static xtensa_insnbuf bbs_template (void);
static xtensa_insnbuf bbsi_template (void);
static xtensa_insnbuf beq_template (void);
static xtensa_insnbuf beqi_template (void);
static xtensa_insnbuf beqz_template (void);
static xtensa_insnbuf beqz_n_template (void);
static xtensa_insnbuf bge_template (void);
static xtensa_insnbuf bgei_template (void);
static xtensa_insnbuf bgeu_template (void);
static xtensa_insnbuf bgeui_template (void);
static xtensa_insnbuf bgez_template (void);
static xtensa_insnbuf blt_template (void);
static xtensa_insnbuf blti_template (void);
static xtensa_insnbuf bltu_template (void);
static xtensa_insnbuf bltui_template (void);
static xtensa_insnbuf bltz_template (void);
static xtensa_insnbuf bnall_template (void);
static xtensa_insnbuf bne_template (void);
static xtensa_insnbuf bnei_template (void);
static xtensa_insnbuf bnez_template (void);
static xtensa_insnbuf bnez_n_template (void);
static xtensa_insnbuf bnone_template (void);
static xtensa_insnbuf break_template (void);
static xtensa_insnbuf break_n_template (void);
static xtensa_insnbuf call0_template (void);
static xtensa_insnbuf call12_template (void);
static xtensa_insnbuf call4_template (void);
static xtensa_insnbuf call8_template (void);
static xtensa_insnbuf callx0_template (void);
static xtensa_insnbuf callx12_template (void);
static xtensa_insnbuf callx4_template (void);
static xtensa_insnbuf callx8_template (void);
static xtensa_insnbuf dhi_template (void);
static xtensa_insnbuf dhwb_template (void);
static xtensa_insnbuf dhwbi_template (void);
static xtensa_insnbuf dii_template (void);
static xtensa_insnbuf diwb_template (void);
static xtensa_insnbuf diwbi_template (void);
static xtensa_insnbuf dpfr_template (void);
static xtensa_insnbuf dpfro_template (void);
static xtensa_insnbuf dpfw_template (void);
static xtensa_insnbuf dpfwo_template (void);
static xtensa_insnbuf dsync_template (void);
static xtensa_insnbuf entry_template (void);
static xtensa_insnbuf esync_template (void);
static xtensa_insnbuf excw_template (void);
static xtensa_insnbuf extui_template (void);
static xtensa_insnbuf idtlb_template (void);
static xtensa_insnbuf idtlba_template (void);
static xtensa_insnbuf ihi_template (void);
static xtensa_insnbuf iii_template (void);
static xtensa_insnbuf iitlb_template (void);
static xtensa_insnbuf iitlba_template (void);
static xtensa_insnbuf ipf_template (void);
static xtensa_insnbuf isync_template (void);
static xtensa_insnbuf j_template (void);
static xtensa_insnbuf jx_template (void);
static xtensa_insnbuf l16si_template (void);
static xtensa_insnbuf l16ui_template (void);
static xtensa_insnbuf l32e_template (void);
static xtensa_insnbuf l32i_template (void);
static xtensa_insnbuf l32i_n_template (void);
static xtensa_insnbuf l32r_template (void);
static xtensa_insnbuf l8ui_template (void);
static xtensa_insnbuf ldct_template (void);
static xtensa_insnbuf lict_template (void);
static xtensa_insnbuf licw_template (void);
static xtensa_insnbuf loop_template (void);
static xtensa_insnbuf loopgtz_template (void);
static xtensa_insnbuf loopnez_template (void);
static xtensa_insnbuf memw_template (void);
static xtensa_insnbuf mov_n_template (void);
static xtensa_insnbuf moveqz_template (void);
static xtensa_insnbuf movgez_template (void);
static xtensa_insnbuf movi_template (void);
static xtensa_insnbuf movi_n_template (void);
static xtensa_insnbuf movltz_template (void);
static xtensa_insnbuf movnez_template (void);
static xtensa_insnbuf movsp_template (void);
static xtensa_insnbuf neg_template (void);
static xtensa_insnbuf nop_n_template (void);
static xtensa_insnbuf nsa_template (void);
static xtensa_insnbuf nsau_template (void);
static xtensa_insnbuf or_template (void);
static xtensa_insnbuf pdtlb_template (void);
static xtensa_insnbuf pitlb_template (void);
static xtensa_insnbuf rdtlb0_template (void);
static xtensa_insnbuf rdtlb1_template (void);
static xtensa_insnbuf ret_template (void);
static xtensa_insnbuf ret_n_template (void);
static xtensa_insnbuf retw_template (void);
static xtensa_insnbuf retw_n_template (void);
static xtensa_insnbuf rfde_template (void);
static xtensa_insnbuf rfe_template (void);
static xtensa_insnbuf rfi_template (void);
static xtensa_insnbuf rfwo_template (void);
static xtensa_insnbuf rfwu_template (void);
static xtensa_insnbuf ritlb0_template (void);
static xtensa_insnbuf ritlb1_template (void);
static xtensa_insnbuf rotw_template (void);
static xtensa_insnbuf rsil_template (void);
static xtensa_insnbuf rsr_template (void);
static xtensa_insnbuf rsync_template (void);
static xtensa_insnbuf s16i_template (void);
static xtensa_insnbuf s32e_template (void);
static xtensa_insnbuf s32i_template (void);
static xtensa_insnbuf s32i_n_template (void);
static xtensa_insnbuf s8i_template (void);
static xtensa_insnbuf sdct_template (void);
static xtensa_insnbuf sict_template (void);
static xtensa_insnbuf sicw_template (void);
static xtensa_insnbuf simcall_template (void);
static xtensa_insnbuf sll_template (void);
static xtensa_insnbuf slli_template (void);
static xtensa_insnbuf sra_template (void);
static xtensa_insnbuf srai_template (void);
static xtensa_insnbuf src_template (void);
static xtensa_insnbuf srl_template (void);
static xtensa_insnbuf srli_template (void);
static xtensa_insnbuf ssa8b_template (void);
static xtensa_insnbuf ssa8l_template (void);
static xtensa_insnbuf ssai_template (void);
static xtensa_insnbuf ssl_template (void);
static xtensa_insnbuf ssr_template (void);
static xtensa_insnbuf sub_template (void);
static xtensa_insnbuf subx2_template (void);
static xtensa_insnbuf subx4_template (void);
static xtensa_insnbuf subx8_template (void);
static xtensa_insnbuf syscall_template (void);
static xtensa_insnbuf waiti_template (void);
static xtensa_insnbuf wdtlb_template (void);
static xtensa_insnbuf witlb_template (void);
static xtensa_insnbuf wsr_template (void);
static xtensa_insnbuf xor_template (void);
static xtensa_insnbuf xsr_template (void);

static xtensa_insnbuf
abs_template (void)
{
  static xtensa_insnbuf_word template[] = { 0x00001006 };
  return &template[0];
}

static xtensa_insnbuf
add_template (void)
{
  static xtensa_insnbuf_word template[] = { 0x00000008 };
  return &template[0];
}

static xtensa_insnbuf
add_n_template (void)
{
  static xtensa_insnbuf_word template[] = { 0x00a00000 };
  return &template[0];
}

static xtensa_insnbuf
addi_template (void)
{
  static xtensa_insnbuf_word template[] = { 0x00200c00 };
  return &template[0];
}

static xtensa_insnbuf
addi_n_template (void)
{
  static xtensa_insnbuf_word template[] = { 0x00b00000 };
  return &template[0];
}

static xtensa_insnbuf
addmi_template (void)
{
  static xtensa_insnbuf_word template[] = { 0x00200d00 };
  return &template[0];
}

static xtensa_insnbuf
addx2_template (void)
{
  static xtensa_insnbuf_word template[] = { 0x00000009 };
  return &template[0];
}

static xtensa_insnbuf
addx4_template (void)
{
  static xtensa_insnbuf_word template[] = { 0x0000000a };
  return &template[0];
}

static xtensa_insnbuf
addx8_template (void)
{
  static xtensa_insnbuf_word template[] = { 0x0000000b };
  return &template[0];
}

static xtensa_insnbuf
and_template (void)
{
  static xtensa_insnbuf_word template[] = { 0x00000001 };
  return &template[0];
}

static xtensa_insnbuf
ball_template (void)
{
  static xtensa_insnbuf_word template[] = { 0x00700400 };
  return &template[0];
}

static xtensa_insnbuf
bany_template (void)
{
  static xtensa_insnbuf_word template[] = { 0x00700800 };
  return &template[0];
}

static xtensa_insnbuf
bbc_template (void)
{
  static xtensa_insnbuf_word template[] = { 0x00700500 };
  return &template[0];
}

static xtensa_insnbuf
bbci_template (void)
{
  static xtensa_insnbuf_word template[] = { 0x00700600 };
  return &template[0];
}

static xtensa_insnbuf
bbs_template (void)
{
  static xtensa_insnbuf_word template[] = { 0x00700d00 };
  return &template[0];
}

static xtensa_insnbuf
bbsi_template (void)
{
  static xtensa_insnbuf_word template[] = { 0x00700e00 };
  return &template[0];
}

static xtensa_insnbuf
beq_template (void)
{
  static xtensa_insnbuf_word template[] = { 0x00700100 };
  return &template[0];
}

static xtensa_insnbuf
beqi_template (void)
{
  static xtensa_insnbuf_word template[] = { 0x00680000 };
  return &template[0];
}

static xtensa_insnbuf
beqz_template (void)
{
  static xtensa_insnbuf_word template[] = { 0x00640000 };
  return &template[0];
}

static xtensa_insnbuf
beqz_n_template (void)
{
  static xtensa_insnbuf_word template[] = { 0x00c80000 };
  return &template[0];
}

static xtensa_insnbuf
bge_template (void)
{
  static xtensa_insnbuf_word template[] = { 0x00700a00 };
  return &template[0];
}

static xtensa_insnbuf
bgei_template (void)
{
  static xtensa_insnbuf_word template[] = { 0x006b0000 };
  return &template[0];
}

static xtensa_insnbuf
bgeu_template (void)
{
  static xtensa_insnbuf_word template[] = { 0x00700b00 };
  return &template[0];
}

static xtensa_insnbuf
bgeui_template (void)
{
  static xtensa_insnbuf_word template[] = { 0x006f0000 };
  return &template[0];
}

static xtensa_insnbuf
bgez_template (void)
{
  static xtensa_insnbuf_word template[] = { 0x00670000 };
  return &template[0];
}

static xtensa_insnbuf
blt_template (void)
{
  static xtensa_insnbuf_word template[] = { 0x00700200 };
  return &template[0];
}

static xtensa_insnbuf
blti_template (void)
{
  static xtensa_insnbuf_word template[] = { 0x006a0000 };
  return &template[0];
}

static xtensa_insnbuf
bltu_template (void)
{
  static xtensa_insnbuf_word template[] = { 0x00700300 };
  return &template[0];
}

static xtensa_insnbuf
bltui_template (void)
{
  static xtensa_insnbuf_word template[] = { 0x006e0000 };
  return &template[0];
}

static xtensa_insnbuf
bltz_template (void)
{
  static xtensa_insnbuf_word template[] = { 0x00660000 };
  return &template[0];
}

static xtensa_insnbuf
bnall_template (void)
{
  static xtensa_insnbuf_word template[] = { 0x00700c00 };
  return &template[0];
}

static xtensa_insnbuf
bne_template (void)
{
  static xtensa_insnbuf_word template[] = { 0x00700900 };
  return &template[0];
}

static xtensa_insnbuf
bnei_template (void)
{
  static xtensa_insnbuf_word template[] = { 0x00690000 };
  return &template[0];
}

static xtensa_insnbuf
bnez_template (void)
{
  static xtensa_insnbuf_word template[] = { 0x00650000 };
  return &template[0];
}

static xtensa_insnbuf
bnez_n_template (void)
{
  static xtensa_insnbuf_word template[] = { 0x00cc0000 };
  return &template[0];
}

static xtensa_insnbuf
bnone_template (void)
{
  static xtensa_insnbuf_word template[] = { 0x00700000 };
  return &template[0];
}

static xtensa_insnbuf
break_template (void)
{
  static xtensa_insnbuf_word template[] = { 0x00000400 };
  return &template[0];
}

static xtensa_insnbuf
break_n_template (void)
{
  static xtensa_insnbuf_word template[] = { 0x00d20f00 };
  return &template[0];
}

static xtensa_insnbuf
call0_template (void)
{
  static xtensa_insnbuf_word template[] = { 0x00500000 };
  return &template[0];
}

static xtensa_insnbuf
call12_template (void)
{
  static xtensa_insnbuf_word template[] = { 0x005c0000 };
  return &template[0];
}

static xtensa_insnbuf
call4_template (void)
{
  static xtensa_insnbuf_word template[] = { 0x00540000 };
  return &template[0];
}

static xtensa_insnbuf
call8_template (void)
{
  static xtensa_insnbuf_word template[] = { 0x00580000 };
  return &template[0];
}

static xtensa_insnbuf
callx0_template (void)
{
  static xtensa_insnbuf_word template[] = { 0x00030000 };
  return &template[0];
}

static xtensa_insnbuf
callx12_template (void)
{
  static xtensa_insnbuf_word template[] = { 0x000f0000 };
  return &template[0];
}

static xtensa_insnbuf
callx4_template (void)
{
  static xtensa_insnbuf_word template[] = { 0x00070000 };
  return &template[0];
}

static xtensa_insnbuf
callx8_template (void)
{
  static xtensa_insnbuf_word template[] = { 0x000b0000 };
  return &template[0];
}

static xtensa_insnbuf
dhi_template (void)
{
  static xtensa_insnbuf_word template[] = { 0x00260700 };
  return &template[0];
}

static xtensa_insnbuf
dhwb_template (void)
{
  static xtensa_insnbuf_word template[] = { 0x00240700 };
  return &template[0];
}

static xtensa_insnbuf
dhwbi_template (void)
{
  static xtensa_insnbuf_word template[] = { 0x00250700 };
  return &template[0];
}

static xtensa_insnbuf
dii_template (void)
{
  static xtensa_insnbuf_word template[] = { 0x00270700 };
  return &template[0];
}

static xtensa_insnbuf
diwb_template (void)
{
  static xtensa_insnbuf_word template[] = { 0x00280740 };
  return &template[0];
}

static xtensa_insnbuf
diwbi_template (void)
{
  static xtensa_insnbuf_word template[] = { 0x00280750 };
  return &template[0];
}

static xtensa_insnbuf
dpfr_template (void)
{
  static xtensa_insnbuf_word template[] = { 0x00200700 };
  return &template[0];
}

static xtensa_insnbuf
dpfro_template (void)
{
  static xtensa_insnbuf_word template[] = { 0x00220700 };
  return &template[0];
}

static xtensa_insnbuf
dpfw_template (void)
{
  static xtensa_insnbuf_word template[] = { 0x00210700 };
  return &template[0];
}

static xtensa_insnbuf
dpfwo_template (void)
{
  static xtensa_insnbuf_word template[] = { 0x00230700 };
  return &template[0];
}

static xtensa_insnbuf
dsync_template (void)
{
  static xtensa_insnbuf_word template[] = { 0x00030200 };
  return &template[0];
}

static xtensa_insnbuf
entry_template (void)
{
  static xtensa_insnbuf_word template[] = { 0x006c0000 };
  return &template[0];
}

static xtensa_insnbuf
esync_template (void)
{
  static xtensa_insnbuf_word template[] = { 0x00020200 };
  return &template[0];
}

static xtensa_insnbuf
excw_template (void)
{
  static xtensa_insnbuf_word template[] = { 0x00080200 };
  return &template[0];
}

static xtensa_insnbuf
extui_template (void)
{
  static xtensa_insnbuf_word template[] = { 0x00000040 };
  return &template[0];
}

static xtensa_insnbuf
idtlb_template (void)
{
  static xtensa_insnbuf_word template[] = { 0x00000c05 };
  return &template[0];
}

static xtensa_insnbuf
idtlba_template (void)
{
  static xtensa_insnbuf_word template[] = { 0x00000805 };
  return &template[0];
}

static xtensa_insnbuf
ihi_template (void)
{
  static xtensa_insnbuf_word template[] = { 0x002e0700 };
  return &template[0];
}

static xtensa_insnbuf
iii_template (void)
{
  static xtensa_insnbuf_word template[] = { 0x002f0700 };
  return &template[0];
}

static xtensa_insnbuf
iitlb_template (void)
{
  static xtensa_insnbuf_word template[] = { 0x00000405 };
  return &template[0];
}

static xtensa_insnbuf
iitlba_template (void)
{
  static xtensa_insnbuf_word template[] = { 0x00000005 };
  return &template[0];
}

static xtensa_insnbuf
ipf_template (void)
{
  static xtensa_insnbuf_word template[] = { 0x002c0700 };
  return &template[0];
}

static xtensa_insnbuf
isync_template (void)
{
  static xtensa_insnbuf_word template[] = { 0x00000200 };
  return &template[0];
}

static xtensa_insnbuf
j_template (void)
{
  static xtensa_insnbuf_word template[] = { 0x00600000 };
  return &template[0];
}

static xtensa_insnbuf
jx_template (void)
{
  static xtensa_insnbuf_word template[] = { 0x000a0000 };
  return &template[0];
}

static xtensa_insnbuf
l16si_template (void)
{
  static xtensa_insnbuf_word template[] = { 0x00200900 };
  return &template[0];
}

static xtensa_insnbuf
l16ui_template (void)
{
  static xtensa_insnbuf_word template[] = { 0x00200100 };
  return &template[0];
}

static xtensa_insnbuf
l32e_template (void)
{
  static xtensa_insnbuf_word template[] = { 0x00000090 };
  return &template[0];
}

static xtensa_insnbuf
l32i_template (void)
{
  static xtensa_insnbuf_word template[] = { 0x00200200 };
  return &template[0];
}

static xtensa_insnbuf
l32i_n_template (void)
{
  static xtensa_insnbuf_word template[] = { 0x00800000 };
  return &template[0];
}

static xtensa_insnbuf
l32r_template (void)
{
  static xtensa_insnbuf_word template[] = { 0x00100000 };
  return &template[0];
}

static xtensa_insnbuf
l8ui_template (void)
{
  static xtensa_insnbuf_word template[] = { 0x00200000 };
  return &template[0];
}

static xtensa_insnbuf
ldct_template (void)
{
  static xtensa_insnbuf_word template[] = { 0x0000081f };
  return &template[0];
}

static xtensa_insnbuf
lict_template (void)
{
  static xtensa_insnbuf_word template[] = { 0x0000001f };
  return &template[0];
}

static xtensa_insnbuf
licw_template (void)
{
  static xtensa_insnbuf_word template[] = { 0x0000021f };
  return &template[0];
}

static xtensa_insnbuf
loop_template (void)
{
  static xtensa_insnbuf_word template[] = { 0x006d0800 };
  return &template[0];
}

static xtensa_insnbuf
loopgtz_template (void)
{
  static xtensa_insnbuf_word template[] = { 0x006d0a00 };
  return &template[0];
}

static xtensa_insnbuf
loopnez_template (void)
{
  static xtensa_insnbuf_word template[] = { 0x006d0900 };
  return &template[0];
}

static xtensa_insnbuf
memw_template (void)
{
  static xtensa_insnbuf_word template[] = { 0x000c0200 };
  return &template[0];
}

static xtensa_insnbuf
mov_n_template (void)
{
  static xtensa_insnbuf_word template[] = { 0x00d00000 };
  return &template[0];
}

static xtensa_insnbuf
moveqz_template (void)
{
  static xtensa_insnbuf_word template[] = { 0x00000038 };
  return &template[0];
}

static xtensa_insnbuf
movgez_template (void)
{
  static xtensa_insnbuf_word template[] = { 0x0000003b };
  return &template[0];
}

static xtensa_insnbuf
movi_template (void)
{
  static xtensa_insnbuf_word template[] = { 0x00200a00 };
  return &template[0];
}

static xtensa_insnbuf
movi_n_template (void)
{
  static xtensa_insnbuf_word template[] = { 0x00c00000 };
  return &template[0];
}

static xtensa_insnbuf
movltz_template (void)
{
  static xtensa_insnbuf_word template[] = { 0x0000003a };
  return &template[0];
}

static xtensa_insnbuf
movnez_template (void)
{
  static xtensa_insnbuf_word template[] = { 0x00000039 };
  return &template[0];
}

static xtensa_insnbuf
movsp_template (void)
{
  static xtensa_insnbuf_word template[] = { 0x00000100 };
  return &template[0];
}

static xtensa_insnbuf
neg_template (void)
{
  static xtensa_insnbuf_word template[] = { 0x00000006 };
  return &template[0];
}

static xtensa_insnbuf
nop_n_template (void)
{
  static xtensa_insnbuf_word template[] = { 0x00d30f00 };
  return &template[0];
}

static xtensa_insnbuf
nsa_template (void)
{
  static xtensa_insnbuf_word template[] = { 0x00000e04 };
  return &template[0];
}

static xtensa_insnbuf
nsau_template (void)
{
  static xtensa_insnbuf_word template[] = { 0x00000f04 };
  return &template[0];
}

static xtensa_insnbuf
or_template (void)
{
  static xtensa_insnbuf_word template[] = { 0x00000002 };
  return &template[0];
}

static xtensa_insnbuf
pdtlb_template (void)
{
  static xtensa_insnbuf_word template[] = { 0x00000d05 };
  return &template[0];
}

static xtensa_insnbuf
pitlb_template (void)
{
  static xtensa_insnbuf_word template[] = { 0x00000505 };
  return &template[0];
}

static xtensa_insnbuf
rdtlb0_template (void)
{
  static xtensa_insnbuf_word template[] = { 0x00000b05 };
  return &template[0];
}

static xtensa_insnbuf
rdtlb1_template (void)
{
  static xtensa_insnbuf_word template[] = { 0x00000f05 };
  return &template[0];
}

static xtensa_insnbuf
ret_template (void)
{
  static xtensa_insnbuf_word template[] = { 0x00020000 };
  return &template[0];
}

static xtensa_insnbuf
ret_n_template (void)
{
  static xtensa_insnbuf_word template[] = { 0x00d00f00 };
  return &template[0];
}

static xtensa_insnbuf
retw_template (void)
{
  static xtensa_insnbuf_word template[] = { 0x00060000 };
  return &template[0];
}

static xtensa_insnbuf
retw_n_template (void)
{
  static xtensa_insnbuf_word template[] = { 0x00d10f00 };
  return &template[0];
}

static xtensa_insnbuf
rfde_template (void)
{
  static xtensa_insnbuf_word template[] = { 0x00002300 };
  return &template[0];
}

static xtensa_insnbuf
rfe_template (void)
{
  static xtensa_insnbuf_word template[] = { 0x00000300 };
  return &template[0];
}

static xtensa_insnbuf
rfi_template (void)
{
  static xtensa_insnbuf_word template[] = { 0x00010300 };
  return &template[0];
}

static xtensa_insnbuf
rfwo_template (void)
{
  static xtensa_insnbuf_word template[] = { 0x00004300 };
  return &template[0];
}

static xtensa_insnbuf
rfwu_template (void)
{
  static xtensa_insnbuf_word template[] = { 0x00005300 };
  return &template[0];
}

static xtensa_insnbuf
ritlb0_template (void)
{
  static xtensa_insnbuf_word template[] = { 0x00000305 };
  return &template[0];
}

static xtensa_insnbuf
ritlb1_template (void)
{
  static xtensa_insnbuf_word template[] = { 0x00000705 };
  return &template[0];
}

static xtensa_insnbuf
rotw_template (void)
{
  static xtensa_insnbuf_word template[] = { 0x00000804 };
  return &template[0];
}

static xtensa_insnbuf
rsil_template (void)
{
  static xtensa_insnbuf_word template[] = { 0x00000600 };
  return &template[0];
}

static xtensa_insnbuf
rsr_template (void)
{
  static xtensa_insnbuf_word template[] = { 0x00000030 };
  return &template[0];
}

static xtensa_insnbuf
rsync_template (void)
{
  static xtensa_insnbuf_word template[] = { 0x00010200 };
  return &template[0];
}

static xtensa_insnbuf
s16i_template (void)
{
  static xtensa_insnbuf_word template[] = { 0x00200500 };
  return &template[0];
}

static xtensa_insnbuf
s32e_template (void)
{
  static xtensa_insnbuf_word template[] = { 0x00000094 };
  return &template[0];
}

static xtensa_insnbuf
s32i_template (void)
{
  static xtensa_insnbuf_word template[] = { 0x00200600 };
  return &template[0];
}

static xtensa_insnbuf
s32i_n_template (void)
{
  static xtensa_insnbuf_word template[] = { 0x00900000 };
  return &template[0];
}

static xtensa_insnbuf
s8i_template (void)
{
  static xtensa_insnbuf_word template[] = { 0x00200400 };
  return &template[0];
}

static xtensa_insnbuf
sdct_template (void)
{
  static xtensa_insnbuf_word template[] = { 0x0000091f };
  return &template[0];
}

static xtensa_insnbuf
sict_template (void)
{
  static xtensa_insnbuf_word template[] = { 0x0000011f };
  return &template[0];
}

static xtensa_insnbuf
sicw_template (void)
{
  static xtensa_insnbuf_word template[] = { 0x0000031f };
  return &template[0];
}

static xtensa_insnbuf
simcall_template (void)
{
  static xtensa_insnbuf_word template[] = { 0x00001500 };
  return &template[0];
}

static xtensa_insnbuf
sll_template (void)
{
  static xtensa_insnbuf_word template[] = { 0x0000001a };
  return &template[0];
}

static xtensa_insnbuf
slli_template (void)
{
  static xtensa_insnbuf_word template[] = { 0x00000010 };
  return &template[0];
}

static xtensa_insnbuf
sra_template (void)
{
  static xtensa_insnbuf_word template[] = { 0x0000001b };
  return &template[0];
}

static xtensa_insnbuf
srai_template (void)
{
  static xtensa_insnbuf_word template[] = { 0x00000012 };
  return &template[0];
}

static xtensa_insnbuf
src_template (void)
{
  static xtensa_insnbuf_word template[] = { 0x00000018 };
  return &template[0];
}

static xtensa_insnbuf
srl_template (void)
{
  static xtensa_insnbuf_word template[] = { 0x00000019 };
  return &template[0];
}

static xtensa_insnbuf
srli_template (void)
{
  static xtensa_insnbuf_word template[] = { 0x00000014 };
  return &template[0];
}

static xtensa_insnbuf
ssa8b_template (void)
{
  static xtensa_insnbuf_word template[] = { 0x00000304 };
  return &template[0];
}

static xtensa_insnbuf
ssa8l_template (void)
{
  static xtensa_insnbuf_word template[] = { 0x00000204 };
  return &template[0];
}

static xtensa_insnbuf
ssai_template (void)
{
  static xtensa_insnbuf_word template[] = { 0x00000404 };
  return &template[0];
}

static xtensa_insnbuf
ssl_template (void)
{
  static xtensa_insnbuf_word template[] = { 0x00000104 };
  return &template[0];
}

static xtensa_insnbuf
ssr_template (void)
{
  static xtensa_insnbuf_word template[] = { 0x00000004 };
  return &template[0];
}

static xtensa_insnbuf
sub_template (void)
{
  static xtensa_insnbuf_word template[] = { 0x0000000c };
  return &template[0];
}

static xtensa_insnbuf
subx2_template (void)
{
  static xtensa_insnbuf_word template[] = { 0x0000000d };
  return &template[0];
}

static xtensa_insnbuf
subx4_template (void)
{
  static xtensa_insnbuf_word template[] = { 0x0000000e };
  return &template[0];
}

static xtensa_insnbuf
subx8_template (void)
{
  static xtensa_insnbuf_word template[] = { 0x0000000f };
  return &template[0];
}

static xtensa_insnbuf
syscall_template (void)
{
  static xtensa_insnbuf_word template[] = { 0x00000500 };
  return &template[0];
}

static xtensa_insnbuf
waiti_template (void)
{
  static xtensa_insnbuf_word template[] = { 0x00000700 };
  return &template[0];
}

static xtensa_insnbuf
wdtlb_template (void)
{
  static xtensa_insnbuf_word template[] = { 0x00000e05 };
  return &template[0];
}

static xtensa_insnbuf
witlb_template (void)
{
  static xtensa_insnbuf_word template[] = { 0x00000605 };
  return &template[0];
}

static xtensa_insnbuf
wsr_template (void)
{
  static xtensa_insnbuf_word template[] = { 0x00000031 };
  return &template[0];
}

static xtensa_insnbuf
xor_template (void)
{
  static xtensa_insnbuf_word template[] = { 0x00000003 };
  return &template[0];
}

static xtensa_insnbuf
xsr_template (void)
{
  static xtensa_insnbuf_word template[] = { 0x00000016 };
  return &template[0];
}

static xtensa_opcode_internal abs_opcode = {
  "abs",
  3,
  abs_template,
  &neg_iclass
};

static xtensa_opcode_internal add_opcode = {
  "add",
  3,
  add_template,
  &addsub_iclass
};

static xtensa_opcode_internal add_n_opcode = {
  "add.n",
  2,
  add_n_template,
  &add_n_iclass
};

static xtensa_opcode_internal addi_opcode = {
  "addi",
  3,
  addi_template,
  &addi_iclass
};

static xtensa_opcode_internal addi_n_opcode = {
  "addi.n",
  2,
  addi_n_template,
  &addi_n_iclass
};

static xtensa_opcode_internal addmi_opcode = {
  "addmi",
  3,
  addmi_template,
  &addmi_iclass
};

static xtensa_opcode_internal addx2_opcode = {
  "addx2",
  3,
  addx2_template,
  &addsub_iclass
};

static xtensa_opcode_internal addx4_opcode = {
  "addx4",
  3,
  addx4_template,
  &addsub_iclass
};

static xtensa_opcode_internal addx8_opcode = {
  "addx8",
  3,
  addx8_template,
  &addsub_iclass
};

static xtensa_opcode_internal and_opcode = {
  "and",
  3,
  and_template,
  &bit_iclass
};

static xtensa_opcode_internal ball_opcode = {
  "ball",
  3,
  ball_template,
  &bst8_iclass
};

static xtensa_opcode_internal bany_opcode = {
  "bany",
  3,
  bany_template,
  &bst8_iclass
};

static xtensa_opcode_internal bbc_opcode = {
  "bbc",
  3,
  bbc_template,
  &bst8_iclass
};

static xtensa_opcode_internal bbci_opcode = {
  "bbci",
  3,
  bbci_template,
  &bsi8b_iclass
};

static xtensa_opcode_internal bbs_opcode = {
  "bbs",
  3,
  bbs_template,
  &bst8_iclass
};

static xtensa_opcode_internal bbsi_opcode = {
  "bbsi",
  3,
  bbsi_template,
  &bsi8b_iclass
};

static xtensa_opcode_internal beq_opcode = {
  "beq",
  3,
  beq_template,
  &bst8_iclass
};

static xtensa_opcode_internal beqi_opcode = {
  "beqi",
  3,
  beqi_template,
  &bsi8_iclass
};

static xtensa_opcode_internal beqz_opcode = {
  "beqz",
  3,
  beqz_template,
  &bsz12_iclass
};

static xtensa_opcode_internal beqz_n_opcode = {
  "beqz.n",
  2,
  beqz_n_template,
  &bz6_iclass
};

static xtensa_opcode_internal bge_opcode = {
  "bge",
  3,
  bge_template,
  &bst8_iclass
};

static xtensa_opcode_internal bgei_opcode = {
  "bgei",
  3,
  bgei_template,
  &bsi8_iclass
};

static xtensa_opcode_internal bgeu_opcode = {
  "bgeu",
  3,
  bgeu_template,
  &bst8_iclass
};

static xtensa_opcode_internal bgeui_opcode = {
  "bgeui",
  3,
  bgeui_template,
  &bsi8u_iclass
};

static xtensa_opcode_internal bgez_opcode = {
  "bgez",
  3,
  bgez_template,
  &bsz12_iclass
};

static xtensa_opcode_internal blt_opcode = {
  "blt",
  3,
  blt_template,
  &bst8_iclass
};

static xtensa_opcode_internal blti_opcode = {
  "blti",
  3,
  blti_template,
  &bsi8_iclass
};

static xtensa_opcode_internal bltu_opcode = {
  "bltu",
  3,
  bltu_template,
  &bst8_iclass
};

static xtensa_opcode_internal bltui_opcode = {
  "bltui",
  3,
  bltui_template,
  &bsi8u_iclass
};

static xtensa_opcode_internal bltz_opcode = {
  "bltz",
  3,
  bltz_template,
  &bsz12_iclass
};

static xtensa_opcode_internal bnall_opcode = {
  "bnall",
  3,
  bnall_template,
  &bst8_iclass
};

static xtensa_opcode_internal bne_opcode = {
  "bne",
  3,
  bne_template,
  &bst8_iclass
};

static xtensa_opcode_internal bnei_opcode = {
  "bnei",
  3,
  bnei_template,
  &bsi8_iclass
};

static xtensa_opcode_internal bnez_opcode = {
  "bnez",
  3,
  bnez_template,
  &bsz12_iclass
};

static xtensa_opcode_internal bnez_n_opcode = {
  "bnez.n",
  2,
  bnez_n_template,
  &bz6_iclass
};

static xtensa_opcode_internal bnone_opcode = {
  "bnone",
  3,
  bnone_template,
  &bst8_iclass
};

static xtensa_opcode_internal break_opcode = {
  "break",
  3,
  break_template,
  &break_iclass
};

static xtensa_opcode_internal break_n_opcode = {
  "break.n",
  2,
  break_n_template,
  &break_n_iclass
};

static xtensa_opcode_internal call0_opcode = {
  "call0",
  3,
  call0_template,
  &call_iclass
};

static xtensa_opcode_internal call12_opcode = {
  "call12",
  3,
  call12_template,
  &call12_iclass
};

static xtensa_opcode_internal call4_opcode = {
  "call4",
  3,
  call4_template,
  &call4_iclass
};

static xtensa_opcode_internal call8_opcode = {
  "call8",
  3,
  call8_template,
  &call8_iclass
};

static xtensa_opcode_internal callx0_opcode = {
  "callx0",
  3,
  callx0_template,
  &callx_iclass
};

static xtensa_opcode_internal callx12_opcode = {
  "callx12",
  3,
  callx12_template,
  &callx12_iclass
};

static xtensa_opcode_internal callx4_opcode = {
  "callx4",
  3,
  callx4_template,
  &callx4_iclass
};

static xtensa_opcode_internal callx8_opcode = {
  "callx8",
  3,
  callx8_template,
  &callx8_iclass
};

static xtensa_opcode_internal dhi_opcode = {
  "dhi",
  3,
  dhi_template,
  &dcache_iclass
};

static xtensa_opcode_internal dhwb_opcode = {
  "dhwb",
  3,
  dhwb_template,
  &dcache_iclass
};

static xtensa_opcode_internal dhwbi_opcode = {
  "dhwbi",
  3,
  dhwbi_template,
  &dcache_iclass
};

static xtensa_opcode_internal dii_opcode = {
  "dii",
  3,
  dii_template,
  &dcache_iclass
};

static xtensa_opcode_internal diwb_opcode = {
  "diwb",
  3,
  diwb_template,
  &dce_iclass
};

static xtensa_opcode_internal diwbi_opcode = {
  "diwbi",
  3,
  diwbi_template,
  &dce_iclass
};

static xtensa_opcode_internal dpfr_opcode = {
  "dpfr",
  3,
  dpfr_template,
  &dpf_iclass
};

static xtensa_opcode_internal dpfro_opcode = {
  "dpfro",
  3,
  dpfro_template,
  &dpf_iclass
};

static xtensa_opcode_internal dpfw_opcode = {
  "dpfw",
  3,
  dpfw_template,
  &dpf_iclass
};

static xtensa_opcode_internal dpfwo_opcode = {
  "dpfwo",
  3,
  dpfwo_template,
  &dpf_iclass
};

static xtensa_opcode_internal dsync_opcode = {
  "dsync",
  3,
  dsync_template,
  &sync_iclass
};

static xtensa_opcode_internal entry_opcode = {
  "entry",
  3,
  entry_template,
  &entry_iclass
};

static xtensa_opcode_internal esync_opcode = {
  "esync",
  3,
  esync_template,
  &sync_iclass
};

static xtensa_opcode_internal excw_opcode = {
  "excw",
  3,
  excw_template,
  &excw_iclass
};

static xtensa_opcode_internal extui_opcode = {
  "extui",
  3,
  extui_template,
  &exti_iclass
};

static xtensa_opcode_internal idtlb_opcode = {
  "idtlb",
  3,
  idtlb_template,
  &itlb_iclass
};

static xtensa_opcode_internal idtlba_opcode = {
  "idtlba",
  3,
  idtlba_template,
  &itlba_iclass
};

static xtensa_opcode_internal ihi_opcode = {
  "ihi",
  3,
  ihi_template,
  &icache_iclass
};

static xtensa_opcode_internal iii_opcode = {
  "iii",
  3,
  iii_template,
  &icache_iclass
};

static xtensa_opcode_internal iitlb_opcode = {
  "iitlb",
  3,
  iitlb_template,
  &itlb_iclass
};

static xtensa_opcode_internal iitlba_opcode = {
  "iitlba",
  3,
  iitlba_template,
  &itlba_iclass
};

static xtensa_opcode_internal ipf_opcode = {
  "ipf",
  3,
  ipf_template,
  &icache_iclass
};

static xtensa_opcode_internal isync_opcode = {
  "isync",
  3,
  isync_template,
  &sync_iclass
};

static xtensa_opcode_internal j_opcode = {
  "j",
  3,
  j_template,
  &jump_iclass
};

static xtensa_opcode_internal jx_opcode = {
  "jx",
  3,
  jx_template,
  &jumpx_iclass
};

static xtensa_opcode_internal l16si_opcode = {
  "l16si",
  3,
  l16si_template,
  &l16i_iclass
};

static xtensa_opcode_internal l16ui_opcode = {
  "l16ui",
  3,
  l16ui_template,
  &l16i_iclass
};

static xtensa_opcode_internal l32e_opcode = {
  "l32e",
  3,
  l32e_template,
  &l32e_iclass
};

static xtensa_opcode_internal l32i_opcode = {
  "l32i",
  3,
  l32i_template,
  &l32i_iclass
};

static xtensa_opcode_internal l32i_n_opcode = {
  "l32i.n",
  2,
  l32i_n_template,
  &loadi4_iclass
};

static xtensa_opcode_internal l32r_opcode = {
  "l32r",
  3,
  l32r_template,
  &l32r_iclass
};

static xtensa_opcode_internal l8ui_opcode = {
  "l8ui",
  3,
  l8ui_template,
  &l8i_iclass
};

static xtensa_opcode_internal ldct_opcode = {
  "ldct",
  3,
  ldct_template,
  &actl_iclass
};

static xtensa_opcode_internal lict_opcode = {
  "lict",
  3,
  lict_template,
  &actl_iclass
};

static xtensa_opcode_internal licw_opcode = {
  "licw",
  3,
  licw_template,
  &actl_iclass
};

static xtensa_opcode_internal loop_opcode = {
  "loop",
  3,
  loop_template,
  &loop_iclass
};

static xtensa_opcode_internal loopgtz_opcode = {
  "loopgtz",
  3,
  loopgtz_template,
  &loop_iclass
};

static xtensa_opcode_internal loopnez_opcode = {
  "loopnez",
  3,
  loopnez_template,
  &loop_iclass
};

static xtensa_opcode_internal memw_opcode = {
  "memw",
  3,
  memw_template,
  &sync_iclass
};

static xtensa_opcode_internal mov_n_opcode = {
  "mov.n",
  2,
  mov_n_template,
  &mov_n_iclass
};

static xtensa_opcode_internal moveqz_opcode = {
  "moveqz",
  3,
  moveqz_template,
  &movz_iclass
};

static xtensa_opcode_internal movgez_opcode = {
  "movgez",
  3,
  movgez_template,
  &movz_iclass
};

static xtensa_opcode_internal movi_opcode = {
  "movi",
  3,
  movi_template,
  &movi_iclass
};

static xtensa_opcode_internal movi_n_opcode = {
  "movi.n",
  2,
  movi_n_template,
  &movi_n_iclass
};

static xtensa_opcode_internal movltz_opcode = {
  "movltz",
  3,
  movltz_template,
  &movz_iclass
};

static xtensa_opcode_internal movnez_opcode = {
  "movnez",
  3,
  movnez_template,
  &movz_iclass
};

static xtensa_opcode_internal movsp_opcode = {
  "movsp",
  3,
  movsp_template,
  &movsp_iclass
};

static xtensa_opcode_internal neg_opcode = {
  "neg",
  3,
  neg_template,
  &neg_iclass
};

static xtensa_opcode_internal nop_n_opcode = {
  "nop.n",
  2,
  nop_n_template,
  &nopn_iclass
};

static xtensa_opcode_internal nsa_opcode = {
  "nsa",
  3,
  nsa_template,
  &nsa_iclass
};

static xtensa_opcode_internal nsau_opcode = {
  "nsau",
  3,
  nsau_template,
  &nsa_iclass
};

static xtensa_opcode_internal or_opcode = {
  "or",
  3,
  or_template,
  &bit_iclass
};

static xtensa_opcode_internal pdtlb_opcode = {
  "pdtlb",
  3,
  pdtlb_template,
  &rtlb_iclass
};

static xtensa_opcode_internal pitlb_opcode = {
  "pitlb",
  3,
  pitlb_template,
  &rtlb_iclass
};

static xtensa_opcode_internal rdtlb0_opcode = {
  "rdtlb0",
  3,
  rdtlb0_template,
  &rtlb_iclass
};

static xtensa_opcode_internal rdtlb1_opcode = {
  "rdtlb1",
  3,
  rdtlb1_template,
  &rtlb_iclass
};

static xtensa_opcode_internal ret_opcode = {
  "ret",
  3,
  ret_template,
  &return_iclass
};

static xtensa_opcode_internal ret_n_opcode = {
  "ret.n",
  2,
  ret_n_template,
  &retn_iclass
};

static xtensa_opcode_internal retw_opcode = {
  "retw",
  3,
  retw_template,
  &return_iclass
};

static xtensa_opcode_internal retw_n_opcode = {
  "retw.n",
  2,
  retw_n_template,
  &retn_iclass
};

static xtensa_opcode_internal rfde_opcode = {
  "rfde",
  3,
  rfde_template,
  &rfe_iclass
};

static xtensa_opcode_internal rfe_opcode = {
  "rfe",
  3,
  rfe_template,
  &rfe_iclass
};

static xtensa_opcode_internal rfi_opcode = {
  "rfi",
  3,
  rfi_template,
  &rfi_iclass
};

static xtensa_opcode_internal rfwo_opcode = {
  "rfwo",
  3,
  rfwo_template,
  &rfe_iclass
};

static xtensa_opcode_internal rfwu_opcode = {
  "rfwu",
  3,
  rfwu_template,
  &rfe_iclass
};

static xtensa_opcode_internal ritlb0_opcode = {
  "ritlb0",
  3,
  ritlb0_template,
  &rtlb_iclass
};

static xtensa_opcode_internal ritlb1_opcode = {
  "ritlb1",
  3,
  ritlb1_template,
  &rtlb_iclass
};

static xtensa_opcode_internal rotw_opcode = {
  "rotw",
  3,
  rotw_template,
  &rotw_iclass
};

static xtensa_opcode_internal rsil_opcode = {
  "rsil",
  3,
  rsil_template,
  &rsil_iclass
};

static xtensa_opcode_internal rsr_opcode = {
  "rsr",
  3,
  rsr_template,
  &rsr_iclass
};

static xtensa_opcode_internal rsync_opcode = {
  "rsync",
  3,
  rsync_template,
  &sync_iclass
};

static xtensa_opcode_internal s16i_opcode = {
  "s16i",
  3,
  s16i_template,
  &s16i_iclass
};

static xtensa_opcode_internal s32e_opcode = {
  "s32e",
  3,
  s32e_template,
  &s32e_iclass
};

static xtensa_opcode_internal s32i_opcode = {
  "s32i",
  3,
  s32i_template,
  &s32i_iclass
};

static xtensa_opcode_internal s32i_n_opcode = {
  "s32i.n",
  2,
  s32i_n_template,
  &storei4_iclass
};

static xtensa_opcode_internal s8i_opcode = {
  "s8i",
  3,
  s8i_template,
  &s8i_iclass
};

static xtensa_opcode_internal sdct_opcode = {
  "sdct",
  3,
  sdct_template,
  &acts_iclass
};

static xtensa_opcode_internal sict_opcode = {
  "sict",
  3,
  sict_template,
  &acts_iclass
};

static xtensa_opcode_internal sicw_opcode = {
  "sicw",
  3,
  sicw_template,
  &acts_iclass
};

static xtensa_opcode_internal simcall_opcode = {
  "simcall",
  3,
  simcall_template,
  &syscall_iclass
};

static xtensa_opcode_internal sll_opcode = {
  "sll",
  3,
  sll_template,
  &shifts_iclass
};

static xtensa_opcode_internal slli_opcode = {
  "slli",
  3,
  slli_template,
  &slli_iclass
};

static xtensa_opcode_internal sra_opcode = {
  "sra",
  3,
  sra_template,
  &shiftt_iclass
};

static xtensa_opcode_internal srai_opcode = {
  "srai",
  3,
  srai_template,
  &srai_iclass
};

static xtensa_opcode_internal src_opcode = {
  "src",
  3,
  src_template,
  &shiftst_iclass
};

static xtensa_opcode_internal srl_opcode = {
  "srl",
  3,
  srl_template,
  &shiftt_iclass
};

static xtensa_opcode_internal srli_opcode = {
  "srli",
  3,
  srli_template,
  &srli_iclass
};

static xtensa_opcode_internal ssa8b_opcode = {
  "ssa8b",
  3,
  ssa8b_template,
  &sar_iclass
};

static xtensa_opcode_internal ssa8l_opcode = {
  "ssa8l",
  3,
  ssa8l_template,
  &sar_iclass
};

static xtensa_opcode_internal ssai_opcode = {
  "ssai",
  3,
  ssai_template,
  &sari_iclass
};

static xtensa_opcode_internal ssl_opcode = {
  "ssl",
  3,
  ssl_template,
  &sar_iclass
};

static xtensa_opcode_internal ssr_opcode = {
  "ssr",
  3,
  ssr_template,
  &sar_iclass
};

static xtensa_opcode_internal sub_opcode = {
  "sub",
  3,
  sub_template,
  &addsub_iclass
};

static xtensa_opcode_internal subx2_opcode = {
  "subx2",
  3,
  subx2_template,
  &addsub_iclass
};

static xtensa_opcode_internal subx4_opcode = {
  "subx4",
  3,
  subx4_template,
  &addsub_iclass
};

static xtensa_opcode_internal subx8_opcode = {
  "subx8",
  3,
  subx8_template,
  &addsub_iclass
};

static xtensa_opcode_internal syscall_opcode = {
  "syscall",
  3,
  syscall_template,
  &syscall_iclass
};

static xtensa_opcode_internal waiti_opcode = {
  "waiti",
  3,
  waiti_template,
  &wait_iclass
};

static xtensa_opcode_internal wdtlb_opcode = {
  "wdtlb",
  3,
  wdtlb_template,
  &wtlb_iclass
};

static xtensa_opcode_internal witlb_opcode = {
  "witlb",
  3,
  witlb_template,
  &wtlb_iclass
};

static xtensa_opcode_internal wsr_opcode = {
  "wsr",
  3,
  wsr_template,
  &wsr_iclass
};

static xtensa_opcode_internal xor_opcode = {
  "xor",
  3,
  xor_template,
  &bit_iclass
};

static xtensa_opcode_internal xsr_opcode = {
  "xsr",
  3,
  xsr_template,
  &xsr_iclass
};

static xtensa_opcode_internal * opcodes[149] = {
  &abs_opcode,
  &add_opcode,
  &add_n_opcode,
  &addi_opcode,
  &addi_n_opcode,
  &addmi_opcode,
  &addx2_opcode,
  &addx4_opcode,
  &addx8_opcode,
  &and_opcode,
  &ball_opcode,
  &bany_opcode,
  &bbc_opcode,
  &bbci_opcode,
  &bbs_opcode,
  &bbsi_opcode,
  &beq_opcode,
  &beqi_opcode,
  &beqz_opcode,
  &beqz_n_opcode,
  &bge_opcode,
  &bgei_opcode,
  &bgeu_opcode,
  &bgeui_opcode,
  &bgez_opcode,
  &blt_opcode,
  &blti_opcode,
  &bltu_opcode,
  &bltui_opcode,
  &bltz_opcode,
  &bnall_opcode,
  &bne_opcode,
  &bnei_opcode,
  &bnez_opcode,
  &bnez_n_opcode,
  &bnone_opcode,
  &break_opcode,
  &break_n_opcode,
  &call0_opcode,
  &call12_opcode,
  &call4_opcode,
  &call8_opcode,
  &callx0_opcode,
  &callx12_opcode,
  &callx4_opcode,
  &callx8_opcode,
  &dhi_opcode,
  &dhwb_opcode,
  &dhwbi_opcode,
  &dii_opcode,
  &diwb_opcode,
  &diwbi_opcode,
  &dpfr_opcode,
  &dpfro_opcode,
  &dpfw_opcode,
  &dpfwo_opcode,
  &dsync_opcode,
  &entry_opcode,
  &esync_opcode,
  &excw_opcode,
  &extui_opcode,
  &idtlb_opcode,
  &idtlba_opcode,
  &ihi_opcode,
  &iii_opcode,
  &iitlb_opcode,
  &iitlba_opcode,
  &ipf_opcode,
  &isync_opcode,
  &j_opcode,
  &jx_opcode,
  &l16si_opcode,
  &l16ui_opcode,
  &l32e_opcode,
  &l32i_opcode,
  &l32i_n_opcode,
  &l32r_opcode,
  &l8ui_opcode,
  &ldct_opcode,
  &lict_opcode,
  &licw_opcode,
  &loop_opcode,
  &loopgtz_opcode,
  &loopnez_opcode,
  &memw_opcode,
  &mov_n_opcode,
  &moveqz_opcode,
  &movgez_opcode,
  &movi_opcode,
  &movi_n_opcode,
  &movltz_opcode,
  &movnez_opcode,
  &movsp_opcode,
  &neg_opcode,
  &nop_n_opcode,
  &nsa_opcode,
  &nsau_opcode,
  &or_opcode,
  &pdtlb_opcode,
  &pitlb_opcode,
  &rdtlb0_opcode,
  &rdtlb1_opcode,
  &ret_opcode,
  &ret_n_opcode,
  &retw_opcode,
  &retw_n_opcode,
  &rfde_opcode,
  &rfe_opcode,
  &rfi_opcode,
  &rfwo_opcode,
  &rfwu_opcode,
  &ritlb0_opcode,
  &ritlb1_opcode,
  &rotw_opcode,
  &rsil_opcode,
  &rsr_opcode,
  &rsync_opcode,
  &s16i_opcode,
  &s32e_opcode,
  &s32i_opcode,
  &s32i_n_opcode,
  &s8i_opcode,
  &sdct_opcode,
  &sict_opcode,
  &sicw_opcode,
  &simcall_opcode,
  &sll_opcode,
  &slli_opcode,
  &sra_opcode,
  &srai_opcode,
  &src_opcode,
  &srl_opcode,
  &srli_opcode,
  &ssa8b_opcode,
  &ssa8l_opcode,
  &ssai_opcode,
  &ssl_opcode,
  &ssr_opcode,
  &sub_opcode,
  &subx2_opcode,
  &subx4_opcode,
  &subx8_opcode,
  &syscall_opcode,
  &waiti_opcode,
  &wdtlb_opcode,
  &witlb_opcode,
  &wsr_opcode,
  &xor_opcode,
  &xsr_opcode
};

xtensa_opcode_internal **
get_opcodes (void)
{
  return &opcodes[0];
}

int
get_num_opcodes (void)
{
  return 149;
}

#define xtensa_abs_op 0
#define xtensa_add_op 1
#define xtensa_add_n_op 2
#define xtensa_addi_op 3
#define xtensa_addi_n_op 4
#define xtensa_addmi_op 5
#define xtensa_addx2_op 6
#define xtensa_addx4_op 7
#define xtensa_addx8_op 8
#define xtensa_and_op 9
#define xtensa_ball_op 10
#define xtensa_bany_op 11
#define xtensa_bbc_op 12
#define xtensa_bbci_op 13
#define xtensa_bbs_op 14
#define xtensa_bbsi_op 15
#define xtensa_beq_op 16
#define xtensa_beqi_op 17
#define xtensa_beqz_op 18
#define xtensa_beqz_n_op 19
#define xtensa_bge_op 20
#define xtensa_bgei_op 21
#define xtensa_bgeu_op 22
#define xtensa_bgeui_op 23
#define xtensa_bgez_op 24
#define xtensa_blt_op 25
#define xtensa_blti_op 26
#define xtensa_bltu_op 27
#define xtensa_bltui_op 28
#define xtensa_bltz_op 29
#define xtensa_bnall_op 30
#define xtensa_bne_op 31
#define xtensa_bnei_op 32
#define xtensa_bnez_op 33
#define xtensa_bnez_n_op 34
#define xtensa_bnone_op 35
#define xtensa_break_op 36
#define xtensa_break_n_op 37
#define xtensa_call0_op 38
#define xtensa_call12_op 39
#define xtensa_call4_op 40
#define xtensa_call8_op 41
#define xtensa_callx0_op 42
#define xtensa_callx12_op 43
#define xtensa_callx4_op 44
#define xtensa_callx8_op 45
#define xtensa_dhi_op 46
#define xtensa_dhwb_op 47
#define xtensa_dhwbi_op 48
#define xtensa_dii_op 49
#define xtensa_diwb_op 50
#define xtensa_diwbi_op 51
#define xtensa_dpfr_op 52
#define xtensa_dpfro_op 53
#define xtensa_dpfw_op 54
#define xtensa_dpfwo_op 55
#define xtensa_dsync_op 56
#define xtensa_entry_op 57
#define xtensa_esync_op 58
#define xtensa_excw_op 59
#define xtensa_extui_op 60
#define xtensa_idtlb_op 61
#define xtensa_idtlba_op 62
#define xtensa_ihi_op 63
#define xtensa_iii_op 64
#define xtensa_iitlb_op 65
#define xtensa_iitlba_op 66
#define xtensa_ipf_op 67
#define xtensa_isync_op 68
#define xtensa_j_op 69
#define xtensa_jx_op 70
#define xtensa_l16si_op 71
#define xtensa_l16ui_op 72
#define xtensa_l32e_op 73
#define xtensa_l32i_op 74
#define xtensa_l32i_n_op 75
#define xtensa_l32r_op 76
#define xtensa_l8ui_op 77
#define xtensa_ldct_op 78
#define xtensa_lict_op 79
#define xtensa_licw_op 80
#define xtensa_loop_op 81
#define xtensa_loopgtz_op 82
#define xtensa_loopnez_op 83
#define xtensa_memw_op 84
#define xtensa_mov_n_op 85
#define xtensa_moveqz_op 86
#define xtensa_movgez_op 87
#define xtensa_movi_op 88
#define xtensa_movi_n_op 89
#define xtensa_movltz_op 90
#define xtensa_movnez_op 91
#define xtensa_movsp_op 92
#define xtensa_neg_op 93
#define xtensa_nop_n_op 94
#define xtensa_nsa_op 95
#define xtensa_nsau_op 96
#define xtensa_or_op 97
#define xtensa_pdtlb_op 98
#define xtensa_pitlb_op 99
#define xtensa_rdtlb0_op 100
#define xtensa_rdtlb1_op 101
#define xtensa_ret_op 102
#define xtensa_ret_n_op 103
#define xtensa_retw_op 104
#define xtensa_retw_n_op 105
#define xtensa_rfde_op 106
#define xtensa_rfe_op 107
#define xtensa_rfi_op 108
#define xtensa_rfwo_op 109
#define xtensa_rfwu_op 110
#define xtensa_ritlb0_op 111
#define xtensa_ritlb1_op 112
#define xtensa_rotw_op 113
#define xtensa_rsil_op 114
#define xtensa_rsr_op 115
#define xtensa_rsync_op 116
#define xtensa_s16i_op 117
#define xtensa_s32e_op 118
#define xtensa_s32i_op 119
#define xtensa_s32i_n_op 120
#define xtensa_s8i_op 121
#define xtensa_sdct_op 122
#define xtensa_sict_op 123
#define xtensa_sicw_op 124
#define xtensa_simcall_op 125
#define xtensa_sll_op 126
#define xtensa_slli_op 127
#define xtensa_sra_op 128
#define xtensa_srai_op 129
#define xtensa_src_op 130
#define xtensa_srl_op 131
#define xtensa_srli_op 132
#define xtensa_ssa8b_op 133
#define xtensa_ssa8l_op 134
#define xtensa_ssai_op 135
#define xtensa_ssl_op 136
#define xtensa_ssr_op 137
#define xtensa_sub_op 138
#define xtensa_subx2_op 139
#define xtensa_subx4_op 140
#define xtensa_subx8_op 141
#define xtensa_syscall_op 142
#define xtensa_waiti_op 143
#define xtensa_wdtlb_op 144
#define xtensa_witlb_op 145
#define xtensa_wsr_op 146
#define xtensa_xor_op 147
#define xtensa_xsr_op 148

int
decode_insn (const xtensa_insnbuf insn)
{
  switch (get_op0_field (insn)) {
  case 0: /* QRST: op0=0000 */
    switch (get_op1_field (insn)) {
    case 3: /* RST3: op1=0011 */
      switch (get_op2_field (insn)) {
      case 8: /* MOVEQZ: op2=1000 */
        return xtensa_moveqz_op;
      case 9: /* MOVNEZ: op2=1001 */
        return xtensa_movnez_op;
      case 10: /* MOVLTZ: op2=1010 */
        return xtensa_movltz_op;
      case 11: /* MOVGEZ: op2=1011 */
        return xtensa_movgez_op;
      case 0: /* RSR: op2=0000 */
        return xtensa_rsr_op;
      case 1: /* WSR: op2=0001 */
        return xtensa_wsr_op;
      }
      break;
    case 9: /* LSI4: op1=1001 */
      switch (get_op2_field (insn)) {
      case 4: /* S32E: op2=0100 */
        return xtensa_s32e_op;
      case 0: /* L32E: op2=0000 */
        return xtensa_l32e_op;
      }
      break;
    case 4: /* EXTUI: op1=010x */
    case 5: /* EXTUI: op1=010x */
      return xtensa_extui_op;
    case 0: /* RST0: op1=0000 */
      switch (get_op2_field (insn)) {
      case 15: /* SUBX8: op2=1111 */
        return xtensa_subx8_op;
      case 0: /* ST0: op2=0000 */
        switch (get_r_field (insn)) {
        case 0: /* SNM0: r=0000 */
          switch (get_m_field (insn)) {
          case 2: /* JR: m=10 */
            switch (get_n_field (insn)) {
            case 0: /* RET: n=00 */
              return xtensa_ret_op;
            case 1: /* RETW: n=01 */
              return xtensa_retw_op;
            case 2: /* JX: n=10 */
              return xtensa_jx_op;
            }
            break;
          case 3: /* CALLX: m=11 */
            switch (get_n_field (insn)) {
            case 0: /* CALLX0: n=00 */
              return xtensa_callx0_op;
            case 1: /* CALLX4: n=01 */
              return xtensa_callx4_op;
            case 2: /* CALLX8: n=10 */
              return xtensa_callx8_op;
            case 3: /* CALLX12: n=11 */
              return xtensa_callx12_op;
            }
            break;
          }
          break;
        case 1: /* MOVSP: r=0001 */
          return xtensa_movsp_op;
        case 2: /* SYNC: r=0010 */
          switch (get_s_field (insn)) {
          case 0: /* SYNCT: s=0000 */
            switch (get_t_field (insn)) {
            case 2: /* ESYNC: t=0010 */
              return xtensa_esync_op;
            case 3: /* DSYNC: t=0011 */
              return xtensa_dsync_op;
            case 8: /* EXCW: t=1000 */
              return xtensa_excw_op;
            case 12: /* MEMW: t=1100 */
              return xtensa_memw_op;
            case 0: /* ISYNC: t=0000 */
              return xtensa_isync_op;
            case 1: /* RSYNC: t=0001 */
              return xtensa_rsync_op;
            }
            break;
          }
          break;
        case 4: /* BREAK: r=0100 */
          return xtensa_break_op;
        case 3: /* RFEI: r=0011 */
          switch (get_t_field (insn)) {
          case 0: /* RFET: t=0000 */
            switch (get_s_field (insn)) {
            case 2: /* RFDE: s=0010 */
              return xtensa_rfde_op;
            case 4: /* RFWO: s=0100 */
              return xtensa_rfwo_op;
            case 5: /* RFWU: s=0101 */
              return xtensa_rfwu_op;
            case 0: /* RFE: s=0000 */
              return xtensa_rfe_op;
            }
            break;
          case 1: /* RFI: t=0001 */
            return xtensa_rfi_op;
          }
          break;
        case 5: /* SCALL: r=0101 */
          switch (get_s_field (insn)) {
          case 0: /* SYSCALL: s=0000 */
            return xtensa_syscall_op;
          case 1: /* SIMCALL: s=0001 */
            return xtensa_simcall_op;
          }
          break;
        case 6: /* RSIL: r=0110 */
          return xtensa_rsil_op;
        case 7: /* WAITI: r=0111 */
          return xtensa_waiti_op;
        }
        break;
      case 1: /* AND: op2=0001 */
        return xtensa_and_op;
      case 2: /* OR: op2=0010 */
        return xtensa_or_op;
      case 3: /* XOR: op2=0011 */
        return xtensa_xor_op;
      case 4: /* ST1: op2=0100 */
        switch (get_r_field (insn)) {
        case 15: /* NSAU: r=1111 */
          return xtensa_nsau_op;
        case 0: /* SSR: r=0000 */
          return xtensa_ssr_op;
        case 1: /* SSL: r=0001 */
          return xtensa_ssl_op;
        case 2: /* SSA8L: r=0010 */
          return xtensa_ssa8l_op;
        case 3: /* SSA8B: r=0011 */
          return xtensa_ssa8b_op;
        case 4: /* SSAI: r=0100 */
          return xtensa_ssai_op;
        case 8: /* ROTW: r=1000 */
          return xtensa_rotw_op;
        case 14: /* NSA: r=1110 */
          return xtensa_nsa_op;
        }
        break;
      case 8: /* ADD: op2=1000 */
        return xtensa_add_op;
      case 5: /* ST4: op2=0101 */
        switch (get_r_field (insn)) {
        case 15: /* RDTLB1: r=1111 */
          return xtensa_rdtlb1_op;
        case 0: /* IITLBA: r=0000 */
          return xtensa_iitlba_op;
        case 3: /* RITLB0: r=0011 */
          return xtensa_ritlb0_op;
        case 4: /* IITLB: r=0100 */
          return xtensa_iitlb_op;
        case 8: /* IDTLBA: r=1000 */
          return xtensa_idtlba_op;
        case 5: /* PITLB: r=0101 */
          return xtensa_pitlb_op;
        case 6: /* WITLB: r=0110 */
          return xtensa_witlb_op;
        case 7: /* RITLB1: r=0111 */
          return xtensa_ritlb1_op;
        case 11: /* RDTLB0: r=1011 */
          return xtensa_rdtlb0_op;
        case 12: /* IDTLB: r=1100 */
          return xtensa_idtlb_op;
        case 13: /* PDTLB: r=1101 */
          return xtensa_pdtlb_op;
        case 14: /* WDTLB: r=1110 */
          return xtensa_wdtlb_op;
        }
        break;
      case 6: /* RT0: op2=0110 */
        switch (get_s_field (insn)) {
        case 0: /* NEG: s=0000 */
          return xtensa_neg_op;
        case 1: /* ABS: s=0001 */
          return xtensa_abs_op;
        }
        break;
      case 9: /* ADDX2: op2=1001 */
        return xtensa_addx2_op;
      case 10: /* ADDX4: op2=1010 */
        return xtensa_addx4_op;
      case 11: /* ADDX8: op2=1011 */
        return xtensa_addx8_op;
      case 12: /* SUB: op2=1100 */
        return xtensa_sub_op;
      case 13: /* SUBX2: op2=1101 */
        return xtensa_subx2_op;
      case 14: /* SUBX4: op2=1110 */
        return xtensa_subx4_op;
      }
      break;
    case 1: /* RST1: op1=0001 */
      switch (get_op2_field (insn)) {
      case 15: /* IMP: op2=1111 */
        switch (get_r_field (insn)) {
        case 0: /* LICT: r=0000 */
          return xtensa_lict_op;
        case 1: /* SICT: r=0001 */
          return xtensa_sict_op;
        case 2: /* LICW: r=0010 */
          return xtensa_licw_op;
        case 3: /* SICW: r=0011 */
          return xtensa_sicw_op;
        case 8: /* LDCT: r=1000 */
          return xtensa_ldct_op;
        case 9: /* SDCT: r=1001 */
          return xtensa_sdct_op;
        }
        break;
      case 0: /* SLLI: op2=000x */
      case 1: /* SLLI: op2=000x */
        return xtensa_slli_op;
      case 2: /* SRAI: op2=001x */
      case 3: /* SRAI: op2=001x */
        return xtensa_srai_op;
      case 4: /* SRLI: op2=0100 */
        return xtensa_srli_op;
      case 8: /* SRC: op2=1000 */
        return xtensa_src_op;
      case 9: /* SRL: op2=1001 */
        return xtensa_srl_op;
      case 6: /* XSR: op2=0110 */
        return xtensa_xsr_op;
      case 10: /* SLL: op2=1010 */
        return xtensa_sll_op;
      case 11: /* SRA: op2=1011 */
        return xtensa_sra_op;
      }
      break;
    }
    break;
  case 1: /* L32R: op0=0001 */
    return xtensa_l32r_op;
  case 2: /* LSAI: op0=0010 */
    switch (get_r_field (insn)) {
    case 0: /* L8UI: r=0000 */
      return xtensa_l8ui_op;
    case 1: /* L16UI: r=0001 */
      return xtensa_l16ui_op;
    case 2: /* L32I: r=0010 */
      return xtensa_l32i_op;
    case 4: /* S8I: r=0100 */
      return xtensa_s8i_op;
    case 5: /* S16I: r=0101 */
      return xtensa_s16i_op;
    case 9: /* L16SI: r=1001 */
      return xtensa_l16si_op;
    case 6: /* S32I: r=0110 */
      return xtensa_s32i_op;
    case 7: /* CACHE: r=0111 */
      switch (get_t_field (insn)) {
      case 15: /* III: t=1111 */
        return xtensa_iii_op;
      case 0: /* DPFR: t=0000 */
        return xtensa_dpfr_op;
      case 1: /* DPFW: t=0001 */
        return xtensa_dpfw_op;
      case 2: /* DPFRO: t=0010 */
        return xtensa_dpfro_op;
      case 4: /* DHWB: t=0100 */
        return xtensa_dhwb_op;
      case 3: /* DPFWO: t=0011 */
        return xtensa_dpfwo_op;
      case 8: /* DCE: t=1000 */
        switch (get_op1_field (insn)) {
        case 4: /* DIWB: op1=0100 */
          return xtensa_diwb_op;
        case 5: /* DIWBI: op1=0101 */
          return xtensa_diwbi_op;
        }
        break;
      case 5: /* DHWBI: t=0101 */
        return xtensa_dhwbi_op;
      case 6: /* DHI: t=0110 */
        return xtensa_dhi_op;
      case 7: /* DII: t=0111 */
        return xtensa_dii_op;
      case 12: /* IPF: t=1100 */
        return xtensa_ipf_op;
      case 14: /* IHI: t=1110 */
        return xtensa_ihi_op;
      }
      break;
    case 10: /* MOVI: r=1010 */
      return xtensa_movi_op;
    case 12: /* ADDI: r=1100 */
      return xtensa_addi_op;
    case 13: /* ADDMI: r=1101 */
      return xtensa_addmi_op;
    }
    break;
  case 8: /* L32I.N: op0=1000 */
    return xtensa_l32i_n_op;
  case 5: /* CALL: op0=0101 */
    switch (get_n_field (insn)) {
    case 0: /* CALL0: n=00 */
      return xtensa_call0_op;
    case 1: /* CALL4: n=01 */
      return xtensa_call4_op;
    case 2: /* CALL8: n=10 */
      return xtensa_call8_op;
    case 3: /* CALL12: n=11 */
      return xtensa_call12_op;
    }
    break;
  case 6: /* SI: op0=0110 */
    switch (get_n_field (insn)) {
    case 0: /* J: n=00 */
      return xtensa_j_op;
    case 1: /* BZ: n=01 */
      switch (get_m_field (insn)) {
      case 0: /* BEQZ: m=00 */
        return xtensa_beqz_op;
      case 1: /* BNEZ: m=01 */
        return xtensa_bnez_op;
      case 2: /* BLTZ: m=10 */
        return xtensa_bltz_op;
      case 3: /* BGEZ: m=11 */
        return xtensa_bgez_op;
      }
      break;
    case 2: /* BI0: n=10 */
      switch (get_m_field (insn)) {
      case 0: /* BEQI: m=00 */
        return xtensa_beqi_op;
      case 1: /* BNEI: m=01 */
        return xtensa_bnei_op;
      case 2: /* BLTI: m=10 */
        return xtensa_blti_op;
      case 3: /* BGEI: m=11 */
        return xtensa_bgei_op;
      }
      break;
    case 3: /* BI1: n=11 */
      switch (get_m_field (insn)) {
      case 0: /* ENTRY: m=00 */
        return xtensa_entry_op;
      case 1: /* B1: m=01 */
        switch (get_r_field (insn)) {
        case 8: /* LOOP: r=1000 */
          return xtensa_loop_op;
        case 9: /* LOOPNEZ: r=1001 */
          return xtensa_loopnez_op;
        case 10: /* LOOPGTZ: r=1010 */
          return xtensa_loopgtz_op;
        }
        break;
      case 2: /* BLTUI: m=10 */
        return xtensa_bltui_op;
      case 3: /* BGEUI: m=11 */
        return xtensa_bgeui_op;
      }
      break;
    }
    break;
  case 9: /* S32I.N: op0=1001 */
    return xtensa_s32i_n_op;
  case 10: /* ADD.N: op0=1010 */
    return xtensa_add_n_op;
  case 7: /* B: op0=0111 */
    switch (get_r_field (insn)) {
    case 6: /* BBCI: r=011x */
    case 7: /* BBCI: r=011x */
      return xtensa_bbci_op;
    case 0: /* BNONE: r=0000 */
      return xtensa_bnone_op;
    case 1: /* BEQ: r=0001 */
      return xtensa_beq_op;
    case 2: /* BLT: r=0010 */
      return xtensa_blt_op;
    case 4: /* BALL: r=0100 */
      return xtensa_ball_op;
    case 14: /* BBSI: r=111x */
    case 15: /* BBSI: r=111x */
      return xtensa_bbsi_op;
    case 3: /* BLTU: r=0011 */
      return xtensa_bltu_op;
    case 5: /* BBC: r=0101 */
      return xtensa_bbc_op;
    case 8: /* BANY: r=1000 */
      return xtensa_bany_op;
    case 9: /* BNE: r=1001 */
      return xtensa_bne_op;
    case 10: /* BGE: r=1010 */
      return xtensa_bge_op;
    case 11: /* BGEU: r=1011 */
      return xtensa_bgeu_op;
    case 12: /* BNALL: r=1100 */
      return xtensa_bnall_op;
    case 13: /* BBS: r=1101 */
      return xtensa_bbs_op;
    }
    break;
  case 11: /* ADDI.N: op0=1011 */
    return xtensa_addi_n_op;
  case 12: /* ST2: op0=1100 */
    switch (get_i_field (insn)) {
    case 0: /* MOVI.N: i=0 */
      return xtensa_movi_n_op;
    case 1: /* BZ6: i=1 */
      switch (get_z_field (insn)) {
      case 0: /* BEQZ.N: z=0 */
        return xtensa_beqz_n_op;
      case 1: /* BNEZ.N: z=1 */
        return xtensa_bnez_n_op;
      }
      break;
    }
    break;
  case 13: /* ST3: op0=1101 */
    switch (get_r_field (insn)) {
    case 15: /* S3: r=1111 */
      switch (get_t_field (insn)) {
      case 0: /* RET.N: t=0000 */
        return xtensa_ret_n_op;
      case 1: /* RETW.N: t=0001 */
        return xtensa_retw_n_op;
      case 2: /* BREAK.N: t=0010 */
        return xtensa_break_n_op;
      case 3: /* NOP.N: t=0011 */
        return xtensa_nop_n_op;
      }
      break;
    case 0: /* MOV.N: r=0000 */
      return xtensa_mov_n_op;
    }
    break;
  }
  return XTENSA_UNDEFINED;
}

int
interface_version (void)
{
  return 3;
}

static struct config_struct config_table[] = {
  {"IsaMemoryOrder", "BigEndian"},
  {"PIFReadDataBits", "128"},
  {"PIFWriteDataBits", "128"},
  {"IsaCoprocessorCount", "0"},
  {"IsaUseBooleans", "0"},
  {"IsaUseDensityInstruction", "1"},
  {0, 0}
};

struct config_struct * get_config_table (void);

struct config_struct *
get_config_table (void)
{
  return config_table;
}

xtensa_isa_module xtensa_isa_modules[] = {
  { get_num_opcodes, get_opcodes, decode_insn, get_config_table },
  { 0, 0, 0, 0 }
};
