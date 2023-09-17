/*
 * Copyright (c) 2013-2019, Intel Corporation
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *  * Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *  * Neither the name of Intel Corporation nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include "pt_ild.h"
#include "pti-imm-defs.h"
#include "pti-imm.h"
#include "pti-modrm-defs.h"
#include "pti-modrm.h"
#include "pti-disp-defs.h"
#include "pti-disp.h"
#include "pti-disp_default.h"
#include "pti-sib.h"

#include <string.h>


static const uint8_t eamode_table[2][4] = {
	/* Default: */ {
		/* ptem_unknown = */	ptem_unknown,
		/* ptem_16bit = */	ptem_16bit,
		/* ptem_32bit = */	ptem_32bit,
		/* ptem_64bit = */	ptem_64bit
	},

	/* With Address-size prefix (0x67): */ {
		/* ptem_unknown = */	ptem_unknown,
		/* ptem_16bit = */	ptem_32bit,
		/* ptem_32bit = */	ptem_16bit,
		/* ptem_64bit = */	ptem_32bit
	}
};

/* SOME ACCESSORS */

static inline uint8_t get_byte(const struct pt_ild *ild, uint8_t i)
{
	return ild->itext[i];
}

static inline uint8_t const *get_byte_ptr(const struct pt_ild *ild, uint8_t i)
{
	return ild->itext + i;
}

static inline int mode_64b(const struct pt_ild *ild)
{
	return ild->mode == ptem_64bit;
}

static inline int mode_32b(const struct pt_ild *ild)
{
	return ild->mode == ptem_32bit;
}

static inline int bits_match(uint8_t x, uint8_t mask, uint8_t target)
{
	return (x & mask) == target;
}

static inline enum pt_exec_mode
pti_get_nominal_eosz_non64(const struct pt_ild *ild)
{
	if (mode_32b(ild)) {
		if (ild->u.s.osz)
			return ptem_16bit;
		return ptem_32bit;
	}
	if (ild->u.s.osz)
		return ptem_32bit;
	return ptem_16bit;
}

static inline enum pt_exec_mode
pti_get_nominal_eosz(const struct pt_ild *ild)
{
	if (mode_64b(ild)) {
		if (ild->u.s.rex_w)
			return ptem_64bit;
		if (ild->u.s.osz)
			return ptem_16bit;
		return ptem_32bit;
	}
	return pti_get_nominal_eosz_non64(ild);
}

static inline enum pt_exec_mode
pti_get_nominal_eosz_df64(const struct pt_ild *ild)
{
	if (mode_64b(ild)) {
		if (ild->u.s.rex_w)
			return ptem_64bit;
		if (ild->u.s.osz)
			return ptem_16bit;
		/* only this next line of code is different relative
		   to pti_get_nominal_eosz(), above */
		return ptem_64bit;
	}
	return pti_get_nominal_eosz_non64(ild);
}

static inline enum pt_exec_mode
pti_get_nominal_easz_non64(const struct pt_ild *ild)
{
	if (mode_32b(ild)) {
		if (ild->u.s.asz)
			return ptem_16bit;
		return ptem_32bit;
	}
	if (ild->u.s.asz)
		return ptem_32bit;
	return ptem_16bit;
}

static inline enum pt_exec_mode
pti_get_nominal_easz(const struct pt_ild *ild)
{
	if (mode_64b(ild)) {
		if (ild->u.s.asz)
			return ptem_32bit;
		return ptem_64bit;
	}
	return pti_get_nominal_easz_non64(ild);
}

static inline int resolve_z(uint8_t *pbytes, enum pt_exec_mode eosz)
{
	static const uint8_t bytes[] = { 2, 4, 4 };
	unsigned int idx;

	if (!pbytes)
		return -pte_internal;

	idx = (unsigned int) eosz - 1;
	if (sizeof(bytes) <= idx)
		return -pte_bad_insn;

	*pbytes = bytes[idx];
	return 0;
}

static inline int resolve_v(uint8_t *pbytes, enum pt_exec_mode eosz)
{
	static const uint8_t bytes[] = { 2, 4, 8 };
	unsigned int idx;

	if (!pbytes)
		return -pte_internal;

	idx = (unsigned int) eosz - 1;
	if (sizeof(bytes) <= idx)
		return -pte_bad_insn;

	*pbytes = bytes[idx];
	return 0;
}

/*  DECODERS */

static int set_imm_bytes(struct pt_ild *ild)
{
	/*: set ild->imm1_bytes and  ild->imm2_bytes for maps 0/1 */
	static uint8_t const *const map_map[] = {
		/* map 0 */ imm_bytes_map_0x0,
		/* map 1 */ imm_bytes_map_0x0F
	};
	uint8_t map, imm_code;

	if (!ild)
		return -pte_internal;

	map = ild->map;

	if ((sizeof(map_map) / sizeof(*map_map)) <= map)
		return 0;

	imm_code = map_map[map][ild->nominal_opcode];
	switch (imm_code) {
	case PTI_IMM_NONE:
	case PTI_0_IMM_WIDTH_CONST_l2:
	default:
		return 0;

	case PTI_UIMM8_IMM_WIDTH_CONST_l2:
		ild->imm1_bytes = 1;
		return 0;

	case PTI_SIMM8_IMM_WIDTH_CONST_l2:
		ild->imm1_bytes = 1;
		return 0;

	case PTI_SIMMz_IMM_WIDTH_OSZ_NONTERM_EOSZ_l2:
		/* SIMMz(eosz) */
		return resolve_z(&ild->imm1_bytes, pti_get_nominal_eosz(ild));

	case PTI_UIMMv_IMM_WIDTH_OSZ_NONTERM_EOSZ_l2:
		/* UIMMv(eosz) */
		return resolve_v(&ild->imm1_bytes, pti_get_nominal_eosz(ild));

	case PTI_UIMM16_IMM_WIDTH_CONST_l2:
		ild->imm1_bytes = 2;
		return 0;

	case PTI_SIMMz_IMM_WIDTH_OSZ_NONTERM_DF64_EOSZ_l2:
		/* push defaults to eosz64 in 64b mode, then uses SIMMz */
		return resolve_z(&ild->imm1_bytes,
				 pti_get_nominal_eosz_df64(ild));

	case PTI_RESOLVE_BYREG_IMM_WIDTH_map0x0_op0xf7_l1:
		if (ild->map == PTI_MAP_0 && pti_get_modrm_reg(ild) < 2) {
			return resolve_z(&ild->imm1_bytes,
					 pti_get_nominal_eosz(ild));
		}
		return 0;

	case PTI_RESOLVE_BYREG_IMM_WIDTH_map0x0_op0xc7_l1:
		if (ild->map == PTI_MAP_0 && pti_get_modrm_reg(ild) == 0) {
			return resolve_z(&ild->imm1_bytes,
					 pti_get_nominal_eosz(ild));
		}
		return 0;

	case PTI_RESOLVE_BYREG_IMM_WIDTH_map0x0_op0xf6_l1:
		if (ild->map == PTI_MAP_0 && pti_get_modrm_reg(ild) < 2)
			ild->imm1_bytes = 1;

		return 0;

	case PTI_IMM_hasimm_map0x0_op0xc8_l1:
		if (ild->map == PTI_MAP_0) {
			/*enter -> imm1=2, imm2=1 */
			ild->imm1_bytes = 2;
			ild->imm2_bytes = 1;
		}
		return 0;

	case PTI_IMM_hasimm_map0x0F_op0x78_l1:
		/* AMD SSE4a (insertq/extrq use  osz/f2) vs vmread
		 * (no prefixes)
		 */
		if (ild->map == PTI_MAP_1) {
			if (ild->u.s.osz || ild->u.s.last_f2f3 == 2) {
				ild->imm1_bytes = 1;
				ild->imm2_bytes = 1;
			}
		}
		return 0;
	}
}

static int imm_dec(struct pt_ild *ild, uint8_t length)
{
	int errcode;

	if (!ild)
		return -pte_internal;

	if (ild->map == PTI_MAP_AMD3DNOW) {
		if (ild->max_bytes <= length)
			return -pte_bad_insn;

		ild->nominal_opcode = get_byte(ild, length);
		return length + 1;
	}

	errcode = set_imm_bytes(ild);
	if (errcode < 0)
		return errcode;

	length += ild->imm1_bytes;
	length += ild->imm2_bytes;
	if (ild->max_bytes < length)
		return -pte_bad_insn;

	return length;
}

static int compute_disp_dec(struct pt_ild *ild)
{
	/* set ild->disp_bytes for maps 0 and 1. */
	static uint8_t const *const map_map[] = {
		/* map 0 */ disp_bytes_map_0x0,
		/* map 1 */ disp_bytes_map_0x0F
	};
	uint8_t map, disp_kind;

	if (!ild)
		return -pte_internal;

	if (0 < ild->disp_bytes)
		return 0;

	map = ild->map;

	if ((sizeof(map_map) / sizeof(*map_map)) <= map)
		return 0;

	disp_kind = map_map[map][ild->nominal_opcode];
	switch (disp_kind) {
	case PTI_DISP_NONE:
		ild->disp_bytes = 0;
		return 0;

	case PTI_PRESERVE_DEFAULT:
		/* nothing to do */
		return 0;

	case PTI_BRDISP8:
		ild->disp_bytes = 1;
		return 0;

	case PTI_DISP_BUCKET_0_l1:
		/* BRDISPz(eosz) for 16/32 modes, and BRDISP32 for 64b mode */
		if (mode_64b(ild)) {
			ild->disp_bytes = 4;
			return 0;
		}

		return resolve_z(&ild->disp_bytes,
				 pti_get_nominal_eosz(ild));

	case PTI_MEMDISPv_DISP_WIDTH_ASZ_NONTERM_EASZ_l2:
		/* MEMDISPv(easz) */
		return resolve_v(&ild->disp_bytes, pti_get_nominal_easz(ild));

	case PTI_BRDISPz_BRDISP_WIDTH_OSZ_NONTERM_EOSZ_l2:
		/* BRDISPz(eosz) for 16/32/64 modes */
		return resolve_z(&ild->disp_bytes, pti_get_nominal_eosz(ild));

	case PTI_RESOLVE_BYREG_DISP_map0x0_op0xc7_l1:
		/* reg=0 -> preserve, reg=7 -> BRDISPz(eosz) */
		if (ild->map == PTI_MAP_0 && pti_get_modrm_reg(ild) == 7) {
			return resolve_z(&ild->disp_bytes,
					 pti_get_nominal_eosz(ild));
		}
		return 0;

	default:
		return -pte_bad_insn;
	}
}

static int disp_dec(struct pt_ild *ild, uint8_t length)
{
	uint8_t disp_bytes;
	int errcode;

	if (!ild)
		return -pte_internal;

	errcode = compute_disp_dec(ild);
	if (errcode < 0)
		return errcode;

	disp_bytes = ild->disp_bytes;
	if (disp_bytes == 0)
		return imm_dec(ild, length);

	if (length + disp_bytes > ild->max_bytes)
		return -pte_bad_insn;

	/*Record only position; must be able to re-read itext bytes for actual
	   value. (SMC/CMC issue). */
	ild->disp_pos = length;

	return imm_dec(ild, length + disp_bytes);
}

static int sib_dec(struct pt_ild *ild, uint8_t length)
{
	uint8_t sib;

	if (!ild)
		return -pte_internal;

	if (ild->max_bytes <= length)
		return -pte_bad_insn;

	sib = get_byte(ild, length);
	if ((sib & 0x07) == 0x05 && pti_get_modrm_mod(ild) == 0)
		ild->disp_bytes = 4;

	return disp_dec(ild, length + 1);
}

static int modrm_dec(struct pt_ild *ild, uint8_t length)
{
	static uint8_t const *const has_modrm_2d[2] = {
		has_modrm_map_0x0,
		has_modrm_map_0x0F
	};
	int has_modrm = PTI_MODRM_FALSE;
	pti_map_enum_t map;

	if (!ild)
		return -pte_internal;

	map = pti_get_map(ild);
	if (map >= PTI_MAP_2)
		has_modrm = PTI_MODRM_TRUE;
	else
		has_modrm = has_modrm_2d[map][ild->nominal_opcode];

	if (has_modrm == PTI_MODRM_FALSE || has_modrm == PTI_MODRM_UNDEF)
		return disp_dec(ild, length);

	/* really >= here because we have not eaten the byte yet */
	if (length >= ild->max_bytes)
		return -pte_bad_insn;

	ild->modrm_byte = get_byte(ild, length);

	if (has_modrm != PTI_MODRM_IGNORE_MOD) {
		/* set disp_bytes and sib using simple tables */

		uint8_t eamode = eamode_table[ild->u.s.asz][ild->mode];
		uint8_t mod = (uint8_t) pti_get_modrm_mod(ild);
		uint8_t rm = (uint8_t) pti_get_modrm_rm(ild);
		uint8_t sib;

		ild->disp_bytes = disp_default[eamode][mod][rm];

		sib = has_sib[eamode][mod][rm];
		if (sib)
			return sib_dec(ild, length + 1);
	}

	return disp_dec(ild, length + 1);
}

static inline int get_next_as_opcode(struct pt_ild *ild, uint8_t length)
{
	if (!ild)
		return -pte_internal;

	if (ild->max_bytes <= length)
		return -pte_bad_insn;

	ild->nominal_opcode = get_byte(ild, length);

	return modrm_dec(ild, length + 1);
}

static int opcode_dec(struct pt_ild *ild, uint8_t length)
{
	uint8_t b, m;

	if (!ild)
		return -pte_internal;

	/*no need to check max_bytes - it was checked in previous scanners */
	b = get_byte(ild, length);
	if (b != 0x0F) {	/* 1B opcodes, map 0 */
		ild->map = PTI_MAP_0;
		ild->nominal_opcode = b;

		return modrm_dec(ild, length + 1);
	}

	length++;		/* eat the 0x0F */

	if (ild->max_bytes <= length)
		return -pte_bad_insn;

	/* 0x0F opcodes MAPS 1,2,3 */
	m = get_byte(ild, length);
	if (m == 0x38) {
		ild->map = PTI_MAP_2;

		return get_next_as_opcode(ild, length + 1);
	} else if (m == 0x3A) {
		ild->map = PTI_MAP_3;
		ild->imm1_bytes = 1;

		return get_next_as_opcode(ild, length + 1);
	} else if (bits_match(m, 0xf8, 0x38)) {
		ild->map = PTI_MAP_INVALID;

		return get_next_as_opcode(ild, length + 1);
	} else if (m == 0x0F) {	/* 3dNow */
		ild->map = PTI_MAP_AMD3DNOW;
		ild->imm1_bytes = 1;
		/* real opcode is in immediate later on, but we need an
		 * opcode now. */
		ild->nominal_opcode = 0x0F;

		return modrm_dec(ild, length + 1);
	} else {	/* map 1 (simple two byte opcodes) */
		ild->nominal_opcode = m;
		ild->map = PTI_MAP_1;

		return modrm_dec(ild, length + 1);
	}
}

typedef int (*prefix_decoder)(struct pt_ild *ild, uint8_t length, uint8_t rex);

static int prefix_osz(struct pt_ild *ild, uint8_t length, uint8_t rex);
static int prefix_asz(struct pt_ild *ild, uint8_t length, uint8_t rex);
static int prefix_lock(struct pt_ild *ild, uint8_t length, uint8_t rex);
static int prefix_f2(struct pt_ild *ild, uint8_t length, uint8_t rex);
static int prefix_f3(struct pt_ild *ild, uint8_t length, uint8_t rex);
static int prefix_rex(struct pt_ild *ild, uint8_t length, uint8_t rex);
static int prefix_vex_c4(struct pt_ild *ild, uint8_t length, uint8_t rex);
static int prefix_vex_c5(struct pt_ild *ild, uint8_t length, uint8_t rex);
static int prefix_evex(struct pt_ild *ild, uint8_t length, uint8_t rex);
static int prefix_ignore(struct pt_ild *ild, uint8_t length, uint8_t rex);
static int prefix_done(struct pt_ild *ild, uint8_t length, uint8_t rex);

static const prefix_decoder prefix_table[256] = {
	/* 00 = */ prefix_done,
	/* 01 = */ prefix_done,
	/* 02 = */ prefix_done,
	/* 03 = */ prefix_done,
	/* 04 = */ prefix_done,
	/* 05 = */ prefix_done,
	/* 06 = */ prefix_done,
	/* 07 = */ prefix_done,
	/* 08 = */ prefix_done,
	/* 09 = */ prefix_done,
	/* 0a = */ prefix_done,
	/* 0b = */ prefix_done,
	/* 0c = */ prefix_done,
	/* 0d = */ prefix_done,
	/* 0e = */ prefix_done,
	/* 0f = */ prefix_done,

	/* 10 = */ prefix_done,
	/* 11 = */ prefix_done,
	/* 12 = */ prefix_done,
	/* 13 = */ prefix_done,
	/* 14 = */ prefix_done,
	/* 15 = */ prefix_done,
	/* 16 = */ prefix_done,
	/* 17 = */ prefix_done,
	/* 18 = */ prefix_done,
	/* 19 = */ prefix_done,
	/* 1a = */ prefix_done,
	/* 1b = */ prefix_done,
	/* 1c = */ prefix_done,
	/* 1d = */ prefix_done,
	/* 1e = */ prefix_done,
	/* 1f = */ prefix_done,

	/* 20 = */ prefix_done,
	/* 21 = */ prefix_done,
	/* 22 = */ prefix_done,
	/* 23 = */ prefix_done,
	/* 24 = */ prefix_done,
	/* 25 = */ prefix_done,
	/* 26 = */ prefix_ignore,
	/* 27 = */ prefix_done,
	/* 28 = */ prefix_done,
	/* 29 = */ prefix_done,
	/* 2a = */ prefix_done,
	/* 2b = */ prefix_done,
	/* 2c = */ prefix_done,
	/* 2d = */ prefix_done,
	/* 2e = */ prefix_ignore,
	/* 2f = */ prefix_done,

	/* 30 = */ prefix_done,
	/* 31 = */ prefix_done,
	/* 32 = */ prefix_done,
	/* 33 = */ prefix_done,
	/* 34 = */ prefix_done,
	/* 35 = */ prefix_done,
	/* 36 = */ prefix_ignore,
	/* 37 = */ prefix_done,
	/* 38 = */ prefix_done,
	/* 39 = */ prefix_done,
	/* 3a = */ prefix_done,
	/* 3b = */ prefix_done,
	/* 3c = */ prefix_done,
	/* 3d = */ prefix_done,
	/* 3e = */ prefix_ignore,
	/* 3f = */ prefix_done,

	/* 40 = */ prefix_rex,
	/* 41 = */ prefix_rex,
	/* 42 = */ prefix_rex,
	/* 43 = */ prefix_rex,
	/* 44 = */ prefix_rex,
	/* 45 = */ prefix_rex,
	/* 46 = */ prefix_rex,
	/* 47 = */ prefix_rex,
	/* 48 = */ prefix_rex,
	/* 49 = */ prefix_rex,
	/* 4a = */ prefix_rex,
	/* 4b = */ prefix_rex,
	/* 4c = */ prefix_rex,
	/* 4d = */ prefix_rex,
	/* 4e = */ prefix_rex,
	/* 4f = */ prefix_rex,

	/* 50 = */ prefix_done,
	/* 51 = */ prefix_done,
	/* 52 = */ prefix_done,
	/* 53 = */ prefix_done,
	/* 54 = */ prefix_done,
	/* 55 = */ prefix_done,
	/* 56 = */ prefix_done,
	/* 57 = */ prefix_done,
	/* 58 = */ prefix_done,
	/* 59 = */ prefix_done,
	/* 5a = */ prefix_done,
	/* 5b = */ prefix_done,
	/* 5c = */ prefix_done,
	/* 5d = */ prefix_done,
	/* 5e = */ prefix_done,
	/* 5f = */ prefix_done,

	/* 60 = */ prefix_done,
	/* 61 = */ prefix_done,
	/* 62 = */ prefix_evex,
	/* 63 = */ prefix_done,
	/* 64 = */ prefix_ignore,
	/* 65 = */ prefix_ignore,
	/* 66 = */ prefix_osz,
	/* 67 = */ prefix_asz,
	/* 68 = */ prefix_done,
	/* 69 = */ prefix_done,
	/* 6a = */ prefix_done,
	/* 6b = */ prefix_done,
	/* 6c = */ prefix_done,
	/* 6d = */ prefix_done,
	/* 6e = */ prefix_done,
	/* 6f = */ prefix_done,

	/* 70 = */ prefix_done,
	/* 71 = */ prefix_done,
	/* 72 = */ prefix_done,
	/* 73 = */ prefix_done,
	/* 74 = */ prefix_done,
	/* 75 = */ prefix_done,
	/* 76 = */ prefix_done,
	/* 77 = */ prefix_done,
	/* 78 = */ prefix_done,
	/* 79 = */ prefix_done,
	/* 7a = */ prefix_done,
	/* 7b = */ prefix_done,
	/* 7c = */ prefix_done,
	/* 7d = */ prefix_done,
	/* 7e = */ prefix_done,
	/* 7f = */ prefix_done,

	/* 80 = */ prefix_done,
	/* 81 = */ prefix_done,
	/* 82 = */ prefix_done,
	/* 83 = */ prefix_done,
	/* 84 = */ prefix_done,
	/* 85 = */ prefix_done,
	/* 86 = */ prefix_done,
	/* 87 = */ prefix_done,
	/* 88 = */ prefix_done,
	/* 89 = */ prefix_done,
	/* 8a = */ prefix_done,
	/* 8b = */ prefix_done,
	/* 8c = */ prefix_done,
	/* 8d = */ prefix_done,
	/* 8e = */ prefix_done,
	/* 8f = */ prefix_done,

	/* 90 = */ prefix_done,
	/* 91 = */ prefix_done,
	/* 92 = */ prefix_done,
	/* 93 = */ prefix_done,
	/* 94 = */ prefix_done,
	/* 95 = */ prefix_done,
	/* 96 = */ prefix_done,
	/* 97 = */ prefix_done,
	/* 98 = */ prefix_done,
	/* 99 = */ prefix_done,
	/* 9a = */ prefix_done,
	/* 9b = */ prefix_done,
	/* 9c = */ prefix_done,
	/* 9d = */ prefix_done,
	/* 9e = */ prefix_done,
	/* 9f = */ prefix_done,

	/* a0 = */ prefix_done,
	/* a1 = */ prefix_done,
	/* a2 = */ prefix_done,
	/* a3 = */ prefix_done,
	/* a4 = */ prefix_done,
	/* a5 = */ prefix_done,
	/* a6 = */ prefix_done,
	/* a7 = */ prefix_done,
	/* a8 = */ prefix_done,
	/* a9 = */ prefix_done,
	/* aa = */ prefix_done,
	/* ab = */ prefix_done,
	/* ac = */ prefix_done,
	/* ad = */ prefix_done,
	/* ae = */ prefix_done,
	/* af = */ prefix_done,

	/* b0 = */ prefix_done,
	/* b1 = */ prefix_done,
	/* b2 = */ prefix_done,
	/* b3 = */ prefix_done,
	/* b4 = */ prefix_done,
	/* b5 = */ prefix_done,
	/* b6 = */ prefix_done,
	/* b7 = */ prefix_done,
	/* b8 = */ prefix_done,
	/* b9 = */ prefix_done,
	/* ba = */ prefix_done,
	/* bb = */ prefix_done,
	/* bc = */ prefix_done,
	/* bd = */ prefix_done,
	/* be = */ prefix_done,
	/* bf = */ prefix_done,

	/* c0 = */ prefix_done,
	/* c1 = */ prefix_done,
	/* c2 = */ prefix_done,
	/* c3 = */ prefix_done,
	/* c4 = */ prefix_vex_c4,
	/* c5 = */ prefix_vex_c5,
	/* c6 = */ prefix_done,
	/* c7 = */ prefix_done,
	/* c8 = */ prefix_done,
	/* c9 = */ prefix_done,
	/* ca = */ prefix_done,
	/* cb = */ prefix_done,
	/* cc = */ prefix_done,
	/* cd = */ prefix_done,
	/* ce = */ prefix_done,
	/* cf = */ prefix_done,

	/* d0 = */ prefix_done,
	/* d1 = */ prefix_done,
	/* d2 = */ prefix_done,
	/* d3 = */ prefix_done,
	/* d4 = */ prefix_done,
	/* d5 = */ prefix_done,
	/* d6 = */ prefix_done,
	/* d7 = */ prefix_done,
	/* d8 = */ prefix_done,
	/* d9 = */ prefix_done,
	/* da = */ prefix_done,
	/* db = */ prefix_done,
	/* dc = */ prefix_done,
	/* dd = */ prefix_done,
	/* de = */ prefix_done,
	/* df = */ prefix_done,

	/* e0 = */ prefix_done,
	/* e1 = */ prefix_done,
	/* e2 = */ prefix_done,
	/* e3 = */ prefix_done,
	/* e4 = */ prefix_done,
	/* e5 = */ prefix_done,
	/* e6 = */ prefix_done,
	/* e7 = */ prefix_done,
	/* e8 = */ prefix_done,
	/* e9 = */ prefix_done,
	/* ea = */ prefix_done,
	/* eb = */ prefix_done,
	/* ec = */ prefix_done,
	/* ed = */ prefix_done,
	/* ee = */ prefix_done,
	/* ef = */ prefix_done,

	/* f0 = */ prefix_lock,
	/* f1 = */ prefix_done,
	/* f2 = */ prefix_f2,
	/* f3 = */ prefix_f3,
	/* f4 = */ prefix_done,
	/* f5 = */ prefix_done,
	/* f6 = */ prefix_done,
	/* f7 = */ prefix_done,
	/* f8 = */ prefix_done,
	/* f9 = */ prefix_done,
	/* fa = */ prefix_done,
	/* fb = */ prefix_done,
	/* fc = */ prefix_done,
	/* fd = */ prefix_done,
	/* fe = */ prefix_done,
	/* ff = */ prefix_done
};

static inline int prefix_decode(struct pt_ild *ild, uint8_t length, uint8_t rex)
{
	uint8_t byte;

	if (!ild)
		return -pte_internal;

	if (ild->max_bytes <= length)
		return -pte_bad_insn;

	byte = get_byte(ild, length);

	return prefix_table[byte](ild, length, rex);
}

static inline int prefix_next(struct pt_ild *ild, uint8_t length, uint8_t rex)
{
	return prefix_decode(ild, length + 1, rex);
}

static int prefix_osz(struct pt_ild *ild, uint8_t length, uint8_t rex)
{
	(void) rex;

	if (!ild)
		return -pte_internal;

	ild->u.s.osz = 1;

	return prefix_next(ild, length, 0);
}

static int prefix_asz(struct pt_ild *ild, uint8_t length, uint8_t rex)
{
	(void) rex;

	if (!ild)
		return -pte_internal;

	ild->u.s.asz = 1;

	return prefix_next(ild, length, 0);
}

static int prefix_lock(struct pt_ild *ild, uint8_t length, uint8_t rex)
{
	(void) rex;

	if (!ild)
		return -pte_internal;

	ild->u.s.lock = 1;

	return prefix_next(ild, length, 0);
}

static int prefix_f2(struct pt_ild *ild, uint8_t length, uint8_t rex)
{
	(void) rex;

	if (!ild)
		return -pte_internal;

	ild->u.s.f2 = 1;
	ild->u.s.last_f2f3 = 2;

	return prefix_next(ild, length, 0);
}

static int prefix_f3(struct pt_ild *ild, uint8_t length, uint8_t rex)
{
	(void) rex;

	if (!ild)
		return -pte_internal;

	ild->u.s.f3 = 1;
	ild->u.s.last_f2f3 = 3;

	return prefix_next(ild, length, 0);
}

static int prefix_ignore(struct pt_ild *ild, uint8_t length, uint8_t rex)
{
	(void) rex;

	return prefix_next(ild, length, 0);
}

static int prefix_done(struct pt_ild *ild, uint8_t length, uint8_t rex)
{
	if (!ild)
		return -pte_internal;

	if (rex & 0x04)
		ild->u.s.rex_r = 1;
	if (rex & 0x08)
		ild->u.s.rex_w = 1;

	return opcode_dec(ild, length);
}

static int prefix_rex(struct pt_ild *ild, uint8_t length, uint8_t rex)
{
	(void) rex;

	if (!ild)
		return -pte_internal;

	if (mode_64b(ild))
		return prefix_next(ild, length, get_byte(ild, length));
	else
		return opcode_dec(ild, length);
}

static inline int prefix_vex_done(struct pt_ild *ild, uint8_t length)
{
	if (!ild)
		return -pte_internal;

	ild->nominal_opcode = get_byte(ild, length);

	return modrm_dec(ild, length + 1);
}

static int prefix_vex_c5(struct pt_ild *ild, uint8_t length, uint8_t rex)
{
	uint8_t max_bytes;
	uint8_t p1;

	(void) rex;

	if (!ild)
		return -pte_internal;

	max_bytes = ild->max_bytes;

	/* Read the next byte to validate that this is indeed VEX. */
	if (max_bytes <= (length + 1))
		return -pte_bad_insn;

	p1 = get_byte(ild, length + 1);

	/* If p1[7:6] is not 11b in non-64-bit mode, this is LDS, not VEX. */
	if (!mode_64b(ild) && !bits_match(p1, 0xc0, 0xc0))
		return opcode_dec(ild, length);

	/* We need at least 3 bytes
	 * - 2 for the VEX prefix and payload and
	 * - 1 for the opcode.
	 */
	if (max_bytes < (length + 3))
		return -pte_bad_insn;

	ild->u.s.vex = 1;
	if (p1 & 0x80)
		ild->u.s.rex_r = 1;

	ild->map = PTI_MAP_1;

	/* Eat the VEX. */
	length += 2;
	return prefix_vex_done(ild, length);
}

static int prefix_vex_c4(struct pt_ild *ild, uint8_t length, uint8_t rex)
{
	uint8_t max_bytes;
	uint8_t p1, p2, map;

	(void) rex;

	if (!ild)
		return -pte_internal;

	max_bytes = ild->max_bytes;

	/* Read the next byte to validate that this is indeed VEX. */
	if (max_bytes <= (length + 1))
		return -pte_bad_insn;

	p1 = get_byte(ild, length + 1);

	/* If p1[7:6] is not 11b in non-64-bit mode, this is LES, not VEX. */
	if (!mode_64b(ild) && !bits_match(p1, 0xc0, 0xc0))
		return opcode_dec(ild, length);

	/* We need at least 4 bytes
	 * - 3 for the VEX prefix and payload and
	 * - 1 for the opcode.
	 */
	if (max_bytes < (length + 4))
		return -pte_bad_insn;

	p2 = get_byte(ild, length + 2);

	ild->u.s.vex = 1;
	if (p1 & 0x80)
		ild->u.s.rex_r = 1;
	if (p2 & 0x80)
		ild->u.s.rex_w = 1;

	map = p1 & 0x1f;
	if (PTI_MAP_INVALID <= map)
		return -pte_bad_insn;

	ild->map = map;
	if (map == PTI_MAP_3)
		ild->imm1_bytes = 1;

	/* Eat the VEX. */
	length += 3;
	return prefix_vex_done(ild, length);
}

static int prefix_evex(struct pt_ild *ild, uint8_t length, uint8_t rex)
{
	uint8_t max_bytes;
	uint8_t p1, p2, map;

	(void) rex;

	if (!ild)
		return -pte_internal;

	max_bytes = ild->max_bytes;

	/* Read the next byte to validate that this is indeed EVEX. */
	if (max_bytes <= (length + 1))
		return -pte_bad_insn;

	p1 = get_byte(ild, length + 1);

	/* If p1[7:6] is not 11b in non-64-bit mode, this is BOUND, not EVEX. */
	if (!mode_64b(ild) && !bits_match(p1, 0xc0, 0xc0))
		return opcode_dec(ild, length);

	/* We need at least 5 bytes
	 * - 4 for the EVEX prefix and payload and
	 * - 1 for the opcode.
	 */
	if (max_bytes < (length + 5))
		return -pte_bad_insn;

	p2 = get_byte(ild, length + 2);

	ild->u.s.vex = 1;
	if (p1 & 0x80)
		ild->u.s.rex_r = 1;
	if (p2 & 0x80)
		ild->u.s.rex_w = 1;

	map = p1 & 0x03;
	ild->map = map;

	if (map == PTI_MAP_3)
		ild->imm1_bytes = 1;

	/* Eat the EVEX. */
	length += 4;
	return prefix_vex_done(ild, length);
}

static int decode(struct pt_ild *ild)
{
	return prefix_decode(ild, 0, 0);
}

static int set_branch_target(struct pt_insn_ext *iext, const struct pt_ild *ild)
{
	if (!iext || !ild)
		return -pte_internal;

	iext->variant.branch.is_direct = 1;

	if (ild->disp_bytes == 1) {
		const int8_t *b = (const int8_t *)
			get_byte_ptr(ild, ild->disp_pos);

		iext->variant.branch.displacement = *b;
	} else if (ild->disp_bytes == 2) {
		const int16_t *w = (const int16_t *)
			get_byte_ptr(ild, ild->disp_pos);

		iext->variant.branch.displacement = *w;
	} else if (ild->disp_bytes == 4) {
		const int32_t *d = (const int32_t *)
			get_byte_ptr(ild, ild->disp_pos);

		iext->variant.branch.displacement = *d;
	} else
		return -pte_bad_insn;

	return 0;
}

static int pt_instruction_length_decode(struct pt_ild *ild)
{
	if (!ild)
		return -pte_internal;

	ild->u.i = 0;
	ild->imm1_bytes = 0;
	ild->imm2_bytes = 0;
	ild->disp_bytes = 0;
	ild->modrm_byte = 0;
	ild->map = PTI_MAP_INVALID;

	if (!ild->mode)
		return -pte_bad_insn;

	return decode(ild);
}

static int pt_instruction_decode(struct pt_insn *insn, struct pt_insn_ext *iext,
				 const struct pt_ild *ild)
{
	uint8_t opcode, map;

	if (!iext || !ild)
		return -pte_internal;

	iext->iclass = PTI_INST_INVALID;
	memset(&iext->variant, 0, sizeof(iext->variant));

	insn->iclass = ptic_other;

	opcode = ild->nominal_opcode;
	map = ild->map;

	if (map > PTI_MAP_1)
		return 0;	/* uninteresting */
	if (ild->u.s.vex)
		return 0;	/* uninteresting */

	/* PTI_INST_JCC,   70...7F, 0F (0x80...0x8F) */
	if (opcode >= 0x70 && opcode <= 0x7F) {
		if (map == PTI_MAP_0) {
			insn->iclass = ptic_cond_jump;
			iext->iclass = PTI_INST_JCC;

			return set_branch_target(iext, ild);
		}
		return 0;
	}
	if (opcode >= 0x80 && opcode <= 0x8F) {
		if (map == PTI_MAP_1) {
			insn->iclass = ptic_cond_jump;
			iext->iclass = PTI_INST_JCC;

			return set_branch_target(iext, ild);
		}
		return 0;
	}

	switch (ild->nominal_opcode) {
	case 0x9A:
		if (map == PTI_MAP_0) {
			insn->iclass = ptic_far_call;
			iext->iclass = PTI_INST_CALL_9A;
		}
		return 0;

	case 0xFF:
		if (map == PTI_MAP_0) {
			uint8_t reg = pti_get_modrm_reg(ild);

			if (reg == 2) {
				insn->iclass = ptic_call;
				iext->iclass = PTI_INST_CALL_FFr2;
			} else if (reg == 3) {
				insn->iclass = ptic_far_call;
				iext->iclass = PTI_INST_CALL_FFr3;
			} else if (reg == 4) {
				insn->iclass = ptic_jump;
				iext->iclass = PTI_INST_JMP_FFr4;
			} else if (reg == 5) {
				insn->iclass = ptic_far_jump;
				iext->iclass = PTI_INST_JMP_FFr5;
			}
		}
		return 0;

	case 0xE8:
		if (map == PTI_MAP_0) {
			insn->iclass = ptic_call;
			iext->iclass = PTI_INST_CALL_E8;

			return set_branch_target(iext, ild);
		}
		return 0;

	case 0xCD:
		if (map == PTI_MAP_0) {
			insn->iclass = ptic_far_call;
			iext->iclass = PTI_INST_INT;
		}

		return 0;

	case 0xCC:
		if (map == PTI_MAP_0) {
			insn->iclass = ptic_far_call;
			iext->iclass = PTI_INST_INT3;
		}

		return 0;

	case 0xCE:
		if (map == PTI_MAP_0) {
			insn->iclass = ptic_far_call;
			iext->iclass = PTI_INST_INTO;
		}

		return 0;

	case 0xF1:
		if (map == PTI_MAP_0) {
			insn->iclass = ptic_far_call;
			iext->iclass = PTI_INST_INT1;
		}

		return 0;

	case 0xCF:
		if (map == PTI_MAP_0) {
			insn->iclass = ptic_far_return;
			iext->iclass = PTI_INST_IRET;
		}
		return 0;

	case 0xE9:
		if (map == PTI_MAP_0) {
			insn->iclass = ptic_jump;
			iext->iclass = PTI_INST_JMP_E9;

			return set_branch_target(iext, ild);
		}
		return 0;

	case 0xEA:
		if (map == PTI_MAP_0) {
			/* Far jumps are treated as indirect jumps. */
			insn->iclass = ptic_far_jump;
			iext->iclass = PTI_INST_JMP_EA;
		}
		return 0;

	case 0xEB:
		if (map == PTI_MAP_0) {
			insn->iclass = ptic_jump;
			iext->iclass = PTI_INST_JMP_EB;

			return set_branch_target(iext, ild);
		}
		return 0;

	case 0xE3:
		if (map == PTI_MAP_0) {
			insn->iclass = ptic_cond_jump;
			iext->iclass = PTI_INST_JrCXZ;

			return set_branch_target(iext, ild);
		}
		return 0;

	case 0xE0:
		if (map == PTI_MAP_0) {
			insn->iclass = ptic_cond_jump;
			iext->iclass = PTI_INST_LOOPNE;

			return set_branch_target(iext, ild);
		}
		return 0;

	case 0xE1:
		if (map == PTI_MAP_0) {
			insn->iclass = ptic_cond_jump;
			iext->iclass = PTI_INST_LOOPE;

			return set_branch_target(iext, ild);
		}
		return 0;

	case 0xE2:
		if (map == PTI_MAP_0) {
			insn->iclass = ptic_cond_jump;
			iext->iclass = PTI_INST_LOOP;

			return set_branch_target(iext, ild);
		}
		return 0;

	case 0x22:
		if (map == PTI_MAP_1)
			if (pti_get_modrm_reg(ild) == 3)
				if (!ild->u.s.rex_r)
					iext->iclass = PTI_INST_MOV_CR3;

		return 0;

	case 0xC3:
		if (map == PTI_MAP_0) {
			insn->iclass = ptic_return;
			iext->iclass = PTI_INST_RET_C3;
		}
		return 0;

	case 0xC2:
		if (map == PTI_MAP_0) {
			insn->iclass = ptic_return;
			iext->iclass = PTI_INST_RET_C2;
		}
		return 0;

	case 0xCB:
		if (map == PTI_MAP_0) {
			insn->iclass = ptic_far_return;
			iext->iclass = PTI_INST_RET_CB;
		}
		return 0;

	case 0xCA:
		if (map == PTI_MAP_0) {
			insn->iclass = ptic_far_return;
			iext->iclass = PTI_INST_RET_CA;
		}
		return 0;

	case 0x05:
		if (map == PTI_MAP_1) {
			insn->iclass = ptic_far_call;
			iext->iclass = PTI_INST_SYSCALL;
		}
		return 0;

	case 0x34:
		if (map == PTI_MAP_1) {
			insn->iclass = ptic_far_call;
			iext->iclass = PTI_INST_SYSENTER;
		}
		return 0;

	case 0x35:
		if (map == PTI_MAP_1) {
			insn->iclass = ptic_far_return;
			iext->iclass = PTI_INST_SYSEXIT;
		}
		return 0;

	case 0x07:
		if (map == PTI_MAP_1) {
			insn->iclass = ptic_far_return;
			iext->iclass = PTI_INST_SYSRET;
		}
		return 0;

	case 0x01:
		if (map == PTI_MAP_1) {
			switch (ild->modrm_byte) {
			case 0xc1:
				insn->iclass = ptic_far_call;
				iext->iclass = PTI_INST_VMCALL;
				break;

			case 0xc2:
				insn->iclass = ptic_far_return;
				iext->iclass = PTI_INST_VMLAUNCH;
				break;

			case 0xc3:
				insn->iclass = ptic_far_return;
				iext->iclass = PTI_INST_VMRESUME;
				break;

			default:
				break;
			}
		}
		return 0;

	case 0xc7:
		if (map == PTI_MAP_1 &&
		    pti_get_modrm_mod(ild) != 3 &&
		    pti_get_modrm_reg(ild) == 6)
			iext->iclass = PTI_INST_VMPTRLD;

		return 0;

	case 0xae:
		if (map == PTI_MAP_1 && ild->u.s.f3 && !ild->u.s.osz &&
		    pti_get_modrm_reg(ild) == 4) {
			insn->iclass = ptic_ptwrite;
			iext->iclass = PTI_INST_PTWRITE;
		}
		return 0;

	default:
		return 0;
	}
}

int pt_ild_decode(struct pt_insn *insn, struct pt_insn_ext *iext)
{
	struct pt_ild ild;
	int size;

	if (!insn || !iext)
		return -pte_internal;

	ild.mode = insn->mode;
	ild.itext = insn->raw;
	ild.max_bytes = insn->size;

	size = pt_instruction_length_decode(&ild);
	if (size < 0)
		return size;

	insn->size = (uint8_t) size;

	return pt_instruction_decode(insn, iext, &ild);
}
