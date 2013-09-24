/*-
 * Copyright (c) 2007 John Birrell (jb@freebsd.org)
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD$
 */

#include <stdlib.h>
#include "_libdwarf.h"

static int64_t
dwarf_decode_sleb128(uint8_t **dp)
{
	int64_t ret = 0;
	uint8_t b;
	int shift = 0;

	uint8_t *src = *dp;

	do {
		b = *src++;

		ret |= ((b & 0x7f) << shift);

		shift += 7;
	} while ((b & 0x80) != 0);

	if (shift < 64 && (b & 0x40) != 0)
		ret |= (-1 << shift);

	*dp = src;

	return ret;
}

static uint64_t
dwarf_decode_uleb128(uint8_t **dp)
{
	uint64_t ret = 0;
	uint8_t b;
	int shift = 0;

	uint8_t *src = *dp;

	do {
		b = *src++;

		ret |= ((b & 0x7f) << shift);

		shift += 7;
	} while ((b & 0x80) != 0);

	*dp = src;

	return ret;
}

/*
 * Given an array of bytes of length 'len' representing a
 * DWARF expression, compute the number of operations based
 * on there being one byte describing the operation and
 * zero or more bytes of operands as defined in the standard
 * for each operation type.
 */
int
dwarf_op_num(uint8_t pointer_size, uint8_t *p, int len)
{
	int count = 0;
	int64_t sval;
	uint64_t uval;
	uint8_t *last = p + len;

	/*
	 * Process each byte. If an error occurs, then the
	 * count will be set to -1.
	 */
	while (p < last && count >= 0) {
		count++;

		switch (*p++) {
		/* Operations with no operands. */
		case DW_OP_deref:
		case DW_OP_reg0:
		case DW_OP_reg1:
		case DW_OP_reg2:
		case DW_OP_reg3:
		case DW_OP_reg4:
		case DW_OP_reg5:
		case DW_OP_reg6:
		case DW_OP_reg7:
		case DW_OP_reg8:
		case DW_OP_reg9:
		case DW_OP_reg10:
		case DW_OP_reg11:
		case DW_OP_reg12:
		case DW_OP_reg13:
		case DW_OP_reg14:
		case DW_OP_reg15:
		case DW_OP_reg16:
		case DW_OP_reg17:
		case DW_OP_reg18:
		case DW_OP_reg19:
		case DW_OP_reg20:
		case DW_OP_reg21:
		case DW_OP_reg22:
		case DW_OP_reg23:
		case DW_OP_reg24:
		case DW_OP_reg25:
		case DW_OP_reg26:
		case DW_OP_reg27:
		case DW_OP_reg28:
		case DW_OP_reg29:
		case DW_OP_reg30:
		case DW_OP_reg31:

		case DW_OP_lit0:
		case DW_OP_lit1:
		case DW_OP_lit2:
		case DW_OP_lit3:
		case DW_OP_lit4:
		case DW_OP_lit5:
		case DW_OP_lit6:
		case DW_OP_lit7:
		case DW_OP_lit8:
		case DW_OP_lit9:
		case DW_OP_lit10:
		case DW_OP_lit11:
		case DW_OP_lit12:
		case DW_OP_lit13:
		case DW_OP_lit14:
		case DW_OP_lit15:
		case DW_OP_lit16:
		case DW_OP_lit17:
		case DW_OP_lit18:
		case DW_OP_lit19:
		case DW_OP_lit20:
		case DW_OP_lit21:
		case DW_OP_lit22:
		case DW_OP_lit23:
		case DW_OP_lit24:
		case DW_OP_lit25:
		case DW_OP_lit26:
		case DW_OP_lit27:
		case DW_OP_lit28:
		case DW_OP_lit29:
		case DW_OP_lit30:
		case DW_OP_lit31:

		case DW_OP_dup:
		case DW_OP_drop:

		case DW_OP_over:

		case DW_OP_swap:
		case DW_OP_rot:
		case DW_OP_xderef:

		case DW_OP_abs:
		case DW_OP_and:
		case DW_OP_div:
		case DW_OP_minus:
		case DW_OP_mod:
		case DW_OP_mul:
		case DW_OP_neg:
		case DW_OP_not:
		case DW_OP_or:
		case DW_OP_plus:

		case DW_OP_shl:
		case DW_OP_shr:
		case DW_OP_shra:
		case DW_OP_xor:

		case DW_OP_eq:
		case DW_OP_ge:
		case DW_OP_gt:
		case DW_OP_le:
		case DW_OP_lt:
		case DW_OP_ne:

		case DW_OP_nop:
			break;

		/* Operations with 1-byte operands. */
		case DW_OP_const1u:
		case DW_OP_const1s:
		case DW_OP_pick:
		case DW_OP_deref_size:
		case DW_OP_xderef_size:
			p++;
			break;

		/* Operations with 2-byte operands. */
		case DW_OP_const2u:
		case DW_OP_const2s:
		case DW_OP_bra:
		case DW_OP_skip:
			p += 2;
			break;

		/* Operations with 4-byte operands. */
		case DW_OP_const4u:
		case DW_OP_const4s:
			p += 4;
			break;

		/* Operations with 8-byte operands. */
		case DW_OP_const8u:
		case DW_OP_const8s:
			p += 8;
			break;

		/* Operations with an unsigned LEB128 operand. */
		case DW_OP_constu:
		case DW_OP_plus_uconst:
		case DW_OP_regx:
		case DW_OP_piece:
			uval = dwarf_decode_uleb128(&p);
			break;

		/* Operations with a signed LEB128 operand. */
		case DW_OP_consts:
		case DW_OP_breg0:
		case DW_OP_breg1:
		case DW_OP_breg2:
		case DW_OP_breg3:
		case DW_OP_breg4:
		case DW_OP_breg5:
		case DW_OP_breg6:
		case DW_OP_breg7:
		case DW_OP_breg8:
		case DW_OP_breg9:
		case DW_OP_breg10:
		case DW_OP_breg11:
		case DW_OP_breg12:
		case DW_OP_breg13:
		case DW_OP_breg14:
		case DW_OP_breg15:
		case DW_OP_breg16:
		case DW_OP_breg17:
		case DW_OP_breg18:
		case DW_OP_breg19:
		case DW_OP_breg20:
		case DW_OP_breg21:
		case DW_OP_breg22:
		case DW_OP_breg23:
		case DW_OP_breg24:
		case DW_OP_breg25:
		case DW_OP_breg26:
		case DW_OP_breg27:
		case DW_OP_breg28:
		case DW_OP_breg29:
		case DW_OP_breg30:
		case DW_OP_breg31:
		case DW_OP_fbreg:
			sval = dwarf_decode_sleb128(&p);
			break;

		/*
		 * Operations with an unsigned LEB128 operand
		 * followed by a signed LEB128 operand.
		 */
		case DW_OP_bregx:
			uval = dwarf_decode_uleb128(&p);
			sval = dwarf_decode_sleb128(&p);
			break;

		/* Target address size operand. */
		case DW_OP_addr:
			p += pointer_size;
			break;

		/* All other operations cause an error. */
		default:
			count = -1;
			break;
		}
	}

	return count;
}

static int
dwarf_loc_fill(Dwarf_Locdesc *lbuf, uint8_t pointer_size, uint8_t *p, int len)
{
	int count = 0;
	int ret = DWARF_E_NONE;
	uint64_t operand1;
	uint64_t operand2;
	uint8_t *last = p + len;

	/*
	 * Process each byte. If an error occurs, then the
	 * count will be set to -1.
	 */
	while (p < last && ret == DWARF_E_NONE) {
		operand1 = 0;
		operand2 = 0;

		lbuf->ld_s[count].lr_atom = *p;

		switch (*p++) {
		/* Operations with no operands. */
		case DW_OP_deref:
		case DW_OP_reg0:
		case DW_OP_reg1:
		case DW_OP_reg2:
		case DW_OP_reg3:
		case DW_OP_reg4:
		case DW_OP_reg5:
		case DW_OP_reg6:
		case DW_OP_reg7:
		case DW_OP_reg8:
		case DW_OP_reg9:
		case DW_OP_reg10:
		case DW_OP_reg11:
		case DW_OP_reg12:
		case DW_OP_reg13:
		case DW_OP_reg14:
		case DW_OP_reg15:
		case DW_OP_reg16:
		case DW_OP_reg17:
		case DW_OP_reg18:
		case DW_OP_reg19:
		case DW_OP_reg20:
		case DW_OP_reg21:
		case DW_OP_reg22:
		case DW_OP_reg23:
		case DW_OP_reg24:
		case DW_OP_reg25:
		case DW_OP_reg26:
		case DW_OP_reg27:
		case DW_OP_reg28:
		case DW_OP_reg29:
		case DW_OP_reg30:
		case DW_OP_reg31:

		case DW_OP_lit0:
		case DW_OP_lit1:
		case DW_OP_lit2:
		case DW_OP_lit3:
		case DW_OP_lit4:
		case DW_OP_lit5:
		case DW_OP_lit6:
		case DW_OP_lit7:
		case DW_OP_lit8:
		case DW_OP_lit9:
		case DW_OP_lit10:
		case DW_OP_lit11:
		case DW_OP_lit12:
		case DW_OP_lit13:
		case DW_OP_lit14:
		case DW_OP_lit15:
		case DW_OP_lit16:
		case DW_OP_lit17:
		case DW_OP_lit18:
		case DW_OP_lit19:
		case DW_OP_lit20:
		case DW_OP_lit21:
		case DW_OP_lit22:
		case DW_OP_lit23:
		case DW_OP_lit24:
		case DW_OP_lit25:
		case DW_OP_lit26:
		case DW_OP_lit27:
		case DW_OP_lit28:
		case DW_OP_lit29:
		case DW_OP_lit30:
		case DW_OP_lit31:

		case DW_OP_dup:
		case DW_OP_drop:

		case DW_OP_over:

		case DW_OP_swap:
		case DW_OP_rot:
		case DW_OP_xderef:

		case DW_OP_abs:
		case DW_OP_and:
		case DW_OP_div:
		case DW_OP_minus:
		case DW_OP_mod:
		case DW_OP_mul:
		case DW_OP_neg:
		case DW_OP_not:
		case DW_OP_or:
		case DW_OP_plus:

		case DW_OP_shl:
		case DW_OP_shr:
		case DW_OP_shra:
		case DW_OP_xor:

		case DW_OP_eq:
		case DW_OP_ge:
		case DW_OP_gt:
		case DW_OP_le:
		case DW_OP_lt:
		case DW_OP_ne:

		case DW_OP_nop:
			break;

		/* Operations with 1-byte operands. */
		case DW_OP_const1u:
		case DW_OP_const1s:
		case DW_OP_pick:
		case DW_OP_deref_size:
		case DW_OP_xderef_size:
			operand1 = *p++;
			break;

		/* Operations with 2-byte operands. */
		case DW_OP_const2u:
		case DW_OP_const2s:
		case DW_OP_bra:
		case DW_OP_skip:
			p += 2;
			break;

		/* Operations with 4-byte operands. */
		case DW_OP_const4u:
		case DW_OP_const4s:
			p += 4;
			break;

		/* Operations with 8-byte operands. */
		case DW_OP_const8u:
		case DW_OP_const8s:
			p += 8;
			break;

		/* Operations with an unsigned LEB128 operand. */
		case DW_OP_constu:
		case DW_OP_plus_uconst:
		case DW_OP_regx:
		case DW_OP_piece:
			operand1 = dwarf_decode_uleb128(&p);
			break;

		/* Operations with a signed LEB128 operand. */
		case DW_OP_consts:
		case DW_OP_breg0:
		case DW_OP_breg1:
		case DW_OP_breg2:
		case DW_OP_breg3:
		case DW_OP_breg4:
		case DW_OP_breg5:
		case DW_OP_breg6:
		case DW_OP_breg7:
		case DW_OP_breg8:
		case DW_OP_breg9:
		case DW_OP_breg10:
		case DW_OP_breg11:
		case DW_OP_breg12:
		case DW_OP_breg13:
		case DW_OP_breg14:
		case DW_OP_breg15:
		case DW_OP_breg16:
		case DW_OP_breg17:
		case DW_OP_breg18:
		case DW_OP_breg19:
		case DW_OP_breg20:
		case DW_OP_breg21:
		case DW_OP_breg22:
		case DW_OP_breg23:
		case DW_OP_breg24:
		case DW_OP_breg25:
		case DW_OP_breg26:
		case DW_OP_breg27:
		case DW_OP_breg28:
		case DW_OP_breg29:
		case DW_OP_breg30:
		case DW_OP_breg31:
		case DW_OP_fbreg:
			operand1 = dwarf_decode_sleb128(&p);
			break;

		/*
		 * Operations with an unsigned LEB128 operand
		 * followed by a signed LEB128 operand.
		 */
		case DW_OP_bregx:
			operand1 = dwarf_decode_uleb128(&p);
			operand2 = dwarf_decode_sleb128(&p);
			break;

		/* Target address size operand. */
		case DW_OP_addr:
			p += pointer_size;
			break;

		/* All other operations cause an error. */
		default:
			break;
		}

		lbuf->ld_s[count].lr_number = operand1;
		lbuf->ld_s[count].lr_number2 = operand2;

		count++;
	}

	return ret;
}

int
dwarf_locdesc(Dwarf_Die die, uint64_t attr, Dwarf_Locdesc **llbuf, Dwarf_Signed *lenp, Dwarf_Error *err)
{
	Dwarf_AttrValue av;
	Dwarf_Locdesc *lbuf;
	int num;
	int ret = DWARF_E_NONE;

	if (err == NULL)
		return DWARF_E_ERROR;

	if (die == NULL || llbuf == NULL || lenp == NULL) {
		DWARF_SET_ERROR(err, DWARF_E_ARGUMENT);
		return DWARF_E_ARGUMENT;
	}

	if ((av = dwarf_attrval_find(die, attr)) == NULL) {
		DWARF_SET_ERROR(err, DWARF_E_NO_ENTRY);
		ret = DWARF_E_NO_ENTRY;
	} else if ((lbuf = calloc(sizeof(Dwarf_Locdesc), 1)) == NULL) {
		DWARF_SET_ERROR(err, DWARF_E_MEMORY);
		ret = DWARF_E_MEMORY;
	} else {
		*lenp = 0;
		switch (av->av_form) {
		case DW_FORM_block:
		case DW_FORM_block1:
		case DW_FORM_block2:
		case DW_FORM_block4:
			/* Compute the number of locations: */
			if ((num = dwarf_op_num(die->die_cu->cu_pointer_size,
			    av->u[1].u8p, av->u[0].u64)) < 0) {
				DWARF_SET_ERROR(err, DWARF_E_INVALID_EXPR);
				ret = DWARF_E_INVALID_EXPR;

			/* Allocate an array of location structures. */
			} else if ((lbuf->ld_s =
			    calloc(sizeof(Dwarf_Loc), num)) == NULL) {
				DWARF_SET_ERROR(err, DWARF_E_MEMORY);
				ret = DWARF_E_MEMORY;

			/* Fill the array of location structures. */
			} else if ((ret = dwarf_loc_fill(lbuf,
			    die->die_cu->cu_pointer_size,
			    av->u[1].u8p, av->u[0].u64)) != DWARF_E_NONE) {
				free(lbuf->ld_s);
			} else
				/* Only one descriptor is returned. */
				*lenp = 1;
			break;
		default:
			printf("%s(%d): form %s not handled\n",__func__,
			    __LINE__,get_form_desc(av->av_form));
			DWARF_SET_ERROR(err, DWARF_E_NOT_IMPLEMENTED);
			ret = DWARF_E_ERROR;
		}

		if (ret == DWARF_E_NONE) {
			*llbuf = lbuf;
		} else
			free(lbuf);
	}

	return ret;
}

int
dwarf_locdesc_free(Dwarf_Locdesc *lbuf, Dwarf_Error *err)
{
	if (err == NULL)
		return DWARF_E_ERROR;

	if (lbuf == NULL) {
		DWARF_SET_ERROR(err, DWARF_E_ARGUMENT);
		return DWARF_E_ARGUMENT;
	}

	if (lbuf->ld_s != NULL)
		free(lbuf->ld_s);

	free(lbuf);

	return DWARF_E_NONE;
}
