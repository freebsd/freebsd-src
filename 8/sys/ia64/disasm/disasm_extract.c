/*-
 * Copyright (c) 2000-2006 Marcel Moolenaar
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>

#include <machine/stdarg.h>

#include <ia64/disasm/disasm_int.h>
#include <ia64/disasm/disasm.h>

#define FRAG(o,l)	((int)((o << 8) | (l & 0xff)))
#define FRAG_OFS(f)	(f >> 8)
#define FRAG_LEN(f)	(f & 0xff)

/*
 * Support functions.
 */
static void
asm_cmpltr_add(struct asm_inst *i, enum asm_cmpltr_class c,
    enum asm_cmpltr_type t)
{

	i->i_cmpltr[i->i_ncmpltrs].c_class = c;
	i->i_cmpltr[i->i_ncmpltrs].c_type = t;
	i->i_ncmpltrs++;
	KASSERT(i->i_ncmpltrs < 6, ("foo"));
}

static void
asm_hint(struct asm_inst *i, enum asm_cmpltr_class c)
{

	switch (FIELD(i->i_bits, 28, 2)) { /* hint */
	case 0:
		asm_cmpltr_add(i, c, ASM_CT_NONE);
		break;
	case 1:
		asm_cmpltr_add(i, c, ASM_CT_NT1);
		break;
	case 2:
		asm_cmpltr_add(i, c, ASM_CT_NT2);
		break;
	case 3:
		asm_cmpltr_add(i, c, ASM_CT_NTA);
		break;
	}
}

static void
asm_sf(struct asm_inst *i)
{

	switch (FIELD(i->i_bits, 34, 2)) {
	case 0:
		asm_cmpltr_add(i, ASM_CC_SF, ASM_CT_S0);
		break;
	case 1:
		asm_cmpltr_add(i, ASM_CC_SF, ASM_CT_S1);
		break;
	case 2:
		asm_cmpltr_add(i, ASM_CC_SF, ASM_CT_S2);
		break;
	case 3:
		asm_cmpltr_add(i, ASM_CC_SF, ASM_CT_S3);
		break;
	}
}

static void
asm_brhint(struct asm_inst *i)
{
	uint64_t bits = i->i_bits;

	switch (FIELD(bits, 33, 2)) { /* bwh */
	case 0:
		asm_cmpltr_add(i, ASM_CC_BWH, ASM_CT_SPTK);
		break;
	case 1:
		asm_cmpltr_add(i, ASM_CC_BWH, ASM_CT_SPNT);
		break;
	case 2:
		asm_cmpltr_add(i, ASM_CC_BWH, ASM_CT_DPTK);
		break;
	case 3:
		asm_cmpltr_add(i, ASM_CC_BWH, ASM_CT_DPNT);
		break;
	}

	if (FIELD(bits, 12, 1)) /* ph */
		asm_cmpltr_add(i, ASM_CC_PH, ASM_CT_MANY);
	else
		asm_cmpltr_add(i, ASM_CC_PH, ASM_CT_FEW);

	if (FIELD(bits, 35, 1)) /* dh */
		asm_cmpltr_add(i, ASM_CC_DH, ASM_CT_CLR);
	else
		asm_cmpltr_add(i, ASM_CC_DH, ASM_CT_NONE);
}

static void
asm_brphint(struct asm_inst *i)
{
	uint64_t bits = i->i_bits;

	switch (FIELD(bits, 3, 2)) { /* ipwh, indwh */
	case 0:
		asm_cmpltr_add(i, ASM_CC_IPWH, ASM_CT_SPTK);
		break;
	case 1:
		asm_cmpltr_add(i, ASM_CC_IPWH, ASM_CT_LOOP);
		break;
	case 2:
		asm_cmpltr_add(i, ASM_CC_IPWH, ASM_CT_DPTK);
		break;
	case 3:
		asm_cmpltr_add(i, ASM_CC_IPWH, ASM_CT_EXIT);
		break;
	}

	if (FIELD(bits, 5, 1)) /* ph */
		asm_cmpltr_add(i, ASM_CC_PH, ASM_CT_MANY);
	else
		asm_cmpltr_add(i, ASM_CC_PH, ASM_CT_FEW);

	switch (FIELD(bits, 0, 3)) { /* pvec */
	case 0:
		asm_cmpltr_add(i, ASM_CC_PVEC, ASM_CT_DC_DC);
		break;
	case 1:
		asm_cmpltr_add(i, ASM_CC_PVEC, ASM_CT_DC_NT);
		break;
	case 2:
		asm_cmpltr_add(i, ASM_CC_PVEC, ASM_CT_TK_DC);
		break;
	case 3:
		asm_cmpltr_add(i, ASM_CC_PVEC, ASM_CT_TK_TK);
		break;
	case 4:
		asm_cmpltr_add(i, ASM_CC_PVEC, ASM_CT_TK_NT);
		break;
	case 5:
		asm_cmpltr_add(i, ASM_CC_PVEC, ASM_CT_NT_DC);
		break;
	case 6:
		asm_cmpltr_add(i, ASM_CC_PVEC, ASM_CT_NT_TK);
		break;
	case 7:
		asm_cmpltr_add(i, ASM_CC_PVEC, ASM_CT_NT_NT);
		break;
	}

	if (FIELD(bits, 35, 1)) /* ih */
		asm_cmpltr_add(i, ASM_CC_IH, ASM_CT_IMP);
	else
		asm_cmpltr_add(i, ASM_CC_IH, ASM_CT_NONE);
}

static enum asm_oper_type
asm_normalize(struct asm_inst *i, enum asm_op op)
{
	enum asm_oper_type ot = ASM_OPER_NONE;

	switch (op) {
	case ASM_OP_BR_CALL:
		asm_cmpltr_add(i, ASM_CC_BTYPE, ASM_CT_CALL);
		op = ASM_OP_BR;
		break;
	case ASM_OP_BR_CEXIT:
		asm_cmpltr_add(i, ASM_CC_BTYPE, ASM_CT_CEXIT);
		op = ASM_OP_BR;
		break;
	case ASM_OP_BR_CLOOP:
		asm_cmpltr_add(i, ASM_CC_BTYPE, ASM_CT_CLOOP);
		op = ASM_OP_BR;
		break;
	case ASM_OP_BR_COND:
		asm_cmpltr_add(i, ASM_CC_BTYPE, ASM_CT_COND);
		op = ASM_OP_BR;
		break;
	case ASM_OP_BR_CTOP:
		asm_cmpltr_add(i, ASM_CC_BTYPE, ASM_CT_CTOP);
		op = ASM_OP_BR;
		break;
	case ASM_OP_BR_IA:
		asm_cmpltr_add(i, ASM_CC_BTYPE, ASM_CT_IA);
		op = ASM_OP_BR;
		break;
	case ASM_OP_BR_RET:
		asm_cmpltr_add(i, ASM_CC_BTYPE, ASM_CT_RET);
		op = ASM_OP_BR;
		break;
	case ASM_OP_BR_WEXIT:
		asm_cmpltr_add(i, ASM_CC_BTYPE, ASM_CT_WEXIT);
		op = ASM_OP_BR;
		break;
	case ASM_OP_BR_WTOP:
		asm_cmpltr_add(i, ASM_CC_BTYPE, ASM_CT_WTOP);
		op = ASM_OP_BR;
		break;
	case ASM_OP_BREAK_B:
		asm_cmpltr_add(i, ASM_CC_UNIT, ASM_CT_B);
		op = ASM_OP_BREAK;
		break;
	case ASM_OP_BREAK_F:
		asm_cmpltr_add(i, ASM_CC_UNIT, ASM_CT_F);
		op = ASM_OP_BREAK;
		break;
	case ASM_OP_BREAK_I:
		asm_cmpltr_add(i, ASM_CC_UNIT, ASM_CT_I);
		op = ASM_OP_BREAK;
		break;
	case ASM_OP_BREAK_M:
		asm_cmpltr_add(i, ASM_CC_UNIT, ASM_CT_M);
		op = ASM_OP_BREAK;
		break;
	case ASM_OP_BREAK_X:
		asm_cmpltr_add(i, ASM_CC_UNIT, ASM_CT_X);
		op = ASM_OP_BREAK;
		break;
	case ASM_OP_BRL_COND:
		asm_cmpltr_add(i, ASM_CC_BTYPE, ASM_CT_COND);
		op = ASM_OP_BRL;
		break;
	case ASM_OP_BRL_CALL:
		asm_cmpltr_add(i, ASM_CC_BTYPE, ASM_CT_CALL);
		op = ASM_OP_BRL;
		break;
	case ASM_OP_BRP_:
		asm_cmpltr_add(i, ASM_CC_BTYPE, ASM_CT_NONE);
		op = ASM_OP_BRP;
		break;
	case ASM_OP_BRP_RET:
		asm_cmpltr_add(i, ASM_CC_BTYPE, ASM_CT_RET);
		op = ASM_OP_BRP;
		break;
	case ASM_OP_BSW_0:
		asm_cmpltr_add(i, ASM_CC_BSW, ASM_CT_0);
		op = ASM_OP_BSW;
		break;
	case ASM_OP_BSW_1:
		asm_cmpltr_add(i, ASM_CC_BSW, ASM_CT_1);
		op = ASM_OP_BSW;
		break;
	case ASM_OP_CHK_A_CLR:
		asm_cmpltr_add(i, ASM_CC_CHK, ASM_CT_A);
		asm_cmpltr_add(i, ASM_CC_ACLR, ASM_CT_CLR);
		op = ASM_OP_CHK;
		break;
	case ASM_OP_CHK_A_NC:
		asm_cmpltr_add(i, ASM_CC_CHK, ASM_CT_A);
		asm_cmpltr_add(i, ASM_CC_ACLR, ASM_CT_NC);
		op = ASM_OP_CHK;
		break;
	case ASM_OP_CHK_S:
		asm_cmpltr_add(i, ASM_CC_CHK, ASM_CT_S);
		op = ASM_OP_CHK;
		break;
	case ASM_OP_CHK_S_I:
		asm_cmpltr_add(i, ASM_CC_CHK, ASM_CT_S);
		asm_cmpltr_add(i, ASM_CC_UNIT, ASM_CT_I);
		op = ASM_OP_CHK;
		break;
	case ASM_OP_CHK_S_M:
		asm_cmpltr_add(i, ASM_CC_CHK, ASM_CT_S);
		asm_cmpltr_add(i, ASM_CC_UNIT, ASM_CT_M);
		op = ASM_OP_CHK;
		break;
	case ASM_OP_CLRRRB_:
		asm_cmpltr_add(i, ASM_CC_CLRRRB, ASM_CT_NONE);
		op = ASM_OP_CLRRRB;
		break;
	case ASM_OP_CLRRRB_PR:
		asm_cmpltr_add(i, ASM_CC_CLRRRB, ASM_CT_PR);
		op = ASM_OP_CLRRRB;
		break;
	case ASM_OP_CMP_EQ:
		asm_cmpltr_add(i, ASM_CC_CREL, ASM_CT_EQ);
		asm_cmpltr_add(i, ASM_CC_CTYPE, ASM_CT_NONE);
		op = ASM_OP_CMP;
		break;
	case ASM_OP_CMP_EQ_AND:
		asm_cmpltr_add(i, ASM_CC_CREL, ASM_CT_EQ);
		asm_cmpltr_add(i, ASM_CC_CTYPE, ASM_CT_AND);
		op = ASM_OP_CMP;
		break;
	case ASM_OP_CMP_EQ_OR:
		asm_cmpltr_add(i, ASM_CC_CREL, ASM_CT_EQ);
		asm_cmpltr_add(i, ASM_CC_CTYPE, ASM_CT_OR);
		op = ASM_OP_CMP;
		break;
	case ASM_OP_CMP_EQ_OR_ANDCM:
		asm_cmpltr_add(i, ASM_CC_CREL, ASM_CT_EQ);
		asm_cmpltr_add(i, ASM_CC_CTYPE, ASM_CT_OR_ANDCM);
		op = ASM_OP_CMP;
		break;
	case ASM_OP_CMP_EQ_UNC:
		asm_cmpltr_add(i, ASM_CC_CREL, ASM_CT_EQ);
		asm_cmpltr_add(i, ASM_CC_CTYPE, ASM_CT_UNC);
		op = ASM_OP_CMP;
		break;
	case ASM_OP_CMP_GE_AND:
		asm_cmpltr_add(i, ASM_CC_CREL, ASM_CT_GE);
		asm_cmpltr_add(i, ASM_CC_CTYPE, ASM_CT_AND);
		op = ASM_OP_CMP;
		break;
	case ASM_OP_CMP_GE_OR:
		asm_cmpltr_add(i, ASM_CC_CREL, ASM_CT_GE);
		asm_cmpltr_add(i, ASM_CC_CTYPE, ASM_CT_OR);
		op = ASM_OP_CMP;
		break;
	case ASM_OP_CMP_GE_OR_ANDCM:
		asm_cmpltr_add(i, ASM_CC_CREL, ASM_CT_GE);
		asm_cmpltr_add(i, ASM_CC_CTYPE, ASM_CT_OR_ANDCM);
		op = ASM_OP_CMP;
		break;
	case ASM_OP_CMP_GT_AND:
		asm_cmpltr_add(i, ASM_CC_CREL, ASM_CT_GT);
		asm_cmpltr_add(i, ASM_CC_CTYPE, ASM_CT_AND);
		op = ASM_OP_CMP;
		break;
	case ASM_OP_CMP_GT_OR:
		asm_cmpltr_add(i, ASM_CC_CREL, ASM_CT_GT);
		asm_cmpltr_add(i, ASM_CC_CTYPE, ASM_CT_OR);
		op = ASM_OP_CMP;
		break;
	case ASM_OP_CMP_GT_OR_ANDCM:
		asm_cmpltr_add(i, ASM_CC_CREL, ASM_CT_GT);
		asm_cmpltr_add(i, ASM_CC_CTYPE, ASM_CT_OR_ANDCM);
		op = ASM_OP_CMP;
		break;
	case ASM_OP_CMP_LE_AND:
		asm_cmpltr_add(i, ASM_CC_CREL, ASM_CT_LE);
		asm_cmpltr_add(i, ASM_CC_CTYPE, ASM_CT_AND);
		op = ASM_OP_CMP;
		break;
	case ASM_OP_CMP_LE_OR:
		asm_cmpltr_add(i, ASM_CC_CREL, ASM_CT_LE);
		asm_cmpltr_add(i, ASM_CC_CTYPE, ASM_CT_OR);
		op = ASM_OP_CMP;
		break;
	case ASM_OP_CMP_LE_OR_ANDCM:
		asm_cmpltr_add(i, ASM_CC_CREL, ASM_CT_LE);
		asm_cmpltr_add(i, ASM_CC_CTYPE, ASM_CT_OR_ANDCM);
		op = ASM_OP_CMP;
		break;
	case ASM_OP_CMP_LT:
		asm_cmpltr_add(i, ASM_CC_CREL, ASM_CT_LT);
		asm_cmpltr_add(i, ASM_CC_CTYPE, ASM_CT_NONE);
		op = ASM_OP_CMP;
		break;
	case ASM_OP_CMP_LT_AND:
		asm_cmpltr_add(i, ASM_CC_CREL, ASM_CT_LT);
		asm_cmpltr_add(i, ASM_CC_CTYPE, ASM_CT_AND);
		op = ASM_OP_CMP;
		break;
	case ASM_OP_CMP_LT_OR:
		asm_cmpltr_add(i, ASM_CC_CREL, ASM_CT_LT);
		asm_cmpltr_add(i, ASM_CC_CTYPE, ASM_CT_OR);
		op = ASM_OP_CMP;
		break;
	case ASM_OP_CMP_LT_OR_ANDCM:
		asm_cmpltr_add(i, ASM_CC_CREL, ASM_CT_LT);
		asm_cmpltr_add(i, ASM_CC_CTYPE, ASM_CT_OR_ANDCM);
		op = ASM_OP_CMP;
		break;
	case ASM_OP_CMP_LT_UNC:
		asm_cmpltr_add(i, ASM_CC_CREL, ASM_CT_LT);
		asm_cmpltr_add(i, ASM_CC_CTYPE, ASM_CT_UNC);
		op = ASM_OP_CMP;
		break;
	case ASM_OP_CMP_LTU:
		asm_cmpltr_add(i, ASM_CC_CREL, ASM_CT_LTU);
		asm_cmpltr_add(i, ASM_CC_CTYPE, ASM_CT_NONE);
		op = ASM_OP_CMP;
		break;
	case ASM_OP_CMP_LTU_UNC:
		asm_cmpltr_add(i, ASM_CC_CREL, ASM_CT_LTU);
		asm_cmpltr_add(i, ASM_CC_CTYPE, ASM_CT_UNC);
		op = ASM_OP_CMP;
		break;
	case ASM_OP_CMP_NE_AND:
		asm_cmpltr_add(i, ASM_CC_CREL, ASM_CT_NE);
		asm_cmpltr_add(i, ASM_CC_CTYPE, ASM_CT_AND);
		op = ASM_OP_CMP;
		break;
	case ASM_OP_CMP_NE_OR:
		asm_cmpltr_add(i, ASM_CC_CREL, ASM_CT_NE);
		asm_cmpltr_add(i, ASM_CC_CTYPE, ASM_CT_OR);
		op = ASM_OP_CMP;
		break;
	case ASM_OP_CMP_NE_OR_ANDCM:
		asm_cmpltr_add(i, ASM_CC_CREL, ASM_CT_NE);
		asm_cmpltr_add(i, ASM_CC_CTYPE, ASM_CT_OR_ANDCM);
		op = ASM_OP_CMP;
		break;
	case ASM_OP_CMP4_EQ:
		asm_cmpltr_add(i, ASM_CC_CREL, ASM_CT_EQ);
		asm_cmpltr_add(i, ASM_CC_CTYPE, ASM_CT_NONE);
		op = ASM_OP_CMP4;
		break;
	case ASM_OP_CMP4_EQ_AND:
		asm_cmpltr_add(i, ASM_CC_CREL, ASM_CT_EQ);
		asm_cmpltr_add(i, ASM_CC_CTYPE, ASM_CT_AND);
		op = ASM_OP_CMP4;
		break;
	case ASM_OP_CMP4_EQ_OR:
		asm_cmpltr_add(i, ASM_CC_CREL, ASM_CT_EQ);
		asm_cmpltr_add(i, ASM_CC_CTYPE, ASM_CT_OR);
		op = ASM_OP_CMP4;
		break;
	case ASM_OP_CMP4_EQ_OR_ANDCM:
		asm_cmpltr_add(i, ASM_CC_CREL, ASM_CT_EQ);
		asm_cmpltr_add(i, ASM_CC_CTYPE, ASM_CT_OR_ANDCM);
		op = ASM_OP_CMP4;
		break;
	case ASM_OP_CMP4_EQ_UNC:
		asm_cmpltr_add(i, ASM_CC_CREL, ASM_CT_EQ);
		asm_cmpltr_add(i, ASM_CC_CTYPE, ASM_CT_UNC);
		op = ASM_OP_CMP4;
		break;
	case ASM_OP_CMP4_GE_AND:
		asm_cmpltr_add(i, ASM_CC_CREL, ASM_CT_GE);
		asm_cmpltr_add(i, ASM_CC_CTYPE, ASM_CT_AND);
		op = ASM_OP_CMP4;
		break;
	case ASM_OP_CMP4_GE_OR:
		asm_cmpltr_add(i, ASM_CC_CREL, ASM_CT_GE);
		asm_cmpltr_add(i, ASM_CC_CTYPE, ASM_CT_OR);
		op = ASM_OP_CMP4;
		break;
	case ASM_OP_CMP4_GE_OR_ANDCM:
		asm_cmpltr_add(i, ASM_CC_CREL, ASM_CT_GE);
		asm_cmpltr_add(i, ASM_CC_CTYPE, ASM_CT_OR_ANDCM);
		op = ASM_OP_CMP4;
		break;
	case ASM_OP_CMP4_GT_AND:
		asm_cmpltr_add(i, ASM_CC_CREL, ASM_CT_GT);
		asm_cmpltr_add(i, ASM_CC_CTYPE, ASM_CT_AND);
		op = ASM_OP_CMP4;
		break;
	case ASM_OP_CMP4_GT_OR:
		asm_cmpltr_add(i, ASM_CC_CREL, ASM_CT_GT);
		asm_cmpltr_add(i, ASM_CC_CTYPE, ASM_CT_OR);
		op = ASM_OP_CMP4;
		break;
	case ASM_OP_CMP4_GT_OR_ANDCM:
		asm_cmpltr_add(i, ASM_CC_CREL, ASM_CT_GT);
		asm_cmpltr_add(i, ASM_CC_CTYPE, ASM_CT_OR_ANDCM);
		op = ASM_OP_CMP4;
		break;
	case ASM_OP_CMP4_LE_AND:
		asm_cmpltr_add(i, ASM_CC_CREL, ASM_CT_LE);
		asm_cmpltr_add(i, ASM_CC_CTYPE, ASM_CT_AND);
		op = ASM_OP_CMP4;
		break;
	case ASM_OP_CMP4_LE_OR:
		asm_cmpltr_add(i, ASM_CC_CREL, ASM_CT_LE);
		asm_cmpltr_add(i, ASM_CC_CTYPE, ASM_CT_OR);
		op = ASM_OP_CMP4;
		break;
	case ASM_OP_CMP4_LE_OR_ANDCM:
		asm_cmpltr_add(i, ASM_CC_CREL, ASM_CT_LE);
		asm_cmpltr_add(i, ASM_CC_CTYPE, ASM_CT_OR_ANDCM);
		op = ASM_OP_CMP4;
		break;
	case ASM_OP_CMP4_LT:
		asm_cmpltr_add(i, ASM_CC_CREL, ASM_CT_LT);
		asm_cmpltr_add(i, ASM_CC_CTYPE, ASM_CT_NONE);
		op = ASM_OP_CMP4;
		break;
	case ASM_OP_CMP4_LT_AND:
		asm_cmpltr_add(i, ASM_CC_CREL, ASM_CT_LT);
		asm_cmpltr_add(i, ASM_CC_CTYPE, ASM_CT_AND);
		op = ASM_OP_CMP4;
		break;
	case ASM_OP_CMP4_LT_OR:
		asm_cmpltr_add(i, ASM_CC_CREL, ASM_CT_LT);
		asm_cmpltr_add(i, ASM_CC_CTYPE, ASM_CT_OR);
		op = ASM_OP_CMP4;
		break;
	case ASM_OP_CMP4_LT_OR_ANDCM:
		asm_cmpltr_add(i, ASM_CC_CREL, ASM_CT_LT);
		asm_cmpltr_add(i, ASM_CC_CTYPE, ASM_CT_OR_ANDCM);
		op = ASM_OP_CMP4;
		break;
	case ASM_OP_CMP4_LT_UNC:
		asm_cmpltr_add(i, ASM_CC_CREL, ASM_CT_LT);
		asm_cmpltr_add(i, ASM_CC_CTYPE, ASM_CT_UNC);
		op = ASM_OP_CMP4;
		break;
	case ASM_OP_CMP4_LTU:
		asm_cmpltr_add(i, ASM_CC_CREL, ASM_CT_LTU);
		asm_cmpltr_add(i, ASM_CC_CTYPE, ASM_CT_NONE);
		op = ASM_OP_CMP4;
		break;
	case ASM_OP_CMP4_LTU_UNC:
		asm_cmpltr_add(i, ASM_CC_CREL, ASM_CT_LTU);
		asm_cmpltr_add(i, ASM_CC_CTYPE, ASM_CT_UNC);
		op = ASM_OP_CMP4;
		break;
	case ASM_OP_CMP4_NE_AND:
		asm_cmpltr_add(i, ASM_CC_CREL, ASM_CT_NE);
		asm_cmpltr_add(i, ASM_CC_CTYPE, ASM_CT_AND);
		op = ASM_OP_CMP4;
		break;
	case ASM_OP_CMP4_NE_OR:
		asm_cmpltr_add(i, ASM_CC_CREL, ASM_CT_NE);
		asm_cmpltr_add(i, ASM_CC_CTYPE, ASM_CT_OR);
		op = ASM_OP_CMP4;
		break;
	case ASM_OP_CMP4_NE_OR_ANDCM:
		asm_cmpltr_add(i, ASM_CC_CREL, ASM_CT_NE);
		asm_cmpltr_add(i, ASM_CC_CTYPE, ASM_CT_OR_ANDCM);
		op = ASM_OP_CMP4;
		break;
	case ASM_OP_CMP8XCHG16_ACQ:
		asm_cmpltr_add(i, ASM_CC_SEM, ASM_CT_ACQ);
		op = ASM_OP_CMP8XCHG16;
		break;
	case ASM_OP_CMP8XCHG16_REL:
		asm_cmpltr_add(i, ASM_CC_SEM, ASM_CT_REL);
		op = ASM_OP_CMP8XCHG16;
		break;
	case ASM_OP_CMPXCHG1_ACQ:
		asm_cmpltr_add(i, ASM_CC_SEM, ASM_CT_ACQ);
		op = ASM_OP_CMPXCHG1;
		break;
	case ASM_OP_CMPXCHG1_REL:
		asm_cmpltr_add(i, ASM_CC_SEM, ASM_CT_REL);
		op = ASM_OP_CMPXCHG1;
		break;
	case ASM_OP_CMPXCHG2_ACQ:
		asm_cmpltr_add(i, ASM_CC_SEM, ASM_CT_ACQ);
		op = ASM_OP_CMPXCHG2;
		break;
	case ASM_OP_CMPXCHG2_REL:
		asm_cmpltr_add(i, ASM_CC_SEM, ASM_CT_REL);
		op = ASM_OP_CMPXCHG2;
		break;
	case ASM_OP_CMPXCHG4_ACQ:
		asm_cmpltr_add(i, ASM_CC_SEM, ASM_CT_ACQ);
		op = ASM_OP_CMPXCHG4;
		break;
	case ASM_OP_CMPXCHG4_REL:
		asm_cmpltr_add(i, ASM_CC_SEM, ASM_CT_REL);
		op = ASM_OP_CMPXCHG4;
		break;
	case ASM_OP_CMPXCHG8_ACQ:
		asm_cmpltr_add(i, ASM_CC_SEM, ASM_CT_ACQ);
		op = ASM_OP_CMPXCHG8;
		break;
	case ASM_OP_CMPXCHG8_REL:
		asm_cmpltr_add(i, ASM_CC_SEM, ASM_CT_REL);
		op = ASM_OP_CMPXCHG8;
		break;
	case ASM_OP_CZX1_L:
		asm_cmpltr_add(i, ASM_CC_LR, ASM_CT_L);
		op = ASM_OP_CZX1;
		break;
	case ASM_OP_CZX1_R:
		asm_cmpltr_add(i, ASM_CC_LR, ASM_CT_R);
		op = ASM_OP_CZX1;
		break;
	case ASM_OP_CZX2_L:
		asm_cmpltr_add(i, ASM_CC_LR, ASM_CT_L);
		op = ASM_OP_CZX2;
		break;
	case ASM_OP_CZX2_R:
		asm_cmpltr_add(i, ASM_CC_LR, ASM_CT_R);
		op = ASM_OP_CZX2;
		break;
	case ASM_OP_DEP_:
		asm_cmpltr_add(i, ASM_CC_DEP, ASM_CT_NONE);
		op = ASM_OP_DEP;
		break;
	case ASM_OP_DEP_Z:
		asm_cmpltr_add(i, ASM_CC_DEP, ASM_CT_Z);
		op = ASM_OP_DEP;
		break;
	case ASM_OP_FC_:
		asm_cmpltr_add(i, ASM_CC_FC, ASM_CT_NONE);
		op = ASM_OP_FC;
		break;
	case ASM_OP_FC_I:
		asm_cmpltr_add(i, ASM_CC_FC, ASM_CT_I);
		op = ASM_OP_FC;
		break;
	case ASM_OP_FCLASS_M:
		asm_cmpltr_add(i, ASM_CC_FCREL, ASM_CT_M);
		op = ASM_OP_FCLASS;
		break;
	case ASM_OP_FCVT_FX:
		asm_cmpltr_add(i, ASM_CC_FCVT, ASM_CT_FX);
		asm_cmpltr_add(i, ASM_CC_TRUNC, ASM_CT_NONE);
		op = ASM_OP_FCVT;
		break;
	case ASM_OP_FCVT_FX_TRUNC:
		asm_cmpltr_add(i, ASM_CC_FCVT, ASM_CT_FX);
		asm_cmpltr_add(i, ASM_CC_TRUNC, ASM_CT_TRUNC);
		op = ASM_OP_FCVT;
		break;
	case ASM_OP_FCVT_FXU:
		asm_cmpltr_add(i, ASM_CC_FCVT, ASM_CT_FXU);
		asm_cmpltr_add(i, ASM_CC_TRUNC, ASM_CT_NONE);
		op = ASM_OP_FCVT;
		break;
	case ASM_OP_FCVT_FXU_TRUNC:
		asm_cmpltr_add(i, ASM_CC_FCVT, ASM_CT_FXU);
		asm_cmpltr_add(i, ASM_CC_TRUNC, ASM_CT_TRUNC);
		op = ASM_OP_FCVT;
		break;
	case ASM_OP_FCVT_XF:
		asm_cmpltr_add(i, ASM_CC_FCVT, ASM_CT_XF);
		asm_cmpltr_add(i, ASM_CC_TRUNC, ASM_CT_NONE);
		op = ASM_OP_FCVT;
		break;
	case ASM_OP_FETCHADD4_ACQ:
		asm_cmpltr_add(i, ASM_CC_SEM, ASM_CT_ACQ);
		op = ASM_OP_FETCHADD4;
		break;
	case ASM_OP_FETCHADD4_REL:
		asm_cmpltr_add(i, ASM_CC_SEM, ASM_CT_REL);
		op = ASM_OP_FETCHADD4;
		break;
	case ASM_OP_FETCHADD8_ACQ:
		asm_cmpltr_add(i, ASM_CC_SEM, ASM_CT_ACQ);
		op = ASM_OP_FETCHADD8;
		break;
	case ASM_OP_FETCHADD8_REL:
		asm_cmpltr_add(i, ASM_CC_SEM, ASM_CT_REL);
		op = ASM_OP_FETCHADD8;
		break;
	case ASM_OP_FMA_:
		asm_cmpltr_add(i, ASM_CC_PC, ASM_CT_NONE);
		op = ASM_OP_FMA;
		break;
	case ASM_OP_FMA_D:
		asm_cmpltr_add(i, ASM_CC_PC, ASM_CT_D);
		op = ASM_OP_FMA;
		break;
	case ASM_OP_FMA_S:
		asm_cmpltr_add(i, ASM_CC_PC, ASM_CT_S);
		op = ASM_OP_FMA;
		break;
	case ASM_OP_FMERGE_NS:
		asm_cmpltr_add(i, ASM_CC_FMERGE, ASM_CT_NS);
		op = ASM_OP_FMERGE;
		break;
	case ASM_OP_FMERGE_S:
		asm_cmpltr_add(i, ASM_CC_FMERGE, ASM_CT_S);
		op = ASM_OP_FMERGE;
		break;
	case ASM_OP_FMERGE_SE:
		asm_cmpltr_add(i, ASM_CC_FMERGE, ASM_CT_SE);
		op = ASM_OP_FMERGE;
		break;
	case ASM_OP_FMIX_L:
		asm_cmpltr_add(i, ASM_CC_LR, ASM_CT_L);
		op = ASM_OP_FMIX;
		break;
	case ASM_OP_FMIX_LR:
		asm_cmpltr_add(i, ASM_CC_LR, ASM_CT_LR);
		op = ASM_OP_FMIX;
		break;
	case ASM_OP_FMIX_R:
		asm_cmpltr_add(i, ASM_CC_LR, ASM_CT_R);
		op = ASM_OP_FMIX;
		break;
	case ASM_OP_FMS_:
		asm_cmpltr_add(i, ASM_CC_PC, ASM_CT_NONE);
		op = ASM_OP_FMS;
		break;
	case ASM_OP_FMS_D:
		asm_cmpltr_add(i, ASM_CC_PC, ASM_CT_D);
		op = ASM_OP_FMS;
		break;
	case ASM_OP_FMS_S:
		asm_cmpltr_add(i, ASM_CC_PC, ASM_CT_S);
		op = ASM_OP_FMS;
		break;
	case ASM_OP_FNMA_:
		asm_cmpltr_add(i, ASM_CC_PC, ASM_CT_NONE);
		op = ASM_OP_FNMA;
		break;
	case ASM_OP_FNMA_D:
		asm_cmpltr_add(i, ASM_CC_PC, ASM_CT_D);
		op = ASM_OP_FNMA;
		break;
	case ASM_OP_FNMA_S:
		asm_cmpltr_add(i, ASM_CC_PC, ASM_CT_S);
		op = ASM_OP_FNMA;
		break;
	case ASM_OP_FPCMP_EQ:
		asm_cmpltr_add(i, ASM_CC_FREL, ASM_CT_EQ);
		op = ASM_OP_FPCMP;
		break;
	case ASM_OP_FPCMP_LE:
		asm_cmpltr_add(i, ASM_CC_FREL, ASM_CT_LE);
		op = ASM_OP_FPCMP;
		break;
	case ASM_OP_FPCMP_LT:
		asm_cmpltr_add(i, ASM_CC_FREL, ASM_CT_LT);
		op = ASM_OP_FPCMP;
		break;
	case ASM_OP_FPCMP_NEQ:
		asm_cmpltr_add(i, ASM_CC_FREL, ASM_CT_NEQ);
		op = ASM_OP_FPCMP;
		break;
	case ASM_OP_FPCMP_NLE:
		asm_cmpltr_add(i, ASM_CC_FREL, ASM_CT_NLE);
		op = ASM_OP_FPCMP;
		break;
	case ASM_OP_FPCMP_NLT:
		asm_cmpltr_add(i, ASM_CC_FREL, ASM_CT_NLT);
		op = ASM_OP_FPCMP;
		break;
	case ASM_OP_FPCMP_ORD:
		asm_cmpltr_add(i, ASM_CC_FREL, ASM_CT_ORD);
		op = ASM_OP_FPCMP;
		break;
	case ASM_OP_FPCMP_UNORD:
		asm_cmpltr_add(i, ASM_CC_FREL, ASM_CT_UNORD);
		op = ASM_OP_FPCMP;
		break;
	case ASM_OP_FPCVT_FX:
		asm_cmpltr_add(i, ASM_CC_FCVT, ASM_CT_FX);
		asm_cmpltr_add(i, ASM_CC_TRUNC, ASM_CT_NONE);
		op = ASM_OP_FPCVT;
		break;
	case ASM_OP_FPCVT_FX_TRUNC:
		asm_cmpltr_add(i, ASM_CC_FCVT, ASM_CT_FX);
		asm_cmpltr_add(i, ASM_CC_TRUNC, ASM_CT_TRUNC);
		op = ASM_OP_FPCVT;
		break;
	case ASM_OP_FPCVT_FXU:
		asm_cmpltr_add(i, ASM_CC_FCVT, ASM_CT_FXU);
		asm_cmpltr_add(i, ASM_CC_TRUNC, ASM_CT_NONE);
		op = ASM_OP_FPCVT;
		break;
	case ASM_OP_FPCVT_FXU_TRUNC:
		asm_cmpltr_add(i, ASM_CC_FCVT, ASM_CT_FXU);
		asm_cmpltr_add(i, ASM_CC_TRUNC, ASM_CT_TRUNC);
		op = ASM_OP_FPCVT;
		break;
	case ASM_OP_FPMERGE_NS:
		asm_cmpltr_add(i, ASM_CC_FMERGE, ASM_CT_NS);
		op = ASM_OP_FPMERGE;
		break;
	case ASM_OP_FPMERGE_S:
		asm_cmpltr_add(i, ASM_CC_FMERGE, ASM_CT_S);
		op = ASM_OP_FPMERGE;
		break;
	case ASM_OP_FPMERGE_SE:
		asm_cmpltr_add(i, ASM_CC_FMERGE, ASM_CT_SE);
		op = ASM_OP_FPMERGE;
		break;
	case ASM_OP_FSWAP_:
		asm_cmpltr_add(i, ASM_CC_FSWAP, ASM_CT_NONE);
		op = ASM_OP_FSWAP;
		break;
	case ASM_OP_FSWAP_NL:
		asm_cmpltr_add(i, ASM_CC_FSWAP, ASM_CT_NL);
		op = ASM_OP_FSWAP;
		break;
	case ASM_OP_FSWAP_NR:
		asm_cmpltr_add(i, ASM_CC_FSWAP, ASM_CT_NR);
		op = ASM_OP_FSWAP;
		break;
	case ASM_OP_FSXT_L:
		asm_cmpltr_add(i, ASM_CC_LR, ASM_CT_L);
		op = ASM_OP_FSXT;
		break;
	case ASM_OP_FSXT_R:
		asm_cmpltr_add(i, ASM_CC_LR, ASM_CT_R);
		op = ASM_OP_FSXT;
		break;
	case ASM_OP_GETF_D:
		asm_cmpltr_add(i, ASM_CC_GETF, ASM_CT_D);
		op = ASM_OP_GETF;
		break;
	case ASM_OP_GETF_EXP:
		asm_cmpltr_add(i, ASM_CC_GETF, ASM_CT_EXP);
		op = ASM_OP_GETF;
		break;
	case ASM_OP_GETF_S:
		asm_cmpltr_add(i, ASM_CC_GETF, ASM_CT_S);
		op = ASM_OP_GETF;
		break;
	case ASM_OP_GETF_SIG:
		asm_cmpltr_add(i, ASM_CC_GETF, ASM_CT_SIG);
		op = ASM_OP_GETF;
		break;
	case ASM_OP_HINT_B:
		asm_cmpltr_add(i, ASM_CC_UNIT, ASM_CT_B);
		op = ASM_OP_HINT;
		break;
	case ASM_OP_HINT_F:
		asm_cmpltr_add(i, ASM_CC_UNIT, ASM_CT_F);
		op = ASM_OP_HINT;
		break;
	case ASM_OP_HINT_I:
		asm_cmpltr_add(i, ASM_CC_UNIT, ASM_CT_I);
		op = ASM_OP_HINT;
		break;
	case ASM_OP_HINT_M:
		asm_cmpltr_add(i, ASM_CC_UNIT, ASM_CT_M);
		op = ASM_OP_HINT;
		break;
	case ASM_OP_HINT_X:
		asm_cmpltr_add(i, ASM_CC_UNIT, ASM_CT_X);
		op = ASM_OP_HINT;
		break;
	case ASM_OP_INVALA_:
		asm_cmpltr_add(i, ASM_CC_INVALA, ASM_CT_NONE);
		op = ASM_OP_INVALA;
		break;
	case ASM_OP_INVALA_E:
		asm_cmpltr_add(i, ASM_CC_INVALA, ASM_CT_E);
		op = ASM_OP_INVALA;
		break;
	case ASM_OP_ITC_D:
		asm_cmpltr_add(i, ASM_CC_ITC, ASM_CT_D);
		op = ASM_OP_ITC;
		break;
	case ASM_OP_ITC_I:
		asm_cmpltr_add(i, ASM_CC_ITC, ASM_CT_I);
		op = ASM_OP_ITC;
		break;
	case ASM_OP_ITR_D:
		asm_cmpltr_add(i, ASM_CC_ITR, ASM_CT_D);
		ot = ASM_OPER_DTR;
		op = ASM_OP_ITR;
		break;
	case ASM_OP_ITR_I:
		asm_cmpltr_add(i, ASM_CC_ITR, ASM_CT_I);
		ot = ASM_OPER_ITR;
		op = ASM_OP_ITR;
		break;
	case ASM_OP_LD1_:
		asm_cmpltr_add(i, ASM_CC_LDTYPE, ASM_CT_NONE);
		op = ASM_OP_LD1;
		break;
	case ASM_OP_LD1_A:
		asm_cmpltr_add(i, ASM_CC_LDTYPE, ASM_CT_A);
		op = ASM_OP_LD1;
		break;
	case ASM_OP_LD1_ACQ:
		asm_cmpltr_add(i, ASM_CC_LDTYPE, ASM_CT_ACQ);
		op = ASM_OP_LD1;
		break;
	case ASM_OP_LD1_BIAS:
		asm_cmpltr_add(i, ASM_CC_LDTYPE, ASM_CT_BIAS);
		op = ASM_OP_LD1;
		break;
	case ASM_OP_LD1_C_CLR:
		asm_cmpltr_add(i, ASM_CC_LDTYPE, ASM_CT_C_CLR);
		op = ASM_OP_LD1;
		break;
	case ASM_OP_LD1_C_CLR_ACQ:
		asm_cmpltr_add(i, ASM_CC_LDTYPE, ASM_CT_C_CLR_ACQ);
		op = ASM_OP_LD1;
		break;
	case ASM_OP_LD1_C_NC:
		asm_cmpltr_add(i, ASM_CC_LDTYPE, ASM_CT_C_NC);
		op = ASM_OP_LD1;
		break;
	case ASM_OP_LD1_S:
		asm_cmpltr_add(i, ASM_CC_LDTYPE, ASM_CT_S);
		op = ASM_OP_LD1;
		break;
	case ASM_OP_LD1_SA: 
		asm_cmpltr_add(i, ASM_CC_LDTYPE, ASM_CT_SA);
		op = ASM_OP_LD1;
		break;
	case ASM_OP_LD16_:
		asm_cmpltr_add(i, ASM_CC_LDTYPE, ASM_CT_NONE);
		op = ASM_OP_LD16;
		break;
	case ASM_OP_LD16_ACQ:
		asm_cmpltr_add(i, ASM_CC_LDTYPE, ASM_CT_ACQ);
		op = ASM_OP_LD16;
		break;
	case ASM_OP_LD2_:
		asm_cmpltr_add(i, ASM_CC_LDTYPE, ASM_CT_NONE);
		op = ASM_OP_LD2;
		break;
	case ASM_OP_LD2_A:
		asm_cmpltr_add(i, ASM_CC_LDTYPE, ASM_CT_A);
		op = ASM_OP_LD2;
		break;
	case ASM_OP_LD2_ACQ:
		asm_cmpltr_add(i, ASM_CC_LDTYPE, ASM_CT_ACQ);
		op = ASM_OP_LD2;
		break;
	case ASM_OP_LD2_BIAS:
		asm_cmpltr_add(i, ASM_CC_LDTYPE, ASM_CT_BIAS);
		op = ASM_OP_LD2;
		break;
	case ASM_OP_LD2_C_CLR:
		asm_cmpltr_add(i, ASM_CC_LDTYPE, ASM_CT_C_CLR);
		op = ASM_OP_LD2;
		break;
	case ASM_OP_LD2_C_CLR_ACQ:
		asm_cmpltr_add(i, ASM_CC_LDTYPE, ASM_CT_C_CLR_ACQ);
		op = ASM_OP_LD2;
		break;
	case ASM_OP_LD2_C_NC:
		asm_cmpltr_add(i, ASM_CC_LDTYPE, ASM_CT_C_NC);
		op = ASM_OP_LD2;
		break;
	case ASM_OP_LD2_S:
		asm_cmpltr_add(i, ASM_CC_LDTYPE, ASM_CT_S);
		op = ASM_OP_LD2;
		break;
	case ASM_OP_LD2_SA: 
		asm_cmpltr_add(i, ASM_CC_LDTYPE, ASM_CT_SA);
		op = ASM_OP_LD2;
		break;
	case ASM_OP_LD4_:
		asm_cmpltr_add(i, ASM_CC_LDTYPE, ASM_CT_NONE);
		op = ASM_OP_LD4;
		break;
	case ASM_OP_LD4_A:
		asm_cmpltr_add(i, ASM_CC_LDTYPE, ASM_CT_A);
		op = ASM_OP_LD4;
		break;
	case ASM_OP_LD4_ACQ:
		asm_cmpltr_add(i, ASM_CC_LDTYPE, ASM_CT_ACQ);
		op = ASM_OP_LD4;
		break;
	case ASM_OP_LD4_BIAS:
		asm_cmpltr_add(i, ASM_CC_LDTYPE, ASM_CT_BIAS);
		op = ASM_OP_LD4;
		break;
	case ASM_OP_LD4_C_CLR:
		asm_cmpltr_add(i, ASM_CC_LDTYPE, ASM_CT_C_CLR);
		op = ASM_OP_LD4;
		break;
	case ASM_OP_LD4_C_CLR_ACQ:
		asm_cmpltr_add(i, ASM_CC_LDTYPE, ASM_CT_C_CLR_ACQ);
		op = ASM_OP_LD4;
		break;
	case ASM_OP_LD4_C_NC:
		asm_cmpltr_add(i, ASM_CC_LDTYPE, ASM_CT_C_NC);
		op = ASM_OP_LD4;
		break;
	case ASM_OP_LD4_S:
		asm_cmpltr_add(i, ASM_CC_LDTYPE, ASM_CT_S);
		op = ASM_OP_LD4;
		break;
	case ASM_OP_LD4_SA: 
		asm_cmpltr_add(i, ASM_CC_LDTYPE, ASM_CT_SA);
		op = ASM_OP_LD4;
		break;
	case ASM_OP_LD8_:
		asm_cmpltr_add(i, ASM_CC_LDTYPE, ASM_CT_NONE);
		op = ASM_OP_LD8;
		break;
	case ASM_OP_LD8_A:
		asm_cmpltr_add(i, ASM_CC_LDTYPE, ASM_CT_A);
		op = ASM_OP_LD8;
		break;
	case ASM_OP_LD8_ACQ:
		asm_cmpltr_add(i, ASM_CC_LDTYPE, ASM_CT_ACQ);
		op = ASM_OP_LD8;
		break;
	case ASM_OP_LD8_BIAS:
		asm_cmpltr_add(i, ASM_CC_LDTYPE, ASM_CT_BIAS);
		op = ASM_OP_LD8;
		break;
	case ASM_OP_LD8_C_CLR:
		asm_cmpltr_add(i, ASM_CC_LDTYPE, ASM_CT_C_CLR);
		op = ASM_OP_LD8;
		break;
	case ASM_OP_LD8_C_CLR_ACQ:
		asm_cmpltr_add(i, ASM_CC_LDTYPE, ASM_CT_C_CLR_ACQ);
		op = ASM_OP_LD8;
		break;
	case ASM_OP_LD8_C_NC:
		asm_cmpltr_add(i, ASM_CC_LDTYPE, ASM_CT_C_NC);
		op = ASM_OP_LD8;
		break;
	case ASM_OP_LD8_FILL:
		asm_cmpltr_add(i, ASM_CC_LDTYPE, ASM_CT_FILL);
		op = ASM_OP_LD8;
		break;
	case ASM_OP_LD8_S:
		asm_cmpltr_add(i, ASM_CC_LDTYPE, ASM_CT_S);
		op = ASM_OP_LD8;
		break;
	case ASM_OP_LD8_SA: 
		asm_cmpltr_add(i, ASM_CC_LDTYPE, ASM_CT_SA);
		op = ASM_OP_LD8;
		break;
	case ASM_OP_LDF_FILL:
		asm_cmpltr_add(i, ASM_CC_FLDTYPE, ASM_CT_FILL);
		op = ASM_OP_LDF;
		break;
	case ASM_OP_LDF8_:
		asm_cmpltr_add(i, ASM_CC_FLDTYPE, ASM_CT_NONE);
		op = ASM_OP_LDF8;
		break;
	case ASM_OP_LDF8_A:
		asm_cmpltr_add(i, ASM_CC_FLDTYPE, ASM_CT_A);
		op = ASM_OP_LDF8;
		break;
	case ASM_OP_LDF8_C_CLR:
		asm_cmpltr_add(i, ASM_CC_FLDTYPE, ASM_CT_C_CLR);
		op = ASM_OP_LDF8;
		break;
	case ASM_OP_LDF8_C_NC:
		asm_cmpltr_add(i, ASM_CC_FLDTYPE, ASM_CT_C_NC);
		op = ASM_OP_LDF8;
		break;
	case ASM_OP_LDF8_S:
		asm_cmpltr_add(i, ASM_CC_FLDTYPE, ASM_CT_S);
		op = ASM_OP_LDF8;
		break;
	case ASM_OP_LDF8_SA:
		asm_cmpltr_add(i, ASM_CC_FLDTYPE, ASM_CT_SA);
		op = ASM_OP_LDF8;
		break;
	case ASM_OP_LDFD_:
		asm_cmpltr_add(i, ASM_CC_FLDTYPE, ASM_CT_NONE);
		op = ASM_OP_LDFD;
		break;
	case ASM_OP_LDFD_A:
		asm_cmpltr_add(i, ASM_CC_FLDTYPE, ASM_CT_A);
		op = ASM_OP_LDFD;
		break;
	case ASM_OP_LDFD_C_CLR:
		asm_cmpltr_add(i, ASM_CC_FLDTYPE, ASM_CT_C_CLR);
		op = ASM_OP_LDFD;
		break;
	case ASM_OP_LDFD_C_NC:
		asm_cmpltr_add(i, ASM_CC_FLDTYPE, ASM_CT_C_NC);
		op = ASM_OP_LDFD;
		break;
	case ASM_OP_LDFD_S:
		asm_cmpltr_add(i, ASM_CC_FLDTYPE, ASM_CT_S);
		op = ASM_OP_LDFD;
		break;
	case ASM_OP_LDFD_SA:
		asm_cmpltr_add(i, ASM_CC_FLDTYPE, ASM_CT_SA);
		op = ASM_OP_LDFD;
		break;
	case ASM_OP_LDFE_:
		asm_cmpltr_add(i, ASM_CC_FLDTYPE, ASM_CT_NONE);
		op = ASM_OP_LDFE;
		break;
	case ASM_OP_LDFE_A:
		asm_cmpltr_add(i, ASM_CC_FLDTYPE, ASM_CT_A);
		op = ASM_OP_LDFE;
		break;
	case ASM_OP_LDFE_C_CLR:
		asm_cmpltr_add(i, ASM_CC_FLDTYPE, ASM_CT_C_CLR);
		op = ASM_OP_LDFE;
		break;
	case ASM_OP_LDFE_C_NC:
		asm_cmpltr_add(i, ASM_CC_FLDTYPE, ASM_CT_C_NC);
		op = ASM_OP_LDFE;
		break;
	case ASM_OP_LDFE_S:
		asm_cmpltr_add(i, ASM_CC_FLDTYPE, ASM_CT_S);
		op = ASM_OP_LDFE;
		break;
	case ASM_OP_LDFE_SA:
		asm_cmpltr_add(i, ASM_CC_FLDTYPE, ASM_CT_SA);
		op = ASM_OP_LDFE;
		break;
	case ASM_OP_LDFP8_:
		asm_cmpltr_add(i, ASM_CC_FLDTYPE, ASM_CT_NONE);
		op = ASM_OP_LDFP8;
		break;
	case ASM_OP_LDFP8_A:
		asm_cmpltr_add(i, ASM_CC_FLDTYPE, ASM_CT_A);
		op = ASM_OP_LDFP8;
		break;
	case ASM_OP_LDFP8_C_CLR:
		asm_cmpltr_add(i, ASM_CC_FLDTYPE, ASM_CT_C_CLR);
		op = ASM_OP_LDFP8;
		break;
	case ASM_OP_LDFP8_C_NC:
		asm_cmpltr_add(i, ASM_CC_FLDTYPE, ASM_CT_C_NC);
		op = ASM_OP_LDFP8;
		break;
	case ASM_OP_LDFP8_S:
		asm_cmpltr_add(i, ASM_CC_FLDTYPE, ASM_CT_S);
		op = ASM_OP_LDFP8;
		break;
	case ASM_OP_LDFP8_SA:
		asm_cmpltr_add(i, ASM_CC_FLDTYPE, ASM_CT_SA);
		op = ASM_OP_LDFP8;
		break;
	case ASM_OP_LDFPD_:
		asm_cmpltr_add(i, ASM_CC_FLDTYPE, ASM_CT_NONE);
		op = ASM_OP_LDFPD;
		break;
	case ASM_OP_LDFPD_A:
		asm_cmpltr_add(i, ASM_CC_FLDTYPE, ASM_CT_A);
		op = ASM_OP_LDFPD;
		break;
	case ASM_OP_LDFPD_C_CLR:
		asm_cmpltr_add(i, ASM_CC_FLDTYPE, ASM_CT_C_CLR);
		op = ASM_OP_LDFPD;
		break;
	case ASM_OP_LDFPD_C_NC:
		asm_cmpltr_add(i, ASM_CC_FLDTYPE, ASM_CT_C_NC);
		op = ASM_OP_LDFPD;
		break;
	case ASM_OP_LDFPD_S:
		asm_cmpltr_add(i, ASM_CC_FLDTYPE, ASM_CT_S);
		op = ASM_OP_LDFPD;
		break;
	case ASM_OP_LDFPD_SA:
		asm_cmpltr_add(i, ASM_CC_FLDTYPE, ASM_CT_SA);
		op = ASM_OP_LDFPD;
		break;
	case ASM_OP_LDFPS_:
		asm_cmpltr_add(i, ASM_CC_FLDTYPE, ASM_CT_NONE);
		op = ASM_OP_LDFPS;
		break;
	case ASM_OP_LDFPS_A:
		asm_cmpltr_add(i, ASM_CC_FLDTYPE, ASM_CT_A);
		op = ASM_OP_LDFPS;
		break;
	case ASM_OP_LDFPS_C_CLR:
		asm_cmpltr_add(i, ASM_CC_FLDTYPE, ASM_CT_C_CLR);
		op = ASM_OP_LDFPS;
		break;
	case ASM_OP_LDFPS_C_NC:
		asm_cmpltr_add(i, ASM_CC_FLDTYPE, ASM_CT_C_NC);
		op = ASM_OP_LDFPS;
		break;
	case ASM_OP_LDFPS_S:
		asm_cmpltr_add(i, ASM_CC_FLDTYPE, ASM_CT_S);
		op = ASM_OP_LDFPS;
		break;
	case ASM_OP_LDFPS_SA:
		asm_cmpltr_add(i, ASM_CC_FLDTYPE, ASM_CT_SA);
		op = ASM_OP_LDFPS;
		break;
	case ASM_OP_LDFS_:
		asm_cmpltr_add(i, ASM_CC_FLDTYPE, ASM_CT_NONE);
		op = ASM_OP_LDFS;
		break;
	case ASM_OP_LDFS_A:
		asm_cmpltr_add(i, ASM_CC_FLDTYPE, ASM_CT_A);
		op = ASM_OP_LDFS;
		break;
	case ASM_OP_LDFS_C_CLR:
		asm_cmpltr_add(i, ASM_CC_FLDTYPE, ASM_CT_C_CLR);
		op = ASM_OP_LDFS;
		break;
	case ASM_OP_LDFS_C_NC:
		asm_cmpltr_add(i, ASM_CC_FLDTYPE, ASM_CT_C_NC);
		op = ASM_OP_LDFS;
		break;
	case ASM_OP_LDFS_S:
		asm_cmpltr_add(i, ASM_CC_FLDTYPE, ASM_CT_S);
		op = ASM_OP_LDFS;
		break;
	case ASM_OP_LDFS_SA:
		asm_cmpltr_add(i, ASM_CC_FLDTYPE, ASM_CT_SA);
		op = ASM_OP_LDFS;
		break;
	case ASM_OP_LFETCH_:
		asm_cmpltr_add(i, ASM_CC_LFTYPE, ASM_CT_NONE);
		asm_cmpltr_add(i, ASM_CC_LFETCH, ASM_CT_NONE);
		op = ASM_OP_LFETCH;
		break;
	case ASM_OP_LFETCH_EXCL:
		asm_cmpltr_add(i, ASM_CC_LFTYPE, ASM_CT_NONE);
		asm_cmpltr_add(i, ASM_CC_LFETCH, ASM_CT_EXCL);
		op = ASM_OP_LFETCH;
		break;
	case ASM_OP_LFETCH_FAULT:
		asm_cmpltr_add(i, ASM_CC_LFTYPE, ASM_CT_FAULT);
		asm_cmpltr_add(i, ASM_CC_LFETCH, ASM_CT_NONE);
		op = ASM_OP_LFETCH;
		break;
	case ASM_OP_LFETCH_FAULT_EXCL:
		asm_cmpltr_add(i, ASM_CC_LFTYPE, ASM_CT_FAULT);
		asm_cmpltr_add(i, ASM_CC_LFETCH, ASM_CT_EXCL);
		op = ASM_OP_LFETCH;
		break;
	case ASM_OP_MF_:
		asm_cmpltr_add(i, ASM_CC_MF, ASM_CT_NONE);
		op = ASM_OP_MF;
		break;
	case ASM_OP_MF_A:
		asm_cmpltr_add(i, ASM_CC_MF, ASM_CT_A);
		op = ASM_OP_MF;
		break;
	case ASM_OP_MIX1_L:
		asm_cmpltr_add(i, ASM_CC_LR, ASM_CT_L);
		op = ASM_OP_MIX1;
		break;
	case ASM_OP_MIX1_R:
		asm_cmpltr_add(i, ASM_CC_LR, ASM_CT_R);
		op = ASM_OP_MIX1;
		break;
	case ASM_OP_MIX2_L:
		asm_cmpltr_add(i, ASM_CC_LR, ASM_CT_L);
		op = ASM_OP_MIX2;
		break;
	case ASM_OP_MIX2_R:
		asm_cmpltr_add(i, ASM_CC_LR, ASM_CT_R);
		op = ASM_OP_MIX2;
		break;
	case ASM_OP_MIX4_L:
		asm_cmpltr_add(i, ASM_CC_LR, ASM_CT_L);
		op = ASM_OP_MIX4;
		break;
	case ASM_OP_MIX4_R:
		asm_cmpltr_add(i, ASM_CC_LR, ASM_CT_R);
		op = ASM_OP_MIX4;
		break;
	case ASM_OP_MOV_:
		asm_cmpltr_add(i, ASM_CC_MOV, ASM_CT_NONE);
		op = ASM_OP_MOV;
		break;
	case ASM_OP_MOV_I:
		asm_cmpltr_add(i, ASM_CC_UNIT, ASM_CT_I);
		op = ASM_OP_MOV;
		break;
	case ASM_OP_MOV_M:
		asm_cmpltr_add(i, ASM_CC_UNIT, ASM_CT_M);
		op = ASM_OP_MOV;
		break;
	case ASM_OP_MOV_RET:
		asm_cmpltr_add(i, ASM_CC_MOV, ASM_CT_RET);
		op = ASM_OP_MOV;
		break;
	case ASM_OP_MOV_CPUID:
		ot = ASM_OPER_CPUID;
		op = ASM_OP_MOV;
		break;
	case ASM_OP_MOV_DBR:
		ot = ASM_OPER_DBR;
		op = ASM_OP_MOV;
		break;
	case ASM_OP_MOV_IBR:
		ot = ASM_OPER_IBR;
		op = ASM_OP_MOV;
		break;
	case ASM_OP_MOV_IP:
		ot = ASM_OPER_IP;
		op = ASM_OP_MOV;
		break;
	case ASM_OP_MOV_MSR:
		ot = ASM_OPER_MSR;
		op = ASM_OP_MOV;
		break;
	case ASM_OP_MOV_PKR:
		ot = ASM_OPER_PKR;
		op = ASM_OP_MOV;
		break;
	case ASM_OP_MOV_PMC:
		ot = ASM_OPER_PMC;
		op = ASM_OP_MOV;
		break;
	case ASM_OP_MOV_PMD:
		ot = ASM_OPER_PMD;
		op = ASM_OP_MOV;
		break;
	case ASM_OP_MOV_PR:
		ot = ASM_OPER_PR;
		op = ASM_OP_MOV;
		break;
	case ASM_OP_MOV_PSR:
		ot = ASM_OPER_PSR;
		op = ASM_OP_MOV;
		break;
	case ASM_OP_MOV_PSR_L:
		ot = ASM_OPER_PSR_L;
		op = ASM_OP_MOV;
		break;
	case ASM_OP_MOV_PSR_UM:
		ot = ASM_OPER_PSR_UM;
		op = ASM_OP_MOV;
		break;
	case ASM_OP_MOV_RR:
		ot = ASM_OPER_RR;
		op = ASM_OP_MOV;
		break;
	case ASM_OP_NOP_B:
		asm_cmpltr_add(i, ASM_CC_UNIT, ASM_CT_B);
		op = ASM_OP_NOP;
		break;
	case ASM_OP_NOP_F:
		asm_cmpltr_add(i, ASM_CC_UNIT, ASM_CT_F);
		op = ASM_OP_NOP;
		break;
	case ASM_OP_NOP_I:
		asm_cmpltr_add(i, ASM_CC_UNIT, ASM_CT_I);
		op = ASM_OP_NOP;
		break;
	case ASM_OP_NOP_M:
		asm_cmpltr_add(i, ASM_CC_UNIT, ASM_CT_M);
		op = ASM_OP_NOP;
		break;
	case ASM_OP_NOP_X:
		asm_cmpltr_add(i, ASM_CC_UNIT, ASM_CT_X);
		op = ASM_OP_NOP;
		break;
	case ASM_OP_PACK2_SSS:
		asm_cmpltr_add(i, ASM_CC_SAT, ASM_CT_SSS);
		op = ASM_OP_PACK2;
		break;
	case ASM_OP_PACK2_USS:
		asm_cmpltr_add(i, ASM_CC_SAT, ASM_CT_USS);
		op = ASM_OP_PACK2;
		break;
	case ASM_OP_PACK4_SSS:
		asm_cmpltr_add(i, ASM_CC_SAT, ASM_CT_SSS);
		op = ASM_OP_PACK4;
		break;
	case ASM_OP_PADD1_:
		asm_cmpltr_add(i, ASM_CC_SAT, ASM_CT_NONE);
		op = ASM_OP_PADD1;
		break;
	case ASM_OP_PADD1_SSS:
		asm_cmpltr_add(i, ASM_CC_SAT, ASM_CT_SSS);
		op = ASM_OP_PADD1;
		break;
	case ASM_OP_PADD1_UUS:
		asm_cmpltr_add(i, ASM_CC_SAT, ASM_CT_UUS);
		op = ASM_OP_PADD1;
		break;
	case ASM_OP_PADD1_UUU:
		asm_cmpltr_add(i, ASM_CC_SAT, ASM_CT_UUU);
		op = ASM_OP_PADD1;
		break;
	case ASM_OP_PADD2_:
		asm_cmpltr_add(i, ASM_CC_SAT, ASM_CT_NONE);
		op = ASM_OP_PADD2;
		break;
	case ASM_OP_PADD2_SSS:
		asm_cmpltr_add(i, ASM_CC_SAT, ASM_CT_SSS);
		op = ASM_OP_PADD2;
		break;
	case ASM_OP_PADD2_UUS:
		asm_cmpltr_add(i, ASM_CC_SAT, ASM_CT_UUS);
		op = ASM_OP_PADD2;
		break;
	case ASM_OP_PADD2_UUU:
		asm_cmpltr_add(i, ASM_CC_SAT, ASM_CT_UUU);
		op = ASM_OP_PADD2;
		break;
	case ASM_OP_PAVG1_:
		asm_cmpltr_add(i, ASM_CC_PAVG, ASM_CT_NONE);
		op = ASM_OP_PAVG1;
		break;
	case ASM_OP_PAVG1_RAZ:
		asm_cmpltr_add(i, ASM_CC_PAVG, ASM_CT_RAZ);
		op = ASM_OP_PAVG1;
		break;
	case ASM_OP_PAVG2_:
		asm_cmpltr_add(i, ASM_CC_PAVG, ASM_CT_NONE);
		op = ASM_OP_PAVG2;
		break;
	case ASM_OP_PAVG2_RAZ:
		asm_cmpltr_add(i, ASM_CC_PAVG, ASM_CT_RAZ);
		op = ASM_OP_PAVG2;
		break;
	case ASM_OP_PCMP1_EQ:
		asm_cmpltr_add(i, ASM_CC_PREL, ASM_CT_EQ);
		op = ASM_OP_PCMP1;
		break;
	case ASM_OP_PCMP1_GT:
		asm_cmpltr_add(i, ASM_CC_PREL, ASM_CT_GT);
		op = ASM_OP_PCMP1;
		break;
	case ASM_OP_PCMP2_EQ:
		asm_cmpltr_add(i, ASM_CC_PREL, ASM_CT_EQ);
		op = ASM_OP_PCMP2;
		break;
	case ASM_OP_PCMP2_GT:
		asm_cmpltr_add(i, ASM_CC_PREL, ASM_CT_GT);
		op = ASM_OP_PCMP2;
		break;
	case ASM_OP_PCMP4_EQ:
		asm_cmpltr_add(i, ASM_CC_PREL, ASM_CT_EQ);
		op = ASM_OP_PCMP4;
		break;
	case ASM_OP_PCMP4_GT:
		asm_cmpltr_add(i, ASM_CC_PREL, ASM_CT_GT);
		op = ASM_OP_PCMP4;
		break;
	case ASM_OP_PMAX1_U:
		asm_cmpltr_add(i, ASM_CC_UNS, ASM_CT_U);
		op = ASM_OP_PMAX1;
		break;
	case ASM_OP_PMIN1_U:
		asm_cmpltr_add(i, ASM_CC_UNS, ASM_CT_U);
		op = ASM_OP_PMIN1;
		break;
	case ASM_OP_PMPY2_L:
		asm_cmpltr_add(i, ASM_CC_LR, ASM_CT_L);
		op = ASM_OP_PMPY2;
		break;
	case ASM_OP_PMPY2_R:
		asm_cmpltr_add(i, ASM_CC_LR, ASM_CT_R);
		op = ASM_OP_PMPY2;
		break;
	case ASM_OP_PMPYSHR2_:
		asm_cmpltr_add(i, ASM_CC_UNS, ASM_CT_NONE);
		op = ASM_OP_PMPYSHR2;
		break;
	case ASM_OP_PMPYSHR2_U:
		asm_cmpltr_add(i, ASM_CC_UNS, ASM_CT_U);
		op = ASM_OP_PMPYSHR2;
		break;
	case ASM_OP_PROBE_R:
		asm_cmpltr_add(i, ASM_CC_RW, ASM_CT_R);
		asm_cmpltr_add(i, ASM_CC_PRTYPE, ASM_CT_NONE);
		op = ASM_OP_PROBE;
		break;
	case ASM_OP_PROBE_R_FAULT:
		asm_cmpltr_add(i, ASM_CC_RW, ASM_CT_R);
		asm_cmpltr_add(i, ASM_CC_PRTYPE, ASM_CT_FAULT);
		op = ASM_OP_PROBE;
		break;
	case ASM_OP_PROBE_RW_FAULT:
		asm_cmpltr_add(i, ASM_CC_RW, ASM_CT_RW);
		asm_cmpltr_add(i, ASM_CC_PRTYPE, ASM_CT_FAULT);
		op = ASM_OP_PROBE;
		break;
	case ASM_OP_PROBE_W:
		asm_cmpltr_add(i, ASM_CC_RW, ASM_CT_W);
		asm_cmpltr_add(i, ASM_CC_PRTYPE, ASM_CT_NONE);
		op = ASM_OP_PROBE;
		break;
	case ASM_OP_PROBE_W_FAULT:
		asm_cmpltr_add(i, ASM_CC_RW, ASM_CT_W);
		asm_cmpltr_add(i, ASM_CC_PRTYPE, ASM_CT_FAULT);
		op = ASM_OP_PROBE;
		break;
	case ASM_OP_PSHR2_:
		asm_cmpltr_add(i, ASM_CC_UNS, ASM_CT_NONE);
		op = ASM_OP_PSHR2;
		break;
	case ASM_OP_PSHR2_U:
		asm_cmpltr_add(i, ASM_CC_UNS, ASM_CT_U);
		op = ASM_OP_PSHR2;
		break;
	case ASM_OP_PSHR4_:
		asm_cmpltr_add(i, ASM_CC_UNS, ASM_CT_NONE);
		op = ASM_OP_PSHR4;
		break;
	case ASM_OP_PSHR4_U:
		asm_cmpltr_add(i, ASM_CC_UNS, ASM_CT_U);
		op = ASM_OP_PSHR4;
		break;
	case ASM_OP_PSUB1_:
		asm_cmpltr_add(i, ASM_CC_SAT, ASM_CT_NONE);
		op = ASM_OP_PSUB1;
		break;
	case ASM_OP_PSUB1_SSS:
		asm_cmpltr_add(i, ASM_CC_SAT, ASM_CT_SSS);
		op = ASM_OP_PSUB1;
		break;
	case ASM_OP_PSUB1_UUS:
		asm_cmpltr_add(i, ASM_CC_SAT, ASM_CT_UUS);
		op = ASM_OP_PSUB1;
		break;
	case ASM_OP_PSUB1_UUU:
		asm_cmpltr_add(i, ASM_CC_SAT, ASM_CT_UUU);
		op = ASM_OP_PSUB1;
		break;
	case ASM_OP_PSUB2_:
		asm_cmpltr_add(i, ASM_CC_SAT, ASM_CT_NONE);
		op = ASM_OP_PSUB2;
		break;
	case ASM_OP_PSUB2_SSS:
		asm_cmpltr_add(i, ASM_CC_SAT, ASM_CT_SSS);
		op = ASM_OP_PSUB2;
		break;
	case ASM_OP_PSUB2_UUS:
		asm_cmpltr_add(i, ASM_CC_SAT, ASM_CT_UUS);
		op = ASM_OP_PSUB2;
		break;
	case ASM_OP_PSUB2_UUU:
		asm_cmpltr_add(i, ASM_CC_SAT, ASM_CT_UUU);
		op = ASM_OP_PSUB2;
		break;
	case ASM_OP_PTC_E:
		asm_cmpltr_add(i, ASM_CC_PTC, ASM_CT_E);
		op = ASM_OP_PTC;
		break;
	case ASM_OP_PTC_G:
		asm_cmpltr_add(i, ASM_CC_PTC, ASM_CT_G);
		op = ASM_OP_PTC;
		break;
	case ASM_OP_PTC_GA:
		asm_cmpltr_add(i, ASM_CC_PTC, ASM_CT_GA);
		op = ASM_OP_PTC;
		break;
	case ASM_OP_PTC_L:
		asm_cmpltr_add(i, ASM_CC_PTC, ASM_CT_L);
		op = ASM_OP_PTC;
		break;
	case ASM_OP_PTR_D:
		asm_cmpltr_add(i, ASM_CC_PTR, ASM_CT_D);
		op = ASM_OP_PTR;
		break;
	case ASM_OP_PTR_I:
		asm_cmpltr_add(i, ASM_CC_PTR, ASM_CT_I);
		op = ASM_OP_PTR;
		break;
	case ASM_OP_SETF_D:
		asm_cmpltr_add(i, ASM_CC_SETF, ASM_CT_D);
		op = ASM_OP_SETF;
		break;
	case ASM_OP_SETF_EXP:
		asm_cmpltr_add(i, ASM_CC_SETF, ASM_CT_EXP);
		op = ASM_OP_SETF;
		break;
	case ASM_OP_SETF_S:
		asm_cmpltr_add(i, ASM_CC_SETF, ASM_CT_S);
		op = ASM_OP_SETF;
		break;
	case ASM_OP_SETF_SIG:
		asm_cmpltr_add(i, ASM_CC_SETF, ASM_CT_SIG);
		op = ASM_OP_SETF;
		break;
	case ASM_OP_SHR_:
		asm_cmpltr_add(i, ASM_CC_UNS, ASM_CT_NONE);
		op = ASM_OP_SHR;
		break;
	case ASM_OP_SHR_U:
		asm_cmpltr_add(i, ASM_CC_UNS, ASM_CT_U);
		op = ASM_OP_SHR;
		break;
	case ASM_OP_SRLZ_D:
		asm_cmpltr_add(i, ASM_CC_SRLZ, ASM_CT_D);
		op = ASM_OP_SRLZ;
		break;
	case ASM_OP_SRLZ_I:
		asm_cmpltr_add(i, ASM_CC_SRLZ, ASM_CT_I);
		op = ASM_OP_SRLZ;
		break;
	case ASM_OP_ST1_:
		asm_cmpltr_add(i, ASM_CC_STTYPE, ASM_CT_NONE);
		op = ASM_OP_ST1;
		break;
	case ASM_OP_ST1_REL:
		asm_cmpltr_add(i, ASM_CC_STTYPE, ASM_CT_REL);
		op = ASM_OP_ST1;
		break;
	case ASM_OP_ST16_:
		asm_cmpltr_add(i, ASM_CC_STTYPE, ASM_CT_NONE);
		op = ASM_OP_ST16;
		break;
	case ASM_OP_ST16_REL:
		asm_cmpltr_add(i, ASM_CC_STTYPE, ASM_CT_REL);
		op = ASM_OP_ST16;
		break;
	case ASM_OP_ST2_:
		asm_cmpltr_add(i, ASM_CC_STTYPE, ASM_CT_NONE);
		op = ASM_OP_ST2;
		break;
	case ASM_OP_ST2_REL:
		asm_cmpltr_add(i, ASM_CC_STTYPE, ASM_CT_REL);
		op = ASM_OP_ST2;
		break;
	case ASM_OP_ST4_:
		asm_cmpltr_add(i, ASM_CC_STTYPE, ASM_CT_NONE);
		op = ASM_OP_ST4;
		break;
	case ASM_OP_ST4_REL:
		asm_cmpltr_add(i, ASM_CC_STTYPE, ASM_CT_REL);
		op = ASM_OP_ST4;
		break;
	case ASM_OP_ST8_:
		asm_cmpltr_add(i, ASM_CC_STTYPE, ASM_CT_NONE);
		op = ASM_OP_ST8;
		break;
	case ASM_OP_ST8_REL:
		asm_cmpltr_add(i, ASM_CC_STTYPE, ASM_CT_REL);
		op = ASM_OP_ST8;
		break;
	case ASM_OP_ST8_SPILL:
		asm_cmpltr_add(i, ASM_CC_STTYPE, ASM_CT_SPILL);
		op = ASM_OP_ST8;
		break;
	case ASM_OP_STF_SPILL:
		asm_cmpltr_add(i, ASM_CC_STTYPE, ASM_CT_SPILL);
		op = ASM_OP_STF;
		break;
	case ASM_OP_SYNC_I:
		asm_cmpltr_add(i, ASM_CC_SYNC, ASM_CT_I);
		op = ASM_OP_SYNC;
		break;
	case ASM_OP_TBIT_NZ_AND:
		asm_cmpltr_add(i, ASM_CC_TREL, ASM_CT_NZ);
		asm_cmpltr_add(i, ASM_CC_CTYPE, ASM_CT_AND);
		op = ASM_OP_TBIT;
		break;
	case ASM_OP_TBIT_NZ_OR:
		asm_cmpltr_add(i, ASM_CC_TREL, ASM_CT_NZ);
		asm_cmpltr_add(i, ASM_CC_CTYPE, ASM_CT_OR);
		op = ASM_OP_TBIT;
		break;
	case ASM_OP_TBIT_NZ_OR_ANDCM:
		asm_cmpltr_add(i, ASM_CC_TREL, ASM_CT_NZ);
		asm_cmpltr_add(i, ASM_CC_CTYPE, ASM_CT_OR_ANDCM);
		op = ASM_OP_TBIT;
		break;
	case ASM_OP_TBIT_Z:
		asm_cmpltr_add(i, ASM_CC_TREL, ASM_CT_Z);
		asm_cmpltr_add(i, ASM_CC_CTYPE, ASM_CT_NONE);
		op = ASM_OP_TBIT;
		break;
	case ASM_OP_TBIT_Z_AND:
		asm_cmpltr_add(i, ASM_CC_TREL, ASM_CT_Z);
		asm_cmpltr_add(i, ASM_CC_CTYPE, ASM_CT_AND);
		op = ASM_OP_TBIT;
		break;
	case ASM_OP_TBIT_Z_OR:
		asm_cmpltr_add(i, ASM_CC_TREL, ASM_CT_Z);
		asm_cmpltr_add(i, ASM_CC_CTYPE, ASM_CT_OR);
		op = ASM_OP_TBIT;
		break;
	case ASM_OP_TBIT_Z_OR_ANDCM:
		asm_cmpltr_add(i, ASM_CC_TREL, ASM_CT_Z);
		asm_cmpltr_add(i, ASM_CC_CTYPE, ASM_CT_OR_ANDCM);
		op = ASM_OP_TBIT;
		break;
	case ASM_OP_TBIT_Z_UNC:
		asm_cmpltr_add(i, ASM_CC_TREL, ASM_CT_Z);
		asm_cmpltr_add(i, ASM_CC_CTYPE, ASM_CT_UNC);
		op = ASM_OP_TBIT;
		break;
	case ASM_OP_TF_NZ_AND:
		asm_cmpltr_add(i, ASM_CC_TREL, ASM_CT_NZ);
		asm_cmpltr_add(i, ASM_CC_CTYPE, ASM_CT_AND);
		op = ASM_OP_TF;
		break;
	case ASM_OP_TF_NZ_OR:
		asm_cmpltr_add(i, ASM_CC_TREL, ASM_CT_NZ);
		asm_cmpltr_add(i, ASM_CC_CTYPE, ASM_CT_OR);
		op = ASM_OP_TF;
		break;
	case ASM_OP_TF_NZ_OR_ANDCM:
		asm_cmpltr_add(i, ASM_CC_TREL, ASM_CT_NZ);
		asm_cmpltr_add(i, ASM_CC_CTYPE, ASM_CT_OR_ANDCM);
		op = ASM_OP_TF;
		break;
	case ASM_OP_TF_Z:
		asm_cmpltr_add(i, ASM_CC_TREL, ASM_CT_Z);
		asm_cmpltr_add(i, ASM_CC_CTYPE, ASM_CT_NONE);
		op = ASM_OP_TF;
		break;
	case ASM_OP_TF_Z_AND:
		asm_cmpltr_add(i, ASM_CC_TREL, ASM_CT_Z);
		asm_cmpltr_add(i, ASM_CC_CTYPE, ASM_CT_AND);
		op = ASM_OP_TF;
		break;
	case ASM_OP_TF_Z_OR:
		asm_cmpltr_add(i, ASM_CC_TREL, ASM_CT_Z);
		asm_cmpltr_add(i, ASM_CC_CTYPE, ASM_CT_OR);
		op = ASM_OP_TF;
		break;
	case ASM_OP_TF_Z_OR_ANDCM:
		asm_cmpltr_add(i, ASM_CC_TREL, ASM_CT_Z);
		asm_cmpltr_add(i, ASM_CC_CTYPE, ASM_CT_OR_ANDCM);
		op = ASM_OP_TF;
		break;
	case ASM_OP_TF_Z_UNC:
		asm_cmpltr_add(i, ASM_CC_TREL, ASM_CT_Z);
		asm_cmpltr_add(i, ASM_CC_CTYPE, ASM_CT_UNC);
		op = ASM_OP_TF;
		break;
	case ASM_OP_TNAT_NZ_AND:
		asm_cmpltr_add(i, ASM_CC_TREL, ASM_CT_NZ);
		asm_cmpltr_add(i, ASM_CC_CTYPE, ASM_CT_AND);
		op = ASM_OP_TNAT;
		break;
	case ASM_OP_TNAT_NZ_OR:
		asm_cmpltr_add(i, ASM_CC_TREL, ASM_CT_NZ);
		asm_cmpltr_add(i, ASM_CC_CTYPE, ASM_CT_OR);
		op = ASM_OP_TNAT;
		break;
	case ASM_OP_TNAT_NZ_OR_ANDCM:
		asm_cmpltr_add(i, ASM_CC_TREL, ASM_CT_NZ);
		asm_cmpltr_add(i, ASM_CC_CTYPE, ASM_CT_OR_ANDCM);
		op = ASM_OP_TNAT;
		break;
	case ASM_OP_TNAT_Z:
		asm_cmpltr_add(i, ASM_CC_TREL, ASM_CT_Z);
		asm_cmpltr_add(i, ASM_CC_CTYPE, ASM_CT_NONE);
		op = ASM_OP_TNAT;
		break;
	case ASM_OP_TNAT_Z_AND:
		asm_cmpltr_add(i, ASM_CC_TREL, ASM_CT_Z);
		asm_cmpltr_add(i, ASM_CC_CTYPE, ASM_CT_AND);
		op = ASM_OP_TNAT;
		break;
	case ASM_OP_TNAT_Z_OR:
		asm_cmpltr_add(i, ASM_CC_TREL, ASM_CT_Z);
		asm_cmpltr_add(i, ASM_CC_CTYPE, ASM_CT_OR);
		op = ASM_OP_TNAT;
		break;
	case ASM_OP_TNAT_Z_OR_ANDCM:
		asm_cmpltr_add(i, ASM_CC_TREL, ASM_CT_Z);
		asm_cmpltr_add(i, ASM_CC_CTYPE, ASM_CT_OR_ANDCM);
		op = ASM_OP_TNAT;
		break;
	case ASM_OP_TNAT_Z_UNC:
		asm_cmpltr_add(i, ASM_CC_TREL, ASM_CT_Z);
		asm_cmpltr_add(i, ASM_CC_CTYPE, ASM_CT_UNC);
		op = ASM_OP_TNAT;
		break;
	case ASM_OP_UNPACK1_H:
		asm_cmpltr_add(i, ASM_CC_UNPACK, ASM_CT_H);
		op = ASM_OP_UNPACK1;
		break;
	case ASM_OP_UNPACK1_L:
		asm_cmpltr_add(i, ASM_CC_UNPACK, ASM_CT_L);
		op = ASM_OP_UNPACK1;
		break;
	case ASM_OP_UNPACK2_H:
		asm_cmpltr_add(i, ASM_CC_UNPACK, ASM_CT_H);
		op = ASM_OP_UNPACK2;
		break;
	case ASM_OP_UNPACK2_L:
		asm_cmpltr_add(i, ASM_CC_UNPACK, ASM_CT_L);
		op = ASM_OP_UNPACK2;
		break;
	case ASM_OP_UNPACK4_H:
		asm_cmpltr_add(i, ASM_CC_UNPACK, ASM_CT_H);
		op = ASM_OP_UNPACK4;
		break;
	case ASM_OP_UNPACK4_L:
		asm_cmpltr_add(i, ASM_CC_UNPACK, ASM_CT_L);
		op = ASM_OP_UNPACK4;
		break;
	case ASM_OP_VMSW_0:
		asm_cmpltr_add(i, ASM_CC_VMSW, ASM_CT_0);
		op = ASM_OP_VMSW;
		break;
	case ASM_OP_VMSW_1:
		asm_cmpltr_add(i, ASM_CC_VMSW, ASM_CT_1);
		op = ASM_OP_VMSW;
		break;
	case ASM_OP_XMA_H:
		asm_cmpltr_add(i, ASM_CC_XMA, ASM_CT_H);
		op = ASM_OP_XMA;
		break;
	case ASM_OP_XMA_HU:
		asm_cmpltr_add(i, ASM_CC_XMA, ASM_CT_HU);
		op = ASM_OP_XMA;
		break;
	case ASM_OP_XMA_L:
		asm_cmpltr_add(i, ASM_CC_XMA, ASM_CT_L);
		op = ASM_OP_XMA;
		break;
	default:
		KASSERT(op < ASM_OP_NUMBER_OF_OPCODES, ("foo"));
		break;
	}
	i->i_op = op;
	return (ot);
}

static __inline void
op_imm(struct asm_inst *i, int op, uint64_t val)
{
	i->i_oper[op].o_type = ASM_OPER_IMM;
	i->i_oper[op].o_value = val;
}

static __inline void
op_type(struct asm_inst *i, int op, enum asm_oper_type ot)
{
	i->i_oper[op].o_type = ot;
}

static __inline void
op_value(struct asm_inst *i, int op, uint64_t val)
{
	i->i_oper[op].o_value = val;
}

static __inline void
operand(struct asm_inst *i, int op, enum asm_oper_type ot, uint64_t bits,
    int o, int l)
{
	i->i_oper[op].o_type = ot;
	i->i_oper[op].o_value = FIELD(bits, o, l);
}

static uint64_t
imm(uint64_t bits, int sign, int o, int l)
{
	uint64_t val = FIELD(bits, o, l);

	if (sign && (val & (1LL << (l - 1))) != 0)
		val |= -1LL << l;
	return (val);
}

static void
s_imm(struct asm_inst *i, int op, uint64_t bits, int o, int l)
{
	i->i_oper[op].o_type = ASM_OPER_IMM;
	i->i_oper[op].o_value = imm(bits, 1, o, l);
}

static void
u_imm(struct asm_inst *i, int op, uint64_t bits, int o, int l)
{
	i->i_oper[op].o_type = ASM_OPER_IMM;
	i->i_oper[op].o_value = imm(bits, 0, o, l);
}

static uint64_t
vimm(uint64_t bits, int sign, va_list ap)
{
	uint64_t val = 0;
	int len = 0;
	int frag;

	while ((frag = va_arg(ap, int)) != 0) {
		val |= (uint64_t)FIELD(bits, FRAG_OFS(frag), FRAG_LEN(frag))
		    << len;
		len += FRAG_LEN(frag);
	}
	if (sign && (val & (1LL << (len - 1))) != 0)
		val |= -1LL << len;
	return (val);
}

static void
s_immf(struct asm_inst *i, int op, uint64_t bits, ...)
{
	va_list ap;
	va_start(ap, bits);
	i->i_oper[op].o_type = ASM_OPER_IMM;
	i->i_oper[op].o_value = vimm(bits, 1, ap);
	va_end(ap);
}

static void
u_immf(struct asm_inst *i, int op, uint64_t bits, ...)
{
	va_list ap;
	va_start(ap, bits);
	i->i_oper[op].o_type = ASM_OPER_IMM;
	i->i_oper[op].o_value = vimm(bits, 0, ap);
	va_end(ap);
}

static void
disp(struct asm_inst *i, int op, uint64_t bits, ...)
{
	va_list ap;
	va_start(ap, bits);
	i->i_oper[op].o_type = ASM_OPER_DISP;
	i->i_oper[op].o_value = vimm(bits, 1, ap) << 4;
	va_end(ap);
}

static __inline void
combine(uint64_t *dst, int dl, uint64_t src, int sl, int so)
{
	*dst = (*dst & ((1LL << dl) - 1LL)) |
	    ((uint64_t)_FLD64(src, so, sl) << dl);
}

int
asm_extract(enum asm_op op, enum asm_fmt fmt, uint64_t bits,
    struct asm_bundle *b, int slot)
{
	struct asm_inst *i = b->b_inst + slot;
	enum asm_oper_type ot;

	KASSERT(op != ASM_OP_NONE, ("foo"));
	i->i_bits = bits;
	i->i_format = fmt;
	i->i_srcidx = 2;

	ot = asm_normalize(i, op);

	if (fmt != ASM_FMT_B6 && fmt != ASM_FMT_B7)
		operand(i, 0, ASM_OPER_PREG, bits, 0, 6);

	switch (fmt) {
	case ASM_FMT_A1:
		operand(i, 1, ASM_OPER_GREG, bits, 6, 7);
		operand(i, 2, ASM_OPER_GREG, bits, 13, 7);
		operand(i, 3, ASM_OPER_GREG, bits, 20, 7);
		if ((op == ASM_OP_ADD && FIELD(bits, 27, 2) == 1) ||
		    (op == ASM_OP_SUB && FIELD(bits, 27, 2) == 0))
			op_imm(i, 4, 1LL);
		break;
	case ASM_FMT_A2:
		operand(i, 1, ASM_OPER_GREG, bits, 6, 7);
		operand(i, 2, ASM_OPER_GREG, bits, 13, 7);
		op_imm(i, 3, 1LL + FIELD(bits, 27, 2));
		operand(i, 4, ASM_OPER_GREG, bits, 20, 7);
		break;
	case ASM_FMT_A3:
		operand(i, 1, ASM_OPER_GREG, bits, 6, 7);
		s_immf(i, 2, bits, FRAG(13,7), FRAG(36,1), 0);
		operand(i, 3, ASM_OPER_GREG, bits, 20, 7);
		break;
	case ASM_FMT_A4:
		operand(i, 1, ASM_OPER_GREG, bits, 6, 7);
		s_immf(i, 2, bits, FRAG(13,7), FRAG(27,6), FRAG(36,1), 0);
		operand(i, 3, ASM_OPER_GREG, bits, 20, 7);
		break;
	case ASM_FMT_A5:
		operand(i, 1, ASM_OPER_GREG, bits, 6, 7);
		s_immf(i, 2, bits, FRAG(13,7), FRAG(27,9), FRAG(22,5),
		    FRAG(36,1), 0);
		operand(i, 3, ASM_OPER_GREG, bits, 20, 2);
		break;
	case ASM_FMT_A6: /* 2 dst */
		operand(i, 1, ASM_OPER_PREG, bits, 6, 6);
		operand(i, 2, ASM_OPER_PREG, bits, 27, 6);
		operand(i, 3, ASM_OPER_GREG, bits, 13, 7);
		operand(i, 4, ASM_OPER_GREG, bits, 20, 7);
		i->i_srcidx++;
		break;
	case ASM_FMT_A7: /* 2 dst */
		if (FIELD(bits, 13, 7) != 0)
			return (0);
		operand(i, 1, ASM_OPER_PREG, bits, 6, 6);
		operand(i, 2, ASM_OPER_PREG, bits, 27, 6);
		operand(i, 3, ASM_OPER_GREG, bits, 13, 7);
		operand(i, 4, ASM_OPER_GREG, bits, 20, 7);
		i->i_srcidx++;
		break;
	case ASM_FMT_A8: /* 2 dst */
		operand(i, 1, ASM_OPER_PREG, bits, 6, 6);
		operand(i, 2, ASM_OPER_PREG, bits, 27, 6);
		s_immf(i, 3, bits, FRAG(13,7), FRAG(36,1), 0);
		operand(i, 4, ASM_OPER_GREG, bits, 20, 7);
		i->i_srcidx++;
		break;
	case ASM_FMT_A9:
		operand(i, 1, ASM_OPER_GREG, bits, 6, 7);
		operand(i, 2, ASM_OPER_GREG, bits, 13, 7);
		operand(i, 3, ASM_OPER_GREG, bits, 20, 7);
		break;
	case ASM_FMT_A10:
		operand(i, 1, ASM_OPER_GREG, bits, 6, 7);
		operand(i, 2, ASM_OPER_GREG, bits, 13, 7);
		op_imm(i, 3, 1LL + FIELD(bits, 27, 2));
		operand(i, 4, ASM_OPER_GREG, bits, 20, 7);
		break;
	case ASM_FMT_B1: /* 0 dst */
		asm_brhint(i);
		disp(i, 1, bits, FRAG(13,20), FRAG(36,1), 0);
		break;
	case ASM_FMT_B2: /* 0 dst */
		if (FIELD(bits, 0, 6) != 0)
			return (0);
		asm_brhint(i);
		disp(i, 1, bits, FRAG(13,20), FRAG(36,1), 0);
		break;
	case ASM_FMT_B3:
		asm_brhint(i);
		operand(i, 1, ASM_OPER_BREG, bits, 6, 3);
		disp(i, 2, bits, FRAG(13,20), FRAG(36,1), 0);
		break;
	case ASM_FMT_B4: /* 0 dst */
		asm_brhint(i);
		operand(i, 1, ASM_OPER_BREG, bits, 13, 3);
		break;
	case ASM_FMT_B5:
#if 0
		if (FIELD(bits, 32, 1) == 0)
			return (0);
#endif
		asm_brhint(i);
		operand(i, 1, ASM_OPER_BREG, bits, 6, 3);
		operand(i, 2, ASM_OPER_BREG, bits, 13, 3);
		break;
	case ASM_FMT_B6: /* 0 dst */
		asm_brphint(i);
		disp(i, 1, bits, FRAG(13,20), FRAG(36,1), 0);
		disp(i, 2, bits, FRAG(6,7), FRAG(33,2), 0);
		i->i_srcidx--;
		break;
	case ASM_FMT_B7: /* 0 dst */
		asm_brphint(i);
		operand(i, 1, ASM_OPER_BREG, bits, 13, 3);
		disp(i, 2, bits, FRAG(6,7), FRAG(33,2), 0);
		i->i_srcidx--;
		break;
	case ASM_FMT_B8:
		/* no operands */
		break;
	case ASM_FMT_B9: /* 0 dst */
		u_immf(i, 1, bits, FRAG(6,20), FRAG(36,1), 0);
		break;
	case ASM_FMT_F1:
		asm_sf(i);
		operand(i, 1, ASM_OPER_FREG, bits, 6, 7);
		operand(i, 2, ASM_OPER_FREG, bits, 13, 7);
		operand(i, 3, ASM_OPER_FREG, bits, 20, 7);
		operand(i, 4, ASM_OPER_FREG, bits, 27, 7);
		break;
	case ASM_FMT_F2:
		operand(i, 1, ASM_OPER_FREG, bits, 6, 7);
		operand(i, 2, ASM_OPER_FREG, bits, 13, 7);
		operand(i, 3, ASM_OPER_FREG, bits, 20, 7);
		operand(i, 4, ASM_OPER_FREG, bits, 27, 7);
		break;
	case ASM_FMT_F3:
		operand(i, 1, ASM_OPER_FREG, bits, 6, 7);
		operand(i, 2, ASM_OPER_FREG, bits, 13, 7);
		operand(i, 3, ASM_OPER_FREG, bits, 20, 7);
		operand(i, 4, ASM_OPER_FREG, bits, 27, 7);
		break;
	case ASM_FMT_F4: /* 2 dst */
		if (FIELD(bits, 33, 1)) { /* ra */
			if (FIELD(bits, 36, 1)) /* rb */
				asm_cmpltr_add(i, ASM_CC_FREL, ASM_CT_UNORD);
			else
				asm_cmpltr_add(i, ASM_CC_FREL, ASM_CT_LE);
		} else {
			if (FIELD(bits, 36, 1)) /* rb */
				asm_cmpltr_add(i, ASM_CC_FREL, ASM_CT_LT);
			else
				asm_cmpltr_add(i, ASM_CC_FREL, ASM_CT_EQ);
		}
		if (FIELD(bits, 12, 1)) /* ta */
			asm_cmpltr_add(i, ASM_CC_FCTYPE, ASM_CT_UNC);
		else
			asm_cmpltr_add(i, ASM_CC_FCTYPE, ASM_CT_NONE);
		asm_sf(i);
		operand(i, 1, ASM_OPER_PREG, bits, 6, 6);
		operand(i, 2, ASM_OPER_PREG, bits, 27, 6);
		operand(i, 3, ASM_OPER_FREG, bits, 13, 7);
		operand(i, 4, ASM_OPER_FREG, bits, 20, 7);
		i->i_srcidx++;
		break;
	case ASM_FMT_F5: /* 2 dst */
		operand(i, 1, ASM_OPER_PREG, bits, 6, 6);
		operand(i, 2, ASM_OPER_PREG, bits, 27, 6);
		operand(i, 3, ASM_OPER_FREG, bits, 13, 7);
		u_immf(i, 4, bits, FRAG(33,2), FRAG(20,7), 0);
		i->i_srcidx++;
		break;
	case ASM_FMT_F6: /* 2 dst */
		asm_sf(i);
		operand(i, 1, ASM_OPER_FREG, bits, 6, 7);
		operand(i, 2, ASM_OPER_PREG, bits, 27, 6);
		operand(i, 3, ASM_OPER_FREG, bits, 13, 7);
		operand(i, 4, ASM_OPER_FREG, bits, 20, 7);
		i->i_srcidx++;
		break;
	case ASM_FMT_F7: /* 2 dst */
		asm_sf(i);
		operand(i, 1, ASM_OPER_FREG, bits, 6, 7);
		operand(i, 2, ASM_OPER_PREG, bits, 27, 6);
		operand(i, 3, ASM_OPER_FREG, bits, 20, 7);
		i->i_srcidx++;
		break;
	case ASM_FMT_F8:
		asm_sf(i);
		operand(i, 1, ASM_OPER_FREG, bits, 6, 7);
		operand(i, 2, ASM_OPER_FREG, bits, 13, 7);
		operand(i, 3, ASM_OPER_FREG, bits, 20, 7);
		break;
	case ASM_FMT_F9:
		operand(i, 1, ASM_OPER_FREG, bits, 6, 7);
		operand(i, 2, ASM_OPER_FREG, bits, 13, 7);
		operand(i, 3, ASM_OPER_FREG, bits, 20, 7);
		break;
	case ASM_FMT_F10:
		asm_sf(i);
		operand(i, 1, ASM_OPER_FREG, bits, 6, 7);
		operand(i, 2, ASM_OPER_FREG, bits, 13, 7);
		break;
	case ASM_FMT_F11:
		operand(i, 1, ASM_OPER_FREG, bits, 6, 7);
		operand(i, 2, ASM_OPER_FREG, bits, 13, 7);
		break;
	case ASM_FMT_F12: /* 0 dst */
		asm_sf(i);
		u_imm(i, 1, bits, 13, 7);
		u_imm(i, 2, bits, 20, 7);
		i->i_srcidx--;
		break;
	case ASM_FMT_F13:
		asm_sf(i);
		/* no operands */
		break;
	case ASM_FMT_F14: /* 0 dst */
		asm_sf(i);
		disp(i, 1, bits, FRAG(6,20), FRAG(36,1), 0);
		break;
	case ASM_FMT_F15: /* 0 dst */
		u_imm(i, 1, bits, 6, 20);
		break;
	case ASM_FMT_F16: /* 0 dst */
		u_imm(i, 1, bits, 6, 20);
		break;
	case ASM_FMT_I1:
		operand(i, 1, ASM_OPER_GREG, bits, 6, 7);
		operand(i, 2, ASM_OPER_GREG, bits, 13, 7);
		operand(i, 3, ASM_OPER_GREG, bits, 20, 7);
		switch (FIELD(bits, 30, 2)) {
		case 0:	op_imm(i, 4, 0LL); break;
		case 1: op_imm(i, 4, 7LL); break;
		case 2: op_imm(i, 4, 15LL); break;
		case 3: op_imm(i, 4, 16LL); break;
		}
		break;
	case ASM_FMT_I2:
		operand(i, 1, ASM_OPER_GREG, bits, 6, 7);
		operand(i, 2, ASM_OPER_GREG, bits, 13, 7);
		operand(i, 3, ASM_OPER_GREG, bits, 20, 7);
		break;
	case ASM_FMT_I3:
		operand(i, 1, ASM_OPER_GREG, bits, 6, 7);
		operand(i, 2, ASM_OPER_GREG, bits, 13, 7);
		u_imm(i, 3, bits, 20, 4);
		break;
	case ASM_FMT_I4:
		operand(i, 1, ASM_OPER_GREG, bits, 6, 7);
		operand(i, 2, ASM_OPER_GREG, bits, 13, 7);
		u_imm(i, 3, bits, 20, 8);
		break;
	case ASM_FMT_I5:
		operand(i, 1, ASM_OPER_GREG, bits, 6, 7);
		operand(i, 2, ASM_OPER_GREG, bits, 20, 7);
		operand(i, 3, ASM_OPER_GREG, bits, 13, 7);
		break;
	case ASM_FMT_I6:
		operand(i, 1, ASM_OPER_GREG, bits, 6, 7);
		operand(i, 2, ASM_OPER_GREG, bits, 20, 7);
		u_imm(i, 3, bits, 14, 5);
		break;
	case ASM_FMT_I7:
		operand(i, 1, ASM_OPER_GREG, bits, 6, 7);
		operand(i, 2, ASM_OPER_GREG, bits, 13, 7);
		operand(i, 3, ASM_OPER_GREG, bits, 20, 7);
		break;
	case ASM_FMT_I8:
		operand(i, 1, ASM_OPER_GREG, bits, 6, 7);
		operand(i, 2, ASM_OPER_GREG, bits, 13, 7);
		op_imm(i, 3, 31LL - FIELD(bits, 20, 5));
		break;
	case ASM_FMT_I9:
		if (FIELD(bits, 13, 7) != 0)
			return (0);
		operand(i, 1, ASM_OPER_GREG, bits, 6, 7);
		operand(i, 2, ASM_OPER_GREG, bits, 20, 7);
		break;
	case ASM_FMT_I10:
		operand(i, 1, ASM_OPER_GREG, bits, 6, 7);
		operand(i, 2, ASM_OPER_GREG, bits, 13, 7);
		operand(i, 3, ASM_OPER_GREG, bits, 20, 7);
		u_imm(i, 4, bits, 27, 6);
		break;
	case ASM_FMT_I11:
		operand(i, 1, ASM_OPER_GREG, bits, 6, 7);
		operand(i, 2, ASM_OPER_GREG, bits, 20, 7);
		u_imm(i, 3, bits, 14, 6);
		op_imm(i, 4, 1LL + FIELD(bits, 27, 6));
		break;
	case ASM_FMT_I12:
		operand(i, 1, ASM_OPER_GREG, bits, 6, 7);
		operand(i, 2, ASM_OPER_GREG, bits, 13, 7);
		op_imm(i, 3, 63LL - FIELD(bits, 20, 6));
		op_imm(i, 4, 1LL + FIELD(bits, 27, 6));
		break;
	case ASM_FMT_I13:
		operand(i, 1, ASM_OPER_GREG, bits, 6, 7);
		s_immf(i, 2, bits, FRAG(13,7), FRAG(36,1), 0);
		op_imm(i, 3, 63LL - FIELD(bits, 20, 6));
		op_imm(i, 4, 1LL + FIELD(bits, 27, 6));
		break;
	case ASM_FMT_I14:
		operand(i, 1, ASM_OPER_GREG, bits, 6, 7);
		s_imm(i, 2, bits, 36, 1);
		operand(i, 3, ASM_OPER_GREG, bits, 20, 7);
		op_imm(i, 4, 63LL - FIELD(bits, 14, 6));
		op_imm(i, 5, 1LL + FIELD(bits, 27, 6));
		break;
	case ASM_FMT_I15:
		operand(i, 1, ASM_OPER_GREG, bits, 6, 7);
		operand(i, 2, ASM_OPER_GREG, bits, 13, 7);
		operand(i, 3, ASM_OPER_GREG, bits, 20, 7);
		op_imm(i, 4, 63LL - FIELD(bits, 31, 6));
		op_imm(i, 5, 1LL + FIELD(bits, 27, 4));
		break;
	case ASM_FMT_I16: /* 2 dst */
		operand(i, 1, ASM_OPER_PREG, bits, 6, 6);
		operand(i, 2, ASM_OPER_PREG, bits, 27, 6);
		operand(i, 3, ASM_OPER_GREG, bits, 20, 7);
		u_imm(i, 4, bits, 14, 6);
		i->i_srcidx++;
		break;
	case ASM_FMT_I17: /* 2 dst */
		operand(i, 1, ASM_OPER_PREG, bits, 6, 6);
		operand(i, 2, ASM_OPER_PREG, bits, 27, 6);
		operand(i, 3, ASM_OPER_GREG, bits, 20, 7);
		i->i_srcidx++;
		break;
	case ASM_FMT_I18:
		u_immf(i, 1, bits, FRAG(6,20), FRAG(36,1), 0);
		break;
	case ASM_FMT_I19:
		u_immf(i, 1, bits, FRAG(6,20), FRAG(36,1), 0);
		break;
	case ASM_FMT_I20: /* 0 dst */
		operand(i, 1, ASM_OPER_GREG, bits, 13, 7);
		disp(i, 2, bits, FRAG(6,7), FRAG(20,13), FRAG(36,1), 0);
		i->i_srcidx--;
		break;
	case ASM_FMT_I21:
		switch (FIELD(bits, 20, 2)) { /* wh */
		case 0:	asm_cmpltr_add(i, ASM_CC_MWH, ASM_CT_SPTK); break;
		case 1:	asm_cmpltr_add(i, ASM_CC_MWH, ASM_CT_NONE); break;
		case 2:	asm_cmpltr_add(i, ASM_CC_MWH, ASM_CT_DPTK); break;
		case 3:	return (0);
		}
		if (FIELD(bits, 23, 1)) /* ih */
			asm_cmpltr_add(i, ASM_CC_IH, ASM_CT_IMP);
		else
			asm_cmpltr_add(i, ASM_CC_IH, ASM_CT_NONE);
		operand(i, 1, ASM_OPER_BREG, bits, 6, 3);
		operand(i, 2, ASM_OPER_GREG, bits, 13, 7);
		disp(i, 3, bits, FRAG(24,9), 0);
		break;
	case ASM_FMT_I22:
		operand(i, 1, ASM_OPER_GREG, bits, 6, 7);
		operand(i, 2, ASM_OPER_BREG, bits, 13, 3);
		break;
	case ASM_FMT_I23:
		op_type(i, 1, ASM_OPER_PR);
		operand(i, 2, ASM_OPER_GREG, bits, 13, 7);
		u_immf(i, 3, bits, FRAG(6,7), FRAG(24,8), FRAG(36,1), 0);
		i->i_oper[3].o_value <<= 1;
		break;
	case ASM_FMT_I24:
		op_type(i, 1, ASM_OPER_PR_ROT);
		s_immf(i, 2, bits, FRAG(6,27), FRAG(36,1), 0);
		break;
	case ASM_FMT_I25:
		operand(i, 1, ASM_OPER_GREG, bits, 6, 7);
		op_type(i, 2, ot);
		break;
	case ASM_FMT_I26:
		operand(i, 1, ASM_OPER_AREG, bits, 20, 7);
		operand(i, 2, ASM_OPER_GREG, bits, 13, 7);
		break;
	case ASM_FMT_I27:
		operand(i, 1, ASM_OPER_AREG, bits, 20, 7);
		s_immf(i, 2, bits, FRAG(13,7), FRAG(36,1), 0);
		break;
	case ASM_FMT_I28:
		operand(i, 1, ASM_OPER_GREG, bits, 6, 7);
		operand(i, 2, ASM_OPER_AREG, bits, 20, 7);
		break;
	case ASM_FMT_I29:
		operand(i, 1, ASM_OPER_GREG, bits, 6, 7);
		operand(i, 2, ASM_OPER_GREG, bits, 20, 7);
		break;
	case ASM_FMT_I30: /* 2 dst */
		operand(i, 1, ASM_OPER_PREG, bits, 6, 6);
		operand(i, 2, ASM_OPER_PREG, bits, 27, 6);
		op_imm(i, 3, 32LL + FIELD(bits, 14, 5));
		i->i_srcidx++;
		break;
	case ASM_FMT_M1:
		asm_hint(i, ASM_CC_LDHINT);
		operand(i, 1, ASM_OPER_GREG, bits, 6, 7);
		if (i->i_op == ASM_OP_LD16) {
			op_type(i, 2, ASM_OPER_AREG);
			op_value(i, 2, AR_CSD);
			i->i_srcidx++;
		}
		operand(i, i->i_srcidx, ASM_OPER_MEM, bits, 20, 7);
		break;
	case ASM_FMT_M2:
		asm_hint(i, ASM_CC_LDHINT);
		operand(i, 1, ASM_OPER_GREG, bits, 6, 7);
		operand(i, 2, ASM_OPER_MEM, bits, 20, 7);
		operand(i, 3, ASM_OPER_GREG, bits, 13, 7);
		break;
	case ASM_FMT_M3:
		asm_hint(i, ASM_CC_LDHINT);
		operand(i, 1, ASM_OPER_GREG, bits, 6, 7);
		operand(i, 2, ASM_OPER_MEM, bits, 20, 7);
		s_immf(i, 3, bits, FRAG(13,7), FRAG(27,1), FRAG(36,1), 0);
		break;
	case ASM_FMT_M4:
		asm_hint(i, ASM_CC_STHINT);
		operand(i, 1, ASM_OPER_MEM, bits, 20, 7);
		operand(i, 2, ASM_OPER_GREG, bits, 13, 7);
		if (i->i_op == ASM_OP_ST16) {
			op_type(i, 3, ASM_OPER_AREG);
			op_value(i, 3, AR_CSD);
		}
		break;
	case ASM_FMT_M5:
		asm_hint(i, ASM_CC_STHINT);
		operand(i, 1, ASM_OPER_MEM, bits, 20, 7);
		operand(i, 2, ASM_OPER_GREG, bits, 13, 7);
		s_immf(i, 3, bits, FRAG(6,7), FRAG(27,1), FRAG(36,1), 0);
		break;
	case ASM_FMT_M6:
		asm_hint(i, ASM_CC_LDHINT);
		operand(i, 1, ASM_OPER_FREG, bits, 6, 7);
		operand(i, 2, ASM_OPER_MEM, bits, 20, 7);
		break;
	case ASM_FMT_M7:
		asm_hint(i, ASM_CC_LDHINT);
		operand(i, 1, ASM_OPER_FREG, bits, 6, 7);
		operand(i, 2, ASM_OPER_MEM, bits, 20, 7);
		operand(i, 3, ASM_OPER_GREG, bits, 13, 7);
		break;
	case ASM_FMT_M8:
		asm_hint(i, ASM_CC_LDHINT);
		operand(i, 1, ASM_OPER_FREG, bits, 6, 7);
		operand(i, 2, ASM_OPER_MEM, bits, 20, 7);
		s_immf(i, 3, bits, FRAG(13,7), FRAG(27,1), FRAG(36,1), 0);
		break;
	case ASM_FMT_M9:
		asm_hint(i, ASM_CC_STHINT);
		operand(i, 1, ASM_OPER_MEM, bits, 20, 7);
		operand(i, 2, ASM_OPER_FREG, bits, 13, 7);
		break;
	case ASM_FMT_M10:
		asm_hint(i, ASM_CC_STHINT);
		operand(i, 1, ASM_OPER_MEM, bits, 20, 7);
		operand(i, 2, ASM_OPER_FREG, bits, 13, 7);
		s_immf(i, 3, bits, FRAG(6,7), FRAG(27,1), FRAG(36,1), 0);
		break;
	case ASM_FMT_M11: /* 2 dst */
		asm_hint(i, ASM_CC_LDHINT);
		operand(i, 1, ASM_OPER_FREG, bits, 6, 7);
		operand(i, 2, ASM_OPER_FREG, bits, 13, 7);
		operand(i, 3, ASM_OPER_MEM, bits, 20, 7);
		i->i_srcidx++;
		break;
	case ASM_FMT_M12: /* 2 dst */
		asm_hint(i, ASM_CC_LDHINT);
		operand(i, 1, ASM_OPER_FREG, bits, 6, 7);
		operand(i, 2, ASM_OPER_FREG, bits, 13, 7);
		operand(i, 3, ASM_OPER_MEM, bits, 20, 7);
		op_imm(i, 4, 8LL << FIELD(bits, 30, 1));
		i->i_srcidx++;
		break;
	case ASM_FMT_M13:
		asm_hint(i, ASM_CC_LFHINT);
		operand(i, 1, ASM_OPER_MEM, bits, 20, 7);
		break;
	case ASM_FMT_M14: /* 0 dst */
		asm_hint(i, ASM_CC_LFHINT);
		operand(i, 1, ASM_OPER_MEM, bits, 20, 7);
		operand(i, 2, ASM_OPER_GREG, bits, 13, 7);
		i->i_srcidx--;
		break;
	case ASM_FMT_M15: /* 0 dst */
		asm_hint(i, ASM_CC_LFHINT);
		operand(i, 1, ASM_OPER_MEM, bits, 20, 7);
		s_immf(i, 2, bits, FRAG(13,7), FRAG(27,1), FRAG(36,1), 0);
		i->i_srcidx--;
		break;
	case ASM_FMT_M16:
		asm_hint(i, ASM_CC_LDHINT);
		operand(i, 1, ASM_OPER_GREG, bits, 6, 7);
		operand(i, 2, ASM_OPER_MEM, bits, 20, 7);
		operand(i, 3, ASM_OPER_GREG, bits, 13, 7);
		if (i->i_op == ASM_OP_CMP8XCHG16) {
			op_type(i, 4, ASM_OPER_AREG);
			op_value(i, 4, AR_CSD);
			op_type(i, 5, ASM_OPER_AREG);
			op_value(i, 5, AR_CCV);
		} else {
			if (FIELD(bits, 30, 6) < 8) {
				op_type(i, 4, ASM_OPER_AREG);
				op_value(i, 4, AR_CCV);
			}
		}
		break;
	case ASM_FMT_M17:
		asm_hint(i, ASM_CC_LDHINT);
		operand(i, 1, ASM_OPER_GREG, bits, 6, 7);
		operand(i, 2, ASM_OPER_MEM, bits, 20, 7);
		switch (FIELD(bits, 13, 2)) {
		case 0: op_imm(i, 3, 1LL << 4); break;
		case 1: op_imm(i, 3, 1LL << 3); break;
		case 2:	op_imm(i, 3, 1LL << 2); break;
		case 3: op_imm(i, 3, 1LL); break;
		}
		if (FIELD(bits, 15, 1))
			i->i_oper[3].o_value *= -1LL;
		break;
	case ASM_FMT_M18:
		operand(i, 1, ASM_OPER_FREG, bits, 6, 7);
		operand(i, 2, ASM_OPER_GREG, bits, 13, 7);
		break;
	case ASM_FMT_M19:
		operand(i, 1, ASM_OPER_GREG, bits, 6, 7);
		operand(i, 2, ASM_OPER_FREG, bits, 13, 7);
		break;
	case ASM_FMT_M20: /* 0 dst */
		operand(i, 1, ASM_OPER_GREG, bits, 13, 7);
		disp(i, 2, bits, FRAG(6,7), FRAG(20,13), FRAG(36,1), 0);
		i->i_srcidx--;
		break;
	case ASM_FMT_M21: /* 0 dst */
		operand(i, 1, ASM_OPER_FREG, bits, 13, 7);
		disp(i, 2, bits, FRAG(6,7), FRAG(20,13), FRAG(36,1), 0);
		i->i_srcidx--;
		break;
	case ASM_FMT_M22: /* 0 dst */
		operand(i, 1, ASM_OPER_GREG, bits, 6, 7);
		disp(i, 2, bits, FRAG(13,20), FRAG(36,1), 0);
		i->i_srcidx--;
		break;
	case ASM_FMT_M23: /* 0 dst */
		operand(i, 1, ASM_OPER_FREG, bits, 6, 7);
		disp(i, 2, bits, FRAG(13,20), FRAG(36,1), 0);
		i->i_srcidx--;
		break;
	case ASM_FMT_M24:
		/* no operands */
		break;
	case ASM_FMT_M25:
		if (FIELD(bits, 0, 6) != 0)
			return (0);
		/* no operands */
		break;
	case ASM_FMT_M26:
		operand(i, 1, ASM_OPER_GREG, bits, 6, 7);
		break;
	case ASM_FMT_M27:
		operand(i, 1, ASM_OPER_FREG, bits, 6, 7);
		break;
	case ASM_FMT_M28:
		operand(i, 1, ASM_OPER_GREG, bits, 20, 7);
		break;
	case ASM_FMT_M29:
		operand(i, 1, ASM_OPER_AREG, bits, 20, 7);
		operand(i, 2, ASM_OPER_GREG, bits, 13, 7);
		break;
	case ASM_FMT_M30:
		operand(i, 1, ASM_OPER_AREG, bits, 20, 7);
		s_immf(i, 2, bits, FRAG(13,7), FRAG(36,1), 0);
		break;
	case ASM_FMT_M31:
		operand(i, 1, ASM_OPER_GREG, bits, 6, 7);
		operand(i, 2, ASM_OPER_AREG, bits, 20, 7);
		break;
	case ASM_FMT_M32:
		operand(i, 1, ASM_OPER_CREG, bits, 20, 7);
		operand(i, 2, ASM_OPER_GREG, bits, 13, 7);
		break;
	case ASM_FMT_M33:
		operand(i, 1, ASM_OPER_GREG, bits, 6, 7);
		operand(i, 2, ASM_OPER_CREG, bits, 20, 7);
		break;
	case ASM_FMT_M34: {
		uint64_t loc, out;
		operand(i, 1, ASM_OPER_GREG, bits, 6, 7);
		op_type(i, 2, ASM_OPER_AREG);
		op_value(i, 2, AR_PFS);
		loc = FIELD(bits, 20, 7);
		out = FIELD(bits, 13, 7) - loc;
		op_imm(i, 3, 0);
		op_imm(i, 4, loc);
		op_imm(i, 5, out);
		op_imm(i, 6, (uint64_t)FIELD(bits, 27, 4) << 3);
		break;
	}
	case ASM_FMT_M35:
		if (FIELD(bits, 27, 6) == 0x2D)
			op_type(i, 1, ASM_OPER_PSR_L);
		else
			op_type(i, 1, ASM_OPER_PSR_UM);
		operand(i, 2, ASM_OPER_GREG, bits, 13, 7);
		break;
	case ASM_FMT_M36:
		operand(i, 1, ASM_OPER_GREG, bits, 6, 7);
		if (FIELD(bits, 27, 6) == 0x25)
			op_type(i, 2, ASM_OPER_PSR);
		else
			op_type(i, 2, ASM_OPER_PSR_UM);
		break;
	case ASM_FMT_M37:
		u_immf(i, 1, bits, FRAG(6,20), FRAG(36,1), 0);
		break;
	case ASM_FMT_M38:
		operand(i, 1, ASM_OPER_GREG, bits, 6, 7);
		operand(i, 2, ASM_OPER_GREG, bits, 20, 7);
		operand(i, 3, ASM_OPER_GREG, bits, 13, 7);
		break;
	case ASM_FMT_M39:
		operand(i, 1, ASM_OPER_GREG, bits, 6, 7);
		operand(i, 2, ASM_OPER_GREG, bits, 20, 7);
		u_imm(i, 3, bits, 13, 2);
		break;
	case ASM_FMT_M40: /* 0 dst */
		operand(i, 1, ASM_OPER_GREG, bits, 20, 7);
		u_imm(i, 2, bits, 13, 2);
		i->i_srcidx--;
		break;
	case ASM_FMT_M41:
		operand(i, 1, ASM_OPER_GREG, bits, 13, 7);
		break;
	case ASM_FMT_M42:
		operand(i, 1, ot, bits, 20, 7);
		operand(i, 2, ASM_OPER_GREG, bits, 13, 7);
		break;
	case ASM_FMT_M43:
		operand(i, 1, ASM_OPER_GREG, bits, 6, 7);
		operand(i, 2, ot, bits, 20, 7);
		break;
	case ASM_FMT_M44:
		u_immf(i, 1, bits, FRAG(6,21), FRAG(31,2), FRAG(36,1), 0);
		break;
	case ASM_FMT_M45: /* 0 dst */
		operand(i, 1, ASM_OPER_GREG, bits, 20, 7);
		operand(i, 2, ASM_OPER_GREG, bits, 13, 7);
		i->i_srcidx--;
		break;
	case ASM_FMT_M46:
		operand(i, 1, ASM_OPER_GREG, bits, 6, 7);
		operand(i, 2, ASM_OPER_GREG, bits, 20, 7);
		break;
	case ASM_FMT_M47:
		operand(i, 1, ASM_OPER_GREG, bits, 20, 7);
		break;
	case ASM_FMT_M48:
		u_immf(i, 1, bits, FRAG(6,20), FRAG(36,1), 0);
		break;
	case ASM_FMT_X1:
		KASSERT(slot == 2, ("foo"));
		u_immf(i, 1, bits, FRAG(6,20), FRAG(36,1), 0);
		combine(&i->i_oper[1].o_value, 21, b->b_inst[1].i_bits, 41, 0);
		break;
	case ASM_FMT_X2:
		KASSERT(slot == 2, ("foo"));
		operand(i, 1, ASM_OPER_GREG, bits, 6, 7);
		u_immf(i, 2, bits, FRAG(13,7), FRAG(27,9), FRAG(22,5),
		    FRAG(21,1), 0);
		combine(&i->i_oper[2].o_value, 22, b->b_inst[1].i_bits, 41, 0);
		combine(&i->i_oper[2].o_value, 63, bits, 1, 36);
		break;
	case ASM_FMT_X3:
		KASSERT(slot == 2, ("foo"));
		asm_brhint(i);
		u_imm(i, 1, bits, 13, 20);
		combine(&i->i_oper[1].o_value, 20, b->b_inst[1].i_bits, 39, 2);
		combine(&i->i_oper[1].o_value, 59, bits, 1, 36);
		i->i_oper[1].o_value <<= 4;
		i->i_oper[1].o_type = ASM_OPER_DISP;
		break;
	case ASM_FMT_X4:
		KASSERT(slot == 2, ("foo"));
		asm_brhint(i);
		operand(i, 1, ASM_OPER_BREG, bits, 6, 3);
		u_imm(i, 2, bits, 13, 20);
		combine(&i->i_oper[2].o_value, 20, b->b_inst[1].i_bits, 39, 2);
		combine(&i->i_oper[2].o_value, 59, bits, 1, 36);
		i->i_oper[2].o_value <<= 4;
		i->i_oper[2].o_type = ASM_OPER_DISP;
		break;
	case ASM_FMT_X5:
		KASSERT(slot == 2, ("foo"));
		u_immf(i, 1, bits, FRAG(6,20), FRAG(36,1), 0);
		combine(&i->i_oper[1].o_value, 21, b->b_inst[1].i_bits, 41, 0);
		break;
	default:
		KASSERT(fmt == ASM_FMT_NONE, ("foo"));
		return (0);
	}

	return (1);
}
