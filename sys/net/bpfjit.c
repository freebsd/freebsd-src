/*-
 * Copyright (c) 2011-2012 Alexander Nasonov.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE
 * COPYRIGHT HOLDERS OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/cdefs.h>
#ifdef _KERNEL
__KERNEL_RCSID(0, "$NetBSD$");
#else
__RCSID("$NetBSD$");
#endif

#include <net/bpfjit.h>

#ifndef _KERNEL
#include <assert.h>
#define BPFJIT_ASSERT(c) assert(c)
#else
#define BPFJIT_ASSERT(c) KASSERT(c)
#endif

#ifndef _KERNEL
#include <stdlib.h>
#define BPFJIT_MALLOC(sz) malloc(sz)
#define BPFJIT_FREE(p) free(p)
#else
#include <sys/malloc.h>
#define BPFJIT_MALLOC(sz) kern_malloc(sz, M_WAITOK)
#define BPFJIT_FREE(p) kern_free(p)
#endif

#ifndef _KERNEL
#include <limits.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#else
#include <machine/limits.h>
#include <sys/null.h>
#include <sys/types.h>
#include <sys/atomic.h>
#include <sys/module.h>
#endif

#include <sys/queue.h>
#include <sys/types.h>

#include <sljitLir.h>

#if !defined(_KERNEL) && defined(SLJIT_VERBOSE) && SLJIT_VERBOSE
#include <stdio.h> /* for stderr */
#endif


#define BPFJIT_A	SLJIT_TEMPORARY_REG1
#define BPFJIT_X	SLJIT_TEMPORARY_EREG1
#define BPFJIT_TMP1	SLJIT_TEMPORARY_REG2
#define BPFJIT_TMP2	SLJIT_TEMPORARY_REG3
#define BPFJIT_BUF	SLJIT_SAVED_REG1
#define BPFJIT_WIRELEN	SLJIT_SAVED_REG2
#define BPFJIT_BUFLEN	SLJIT_SAVED_REG3
#define BPFJIT_KERN_TMP SLJIT_TEMPORARY_EREG2

/* 
 * Flags for bpfjit_optimization_hints().
 */
#define BPFJIT_INIT_X 0x10000
#define BPFJIT_INIT_A 0x20000


/*
 * Node of bj_jumps list.
 */
struct bpfjit_jump
{
	struct sljit_jump *bj_jump;
	SLIST_ENTRY(bpfjit_jump) bj_entries;
	uint32_t bj_safe_length;
};

/*
 * Data for BPF_JMP instruction.
 */
struct bpfjit_jump_data
{
	/*
	 * These entries make up bj_jumps list:
	 * bj_jtf[0] - when coming from jt path,
	 * bj_jtf[1] - when coming from jf path.
	 */
	struct bpfjit_jump bj_jtf[2];
};

/*
 * Data for "read from packet" instructions.
 * See also read_pkt_insn() function below.
 */
struct bpfjit_read_pkt_data
{
	/*
	 * If positive, emit "if (buflen < bj_check_length) return 0".
	 * We assume that buflen is never equal to UINT32_MAX (otherwise,
	 * we need a special bool variable to emit unconditional "return 0").
	 */
	uint32_t bj_check_length;
};

/*
 * Additional (optimization-related) data for bpf_insn.
 */
struct bpfjit_insn_data
{
	/* List of jumps to this insn. */
	SLIST_HEAD(, bpfjit_jump) bj_jumps;

	union {
		struct bpfjit_jump_data     bj_jdata;
		struct bpfjit_read_pkt_data bj_rdata;
	} bj_aux;

	bool bj_unreachable;
};

#ifdef _KERNEL

uint32_t m_xword(const struct mbuf *, uint32_t, int *);
uint32_t m_xhalf(const struct mbuf *, uint32_t, int *);
uint32_t m_xbyte(const struct mbuf *, uint32_t, int *);

MODULE(MODULE_CLASS_MISC, bpfjit, "sljit")

static int
bpfjit_modcmd(modcmd_t cmd, void *arg)
{

	switch (cmd) {
	case MODULE_CMD_INIT:
		bpfjit_module_ops.bj_free_code = &bpfjit_free_code;
		membar_producer();
		bpfjit_module_ops.bj_generate_code = &bpfjit_generate_code;
		membar_producer();
		return 0;

	case MODULE_CMD_FINI:
		return EOPNOTSUPP;

	default:
		return ENOTTY;
	}
}
#endif

static uint32_t
read_width(struct bpf_insn *pc)
{

	switch (BPF_SIZE(pc->code)) {
	case BPF_W:
		return 4;
	case BPF_H:
		return 2;
	case BPF_B:
		return 1;
	default:
		BPFJIT_ASSERT(false);
		return 0;
	}
}

/*
 * Get offset of M[k] on the stack.
 */
static size_t
mem_local_offset(uint32_t k, unsigned int minm)
{
	size_t moff = (k - minm) * sizeof(uint32_t);

#ifdef _KERNEL
	/*
	 * 4 bytes for the third argument of m_xword/m_xhalf/m_xbyte.
	 */
	return sizeof(uint32_t) + moff;
#else
	return moff;
#endif
}

/*
 * Generate code for BPF_LD+BPF_B+BPF_ABS    A <- P[k:1].
 */
static int
emit_read8(struct sljit_compiler* compiler, uint32_t k)
{

	return sljit_emit_op1(compiler,
	    SLJIT_MOV_UB,
	    BPFJIT_A, 0,
	    SLJIT_MEM1(BPFJIT_BUF), k);
}

/*
 * Generate code for BPF_LD+BPF_H+BPF_ABS    A <- P[k:2].
 */
static int
emit_read16(struct sljit_compiler* compiler, uint32_t k)
{
	int status;

	/* tmp1 = buf[k]; */
	status = sljit_emit_op1(compiler,
	    SLJIT_MOV_UB,
	    BPFJIT_TMP1, 0,
	    SLJIT_MEM1(BPFJIT_BUF), k);
	if (status != SLJIT_SUCCESS)
		return status;

	/* A = buf[k+1]; */
	status = sljit_emit_op1(compiler,
	    SLJIT_MOV_UB,
	    BPFJIT_A, 0,
	    SLJIT_MEM1(BPFJIT_BUF), k+1);
	if (status != SLJIT_SUCCESS)
		return status;

	/* tmp1 = tmp1 << 8; */
	status = sljit_emit_op2(compiler,
	    SLJIT_SHL,
	    BPFJIT_TMP1, 0,
	    BPFJIT_TMP1, 0,
	    SLJIT_IMM, 8);
	if (status != SLJIT_SUCCESS)
		return status;

	/* A = A + tmp1; */
	status = sljit_emit_op2(compiler,
	    SLJIT_ADD,
	    BPFJIT_A, 0,
	    BPFJIT_A, 0,
	    BPFJIT_TMP1, 0);
	return status;
}

/*
 * Generate code for BPF_LD+BPF_W+BPF_ABS    A <- P[k:4].
 */
static int
emit_read32(struct sljit_compiler* compiler, uint32_t k)
{
	int status;

	/* tmp1 = buf[k]; */
	status = sljit_emit_op1(compiler,
	    SLJIT_MOV_UB,
	    BPFJIT_TMP1, 0,
	    SLJIT_MEM1(BPFJIT_BUF), k);
	if (status != SLJIT_SUCCESS)
		return status;

	/* tmp2 = buf[k+1]; */
	status = sljit_emit_op1(compiler,
	    SLJIT_MOV_UB,
	    BPFJIT_TMP2, 0,
	    SLJIT_MEM1(BPFJIT_BUF), k+1);
	if (status != SLJIT_SUCCESS)
		return status;

	/* A = buf[k+3]; */
	status = sljit_emit_op1(compiler,
	    SLJIT_MOV_UB,
	    BPFJIT_A, 0,
	    SLJIT_MEM1(BPFJIT_BUF), k+3);
	if (status != SLJIT_SUCCESS)
		return status;

	/* tmp1 = tmp1 << 24; */
	status = sljit_emit_op2(compiler,
	    SLJIT_SHL,
	    BPFJIT_TMP1, 0,
	    BPFJIT_TMP1, 0,
	    SLJIT_IMM, 24);
	if (status != SLJIT_SUCCESS)
		return status;

	/* A = A + tmp1; */
	status = sljit_emit_op2(compiler,
	    SLJIT_ADD,
	    BPFJIT_A, 0,
	    BPFJIT_A, 0,
	    BPFJIT_TMP1, 0);
	if (status != SLJIT_SUCCESS)
		return status;

	/* tmp1 = buf[k+2]; */
	status = sljit_emit_op1(compiler,
	    SLJIT_MOV_UB,
	    BPFJIT_TMP1, 0,
	    SLJIT_MEM1(BPFJIT_BUF), k+2);
	if (status != SLJIT_SUCCESS)
		return status;

	/* tmp2 = tmp2 << 16; */
	status = sljit_emit_op2(compiler,
	    SLJIT_SHL,
	    BPFJIT_TMP2, 0,
	    BPFJIT_TMP2, 0,
	    SLJIT_IMM, 16);
	if (status != SLJIT_SUCCESS)
		return status;

	/* A = A + tmp2; */
	status = sljit_emit_op2(compiler,
	    SLJIT_ADD,
	    BPFJIT_A, 0,
	    BPFJIT_A, 0,
	    BPFJIT_TMP2, 0);
	if (status != SLJIT_SUCCESS)
		return status;

	/* tmp1 = tmp1 << 8; */
	status = sljit_emit_op2(compiler,
	    SLJIT_SHL,
	    BPFJIT_TMP1, 0,
	    BPFJIT_TMP1, 0,
	    SLJIT_IMM, 8);
	if (status != SLJIT_SUCCESS)
		return status;

	/* A = A + tmp1; */
	status = sljit_emit_op2(compiler,
	    SLJIT_ADD,
	    BPFJIT_A, 0,
	    BPFJIT_A, 0,
	    BPFJIT_TMP1, 0);
	return status;
}

#ifdef _KERNEL
/*
 * Generate m_xword/m_xhalf/m_xbyte call.
 *
 * pc is one of:
 * BPF_LD+BPF_W+BPF_ABS    A <- P[k:4]
 * BPF_LD+BPF_H+BPF_ABS    A <- P[k:2]
 * BPF_LD+BPF_B+BPF_ABS    A <- P[k:1]
 * BPF_LD+BPF_W+BPF_IND    A <- P[X+k:4]
 * BPF_LD+BPF_H+BPF_IND    A <- P[X+k:2]
 * BPF_LD+BPF_B+BPF_IND    A <- P[X+k:1]
 * BPF_LDX+BPF_B+BPF_MSH   X <- 4*(P[k:1]&0xf)
 *
 * dst must be BPFJIT_A for BPF_LD instructions and BPFJIT_X
 * or any of BPFJIT_TMP* registrers for BPF_MSH instruction.
 */
static int
emit_xcall(struct sljit_compiler* compiler, struct bpf_insn *pc,
    int dst, sljit_w dstw, struct sljit_jump **ret0_jump,
    uint32_t (*fn)(const struct mbuf *, uint32_t, int *))
{
#if BPFJIT_X != SLJIT_TEMPORARY_EREG1 || \
    BPFJIT_X == SLJIT_RETURN_REG
#error "Not supported assignment of registers."
#endif
	int status;

	/*
	 * The third argument of fn is an address on stack.
	 */
	const int arg3_offset = 0;

	if (BPF_CLASS(pc->code) == BPF_LDX) {
		/* save A */
		status = sljit_emit_op1(compiler,
		    SLJIT_MOV,
		    BPFJIT_KERN_TMP, 0,
		    BPFJIT_A, 0);
		if (status != SLJIT_SUCCESS)
			return status;
	}

	/*
	 * Prepare registers for fn(buf, k, &err) call.
	 */
	status = sljit_emit_op1(compiler,
	    SLJIT_MOV,
	    SLJIT_TEMPORARY_REG1, 0,
	    BPFJIT_BUF, 0);
	if (status != SLJIT_SUCCESS)
		return status;

	if (BPF_CLASS(pc->code) == BPF_LD && BPF_MODE(pc->code) == BPF_IND) {
		status = sljit_emit_op2(compiler,
		    SLJIT_ADD,
		    SLJIT_TEMPORARY_REG2, 0,
		    BPFJIT_X, 0,
		    SLJIT_IMM, (uint32_t)pc->k);
	} else {
		status = sljit_emit_op1(compiler,
		    SLJIT_MOV,
		    SLJIT_TEMPORARY_REG2, 0,
		    SLJIT_IMM, (uint32_t)pc->k);
	}

	if (status != SLJIT_SUCCESS)
		return status;

	status = sljit_get_local_base(compiler,
	    SLJIT_TEMPORARY_REG3, 0, arg3_offset);
	if (status != SLJIT_SUCCESS)
		return status;

	/* fn(buf, k, &err); */
	status = sljit_emit_ijump(compiler,
	    SLJIT_CALL3,
	    SLJIT_IMM, SLJIT_FUNC_OFFSET(fn));

	if (BPF_CLASS(pc->code) == BPF_LDX) {

		/* move return value to dst */
		BPFJIT_ASSERT(dst != SLJIT_RETURN_REG);
		status = sljit_emit_op1(compiler,
		    SLJIT_MOV,
		    dst, dstw,
		    SLJIT_RETURN_REG, 0);
		if (status != SLJIT_SUCCESS)
			return status;

		/* restore A */
		status = sljit_emit_op1(compiler,
		    SLJIT_MOV,
		    BPFJIT_A, 0,
		    BPFJIT_KERN_TMP, 0);
		if (status != SLJIT_SUCCESS)
			return status;

	} else if (dst != SLJIT_RETURN_REG) {
		status = sljit_emit_op1(compiler,
		    SLJIT_MOV,
		    dst, dstw,
		    SLJIT_RETURN_REG, 0);
		if (status != SLJIT_SUCCESS)
			return status;
	}

	/* tmp3 = *err; */
	status = sljit_emit_op1(compiler,
	    SLJIT_MOV_UI,
	    SLJIT_TEMPORARY_REG3, 0,
	    SLJIT_MEM1(SLJIT_LOCALS_REG), arg3_offset);
	if (status != SLJIT_SUCCESS)
		return status;

	/* if (tmp3 != 0) return 0; */
	*ret0_jump = sljit_emit_cmp(compiler,
	    SLJIT_C_NOT_EQUAL,
	    SLJIT_TEMPORARY_REG3, 0,
	    SLJIT_IMM, 0);
	if (*ret0_jump == NULL)
		return SLJIT_ERR_ALLOC_FAILED;

	return status;
}
#endif

/*
 * Generate code for
 * BPF_LD+BPF_W+BPF_ABS    A <- P[k:4]
 * BPF_LD+BPF_H+BPF_ABS    A <- P[k:2]
 * BPF_LD+BPF_B+BPF_ABS    A <- P[k:1]
 * BPF_LD+BPF_W+BPF_IND    A <- P[X+k:4]
 * BPF_LD+BPF_H+BPF_IND    A <- P[X+k:2]
 * BPF_LD+BPF_B+BPF_IND    A <- P[X+k:1]
 */
static int
emit_pkt_read(struct sljit_compiler* compiler,
    struct bpf_insn *pc, struct sljit_jump *to_mchain_jump,
    struct sljit_jump **ret0, size_t *ret0_size)
{
	int status;
	uint32_t width;
	struct sljit_jump *jump;
#ifdef _KERNEL
	struct sljit_label *label;
	struct sljit_jump *over_mchain_jump;
	const bool check_zero_buflen = (to_mchain_jump != NULL);
#endif
	const uint32_t k = pc->k;

#ifdef _KERNEL
	if (to_mchain_jump == NULL) {
		to_mchain_jump = sljit_emit_cmp(compiler,
		    SLJIT_C_EQUAL,
		    BPFJIT_BUFLEN, 0,
		    SLJIT_IMM, 0);
		if (to_mchain_jump == NULL)
  			return SLJIT_ERR_ALLOC_FAILED;
	}
#endif

	width = read_width(pc);

	if (BPF_MODE(pc->code) == BPF_IND) {
		/* tmp1 = buflen - (pc->k + width); */
		status = sljit_emit_op2(compiler,
		    SLJIT_SUB,
		    BPFJIT_TMP1, 0,
		    BPFJIT_BUFLEN, 0,
		    SLJIT_IMM, k + width);
		if (status != SLJIT_SUCCESS)
			return status;

		/* buf += X; */
		status = sljit_emit_op2(compiler,
		    SLJIT_ADD,
		    BPFJIT_BUF, 0,
		    BPFJIT_BUF, 0,
		    BPFJIT_X, 0);
		if (status != SLJIT_SUCCESS)
			return status;

		/* if (tmp1 < X) return 0; */
		jump = sljit_emit_cmp(compiler,
		    SLJIT_C_LESS,
		    BPFJIT_TMP1, 0,
		    BPFJIT_X, 0);
		if (jump == NULL)
  			return SLJIT_ERR_ALLOC_FAILED;
		ret0[(*ret0_size)++] = jump;
	}

	switch (width) {
	case 4:
		status = emit_read32(compiler, k);
		break;
	case 2:
		status = emit_read16(compiler, k);
		break;
	case 1:
		status = emit_read8(compiler, k);
		break;
	}

	if (status != SLJIT_SUCCESS)
		return status;

	if (BPF_MODE(pc->code) == BPF_IND) {
		/* buf -= X; */
		status = sljit_emit_op2(compiler,
		    SLJIT_SUB,
		    BPFJIT_BUF, 0,
		    BPFJIT_BUF, 0,
		    BPFJIT_X, 0);
		if (status != SLJIT_SUCCESS)
			return status;
	}

#ifdef _KERNEL
	over_mchain_jump = sljit_emit_jump(compiler, SLJIT_JUMP);
	if (over_mchain_jump == NULL)
  		return SLJIT_ERR_ALLOC_FAILED;

	/* entry point to mchain handler */
	label = sljit_emit_label(compiler);
	if (label == NULL)
  		return SLJIT_ERR_ALLOC_FAILED;
	sljit_set_label(to_mchain_jump, label);

	if (check_zero_buflen) {
		/* if (buflen != 0) return 0; */
		jump = sljit_emit_cmp(compiler,
		    SLJIT_C_NOT_EQUAL,
		    BPFJIT_BUFLEN, 0,
		    SLJIT_IMM, 0);
		if (jump == NULL)
			return SLJIT_ERR_ALLOC_FAILED;
		ret0[(*ret0_size)++] = jump;
	}

	switch (width) {
	case 4:
		status = emit_xcall(compiler, pc, BPFJIT_A, 0, &jump, &m_xword);
		break;
	case 2:
		status = emit_xcall(compiler, pc, BPFJIT_A, 0, &jump, &m_xhalf);
		break;
	case 1:
		status = emit_xcall(compiler, pc, BPFJIT_A, 0, &jump, &m_xbyte);
		break;
	}

	if (status != SLJIT_SUCCESS)
		return status;

	ret0[(*ret0_size)++] = jump;

	label = sljit_emit_label(compiler);
	if (label == NULL)
		return SLJIT_ERR_ALLOC_FAILED;
	sljit_set_label(over_mchain_jump, label);
#endif

	return status;
}

/*
 * Generate code for BPF_LDX+BPF_B+BPF_MSH    X <- 4*(P[k:1]&0xf).
 */
static int
emit_msh(struct sljit_compiler* compiler,
    struct bpf_insn *pc, struct sljit_jump *to_mchain_jump,
    struct sljit_jump **ret0, size_t *ret0_size)
{
	int status;
#ifdef _KERNEL
	struct sljit_label *label;
	struct sljit_jump *jump, *over_mchain_jump;
	const bool check_zero_buflen = (to_mchain_jump != NULL);
#endif
	const uint32_t k = pc->k;

#ifdef _KERNEL
	if (to_mchain_jump == NULL) {
		to_mchain_jump = sljit_emit_cmp(compiler,
		    SLJIT_C_EQUAL,
		    BPFJIT_BUFLEN, 0,
		    SLJIT_IMM, 0);
		if (to_mchain_jump == NULL)
 			return SLJIT_ERR_ALLOC_FAILED;
	}
#endif

	/* tmp1 = buf[k] */
	status = sljit_emit_op1(compiler,
	    SLJIT_MOV_UB,
	    BPFJIT_TMP1, 0,
	    SLJIT_MEM1(BPFJIT_BUF), k);
	if (status != SLJIT_SUCCESS)
		return status;

	/* tmp1 &= 0xf */
	status = sljit_emit_op2(compiler,
	    SLJIT_AND,
	    BPFJIT_TMP1, 0,
	    BPFJIT_TMP1, 0,
	    SLJIT_IMM, 0xf);
	if (status != SLJIT_SUCCESS)
		return status;

	/* tmp1 = tmp1 << 2 */
	status = sljit_emit_op2(compiler,
	    SLJIT_SHL,
	    BPFJIT_X, 0,
	    BPFJIT_TMP1, 0,
	    SLJIT_IMM, 2);
	if (status != SLJIT_SUCCESS)
		return status;

#ifdef _KERNEL
	over_mchain_jump = sljit_emit_jump(compiler, SLJIT_JUMP);
	if (over_mchain_jump == NULL)
		return SLJIT_ERR_ALLOC_FAILED;

	/* entry point to mchain handler */
	label = sljit_emit_label(compiler);
	if (label == NULL)
		return SLJIT_ERR_ALLOC_FAILED;
	sljit_set_label(to_mchain_jump, label);

	if (check_zero_buflen) {
		/* if (buflen != 0) return 0; */
		jump = sljit_emit_cmp(compiler,
		    SLJIT_C_NOT_EQUAL,
		    BPFJIT_BUFLEN, 0,
		    SLJIT_IMM, 0);
		if (jump == NULL)
  			return SLJIT_ERR_ALLOC_FAILED;
		ret0[(*ret0_size)++] = jump;
	}

	status = emit_xcall(compiler, pc, BPFJIT_TMP1, 0, &jump, &m_xbyte);
	if (status != SLJIT_SUCCESS)
		return status;
	ret0[(*ret0_size)++] = jump;

	/* tmp1 &= 0xf */
	status = sljit_emit_op2(compiler,
	    SLJIT_AND,
	    BPFJIT_TMP1, 0,
	    BPFJIT_TMP1, 0,
	    SLJIT_IMM, 0xf);
	if (status != SLJIT_SUCCESS)
		return status;

	/* tmp1 = tmp1 << 2 */
	status = sljit_emit_op2(compiler,
	    SLJIT_SHL,
	    BPFJIT_X, 0,
	    BPFJIT_TMP1, 0,
	    SLJIT_IMM, 2);
	if (status != SLJIT_SUCCESS)
		return status;


	label = sljit_emit_label(compiler);
	if (label == NULL)
		return SLJIT_ERR_ALLOC_FAILED;
	sljit_set_label(over_mchain_jump, label);
#endif

	return status;
}

static int
emit_pow2_division(struct sljit_compiler* compiler, uint32_t k)
{
	int shift = 0;
	int status = SLJIT_SUCCESS;

	while (k > 1) {
		k >>= 1;
		shift++;
	}

	BPFJIT_ASSERT(k == 1 && shift < 32);

	if (shift != 0) {
		status = sljit_emit_op2(compiler,
		    SLJIT_LSHR|SLJIT_INT_OP,
		    BPFJIT_A, 0,
		    BPFJIT_A, 0,
		    SLJIT_IMM, shift);
	}

	return status;
}

#if !defined(BPFJIT_USE_UDIV)
static sljit_uw
divide(sljit_uw x, sljit_uw y)
{

	return (uint32_t)x / (uint32_t)y;
}
#endif

/*
 * Generate A = A / div.
 * divt,divw are either SLJIT_IMM,pc->k or BPFJIT_X,0.
 */
static int
emit_division(struct sljit_compiler* compiler, int divt, sljit_w divw)
{
	int status;

#if BPFJIT_X == SLJIT_TEMPORARY_REG1 || \
    BPFJIT_X == SLJIT_RETURN_REG     || \
    BPFJIT_X == SLJIT_TEMPORARY_REG2 || \
    BPFJIT_A == SLJIT_TEMPORARY_REG2
#error "Not supported assignment of registers."
#endif

#if BPFJIT_A != SLJIT_TEMPORARY_REG1
	status = sljit_emit_op1(compiler,
	    SLJIT_MOV,
	    SLJIT_TEMPORARY_REG1, 0,
	    BPFJIT_A, 0);
	if (status != SLJIT_SUCCESS)
		return status;
#endif

	status = sljit_emit_op1(compiler,
	    SLJIT_MOV,
	    SLJIT_TEMPORARY_REG2, 0,
	    divt, divw);
	if (status != SLJIT_SUCCESS)
		return status;

#if defined(BPFJIT_USE_UDIV)
	status = sljit_emit_op0(compiler, SLJIT_UDIV|SLJIT_INT_OP);

#if BPFJIT_A != SLJIT_TEMPORARY_REG1
	status = sljit_emit_op1(compiler,
	    SLJIT_MOV,
	    BPFJIT_A, 0,
	    SLJIT_TEMPORARY_REG1, 0);
	if (status != SLJIT_SUCCESS)
		return status;
#endif
#else
	status = sljit_emit_ijump(compiler,
	    SLJIT_CALL2,
	    SLJIT_IMM, SLJIT_FUNC_OFFSET(divide));

#if BPFJIT_A != SLJIT_RETURN_REG
	status = sljit_emit_op1(compiler,
	    SLJIT_MOV,
	    BPFJIT_A, 0,
	    SLJIT_RETURN_REG, 0);
	if (status != SLJIT_SUCCESS)
		return status;
#endif
#endif

	return status;
}

/*
 * Count BPF_RET instructions.
 */
static size_t
count_returns(struct bpf_insn *insns, size_t insn_count)
{
	size_t i;
	size_t rv;

	rv = 0;
	for (i = 0; i < insn_count; i++) {
		if (BPF_CLASS(insns[i].code) == BPF_RET)
			rv++;
	}

	return rv;
}

/*
 * Return true if pc is a "read from packet" instruction.
 * If length is not NULL and return value is true, *length will
 * be set to a safe length required to read a packet.
 */
static bool
read_pkt_insn(struct bpf_insn *pc, uint32_t *length)
{
	bool rv;
	uint32_t width;

	switch (BPF_CLASS(pc->code)) {
	default:
		rv = false;
		break;

	case BPF_LD:
		rv = BPF_MODE(pc->code) == BPF_ABS ||
		     BPF_MODE(pc->code) == BPF_IND;
		if (rv)
			width = read_width(pc);
		break;

	case BPF_LDX:
		rv = pc->code == (BPF_LDX|BPF_B|BPF_MSH);
		width = 1;
		break;
	}

	if (rv && length != NULL) {
		*length = (pc->k > UINT32_MAX - width) ?
		    UINT32_MAX : pc->k + width;
	}

	return rv;
}

/*
 * Set bj_check_length for all "read from packet" instructions
 * in a linear block of instructions [from, to).
 */
static void
set_check_length(struct bpf_insn *insns, struct bpfjit_insn_data *insn_dat,
    size_t from, size_t to, uint32_t length)
{

	for (; from < to; from++) {
		if (read_pkt_insn(&insns[from], NULL)) {
			insn_dat[from].bj_aux.bj_rdata.bj_check_length = length;
			length = 0;
		}
	}
}

/*
 * The function divides instructions into blocks. Destination of a jump
 * instruction starts a new block. BPF_RET and BPF_JMP instructions
 * terminate a block. Blocks are linear, that is, there are no jumps out
 * from the middle of a block and there are no jumps in to the middle of
 * a block.
 * If a block has one or more "read from packet" instructions,
 * bj_check_length will be set to one value for the whole block and that
 * value will be equal to the greatest value of safe lengths of "read from
 * packet" instructions inside the block.
 */
static int
optimize(struct bpf_insn *insns,
    struct bpfjit_insn_data *insn_dat, size_t insn_count)
{
	size_t i;
	size_t first_read;
	bool unreachable;
	uint32_t jt, jf;
	uint32_t length, safe_length;
	struct bpfjit_jump *jmp, *jtf;

	for (i = 0; i < insn_count; i++)
		SLIST_INIT(&insn_dat[i].bj_jumps);

	safe_length = 0;
	unreachable = false;
	first_read = SIZE_MAX;

	for (i = 0; i < insn_count; i++) {

		if (!SLIST_EMPTY(&insn_dat[i].bj_jumps)) {
			unreachable = false;

			set_check_length(insns, insn_dat,
			    first_read, i, safe_length);
			first_read = SIZE_MAX;

			safe_length = UINT32_MAX;
			SLIST_FOREACH(jmp, &insn_dat[i].bj_jumps, bj_entries) {
				if (jmp->bj_safe_length < safe_length)
					safe_length = jmp->bj_safe_length;
			}
		}

		insn_dat[i].bj_unreachable = unreachable;
		if (unreachable)
			continue;

		if (read_pkt_insn(&insns[i], &length)) {
			if (first_read == SIZE_MAX)
				first_read = i;
			if (length > safe_length)
				safe_length = length;
		}

		switch (BPF_CLASS(insns[i].code)) {
		case BPF_RET:
			unreachable = true;
			continue;

		case BPF_JMP:
			if (insns[i].code == (BPF_JMP|BPF_JA)) {
				jt = jf = insns[i].k;
			} else {
				jt = insns[i].jt;
				jf = insns[i].jf;
			}

			if (jt >= insn_count - (i + 1) ||
			    jf >= insn_count - (i + 1)) {
				return -1;
			}

			if (jt > 0 && jf > 0)
				unreachable = true;

			jtf = insn_dat[i].bj_aux.bj_jdata.bj_jtf;

			jtf[0].bj_jump = NULL;
			jtf[0].bj_safe_length = safe_length;
			SLIST_INSERT_HEAD(&insn_dat[i + 1 + jt].bj_jumps,
			    &jtf[0], bj_entries);

			if (jf != jt) {
				jtf[1].bj_jump = NULL;
				jtf[1].bj_safe_length = safe_length;
				SLIST_INSERT_HEAD(&insn_dat[i + 1 + jf].bj_jumps,
				    &jtf[1], bj_entries);
			}

			continue;
		}
	}

	set_check_length(insns, insn_dat, first_read, insn_count, safe_length);

	return 0;
}

/*
 * Count out-of-bounds and division by zero jumps.
 *
 * insn_dat should be initialized by optimize().
 */
static size_t
get_ret0_size(struct bpf_insn *insns, struct bpfjit_insn_data *insn_dat,
    size_t insn_count)
{
	size_t rv = 0;
	size_t i;

	for (i = 0; i < insn_count; i++) {

		if (read_pkt_insn(&insns[i], NULL)) {
			if (insn_dat[i].bj_aux.bj_rdata.bj_check_length > 0)
				rv++;
#ifdef _KERNEL
			rv++;
#endif
		}

		if (insns[i].code == (BPF_LD|BPF_IND|BPF_B) ||
		    insns[i].code == (BPF_LD|BPF_IND|BPF_H) ||
		    insns[i].code == (BPF_LD|BPF_IND|BPF_W)) {
			rv++;
		}

		if (insns[i].code == (BPF_ALU|BPF_DIV|BPF_X))
			rv++;

		if (insns[i].code == (BPF_ALU|BPF_DIV|BPF_K) &&
		    insns[i].k == 0) {
			rv++;
		}
	}

	return rv;
}

/*
 * Convert BPF_ALU operations except BPF_NEG and BPF_DIV to sljit operation.
 */
static int
bpf_alu_to_sljit_op(struct bpf_insn *pc)
{

	/*
	 * Note: all supported 64bit arches have 32bit multiply
	 * instruction so SLJIT_INT_OP doesn't have any overhead.
	 */
	switch (BPF_OP(pc->code)) {
	case BPF_ADD: return SLJIT_ADD;
	case BPF_SUB: return SLJIT_SUB;
	case BPF_MUL: return SLJIT_MUL|SLJIT_INT_OP;
	case BPF_OR:  return SLJIT_OR;
	case BPF_AND: return SLJIT_AND;
	case BPF_LSH: return SLJIT_SHL;
	case BPF_RSH: return SLJIT_LSHR|SLJIT_INT_OP;
	default:
		BPFJIT_ASSERT(false);
		return 0;
	}
}

/*
 * Convert BPF_JMP operations except BPF_JA to sljit condition.
 */
static int
bpf_jmp_to_sljit_cond(struct bpf_insn *pc, bool negate)
{
	/*
	 * Note: all supported 64bit arches have 32bit comparison
	 * instructions so SLJIT_INT_OP doesn't have any overhead.
	 */
	int rv = SLJIT_INT_OP;

	switch (BPF_OP(pc->code)) {
	case BPF_JGT:
		rv |= negate ? SLJIT_C_LESS_EQUAL : SLJIT_C_GREATER;
		break;
	case BPF_JGE:
		rv |= negate ? SLJIT_C_LESS : SLJIT_C_GREATER_EQUAL;
		break;
	case BPF_JEQ:
		rv |= negate ? SLJIT_C_NOT_EQUAL : SLJIT_C_EQUAL;
		break;
	case BPF_JSET:
		rv |= negate ? SLJIT_C_EQUAL : SLJIT_C_NOT_EQUAL;
		break;
	default:
		BPFJIT_ASSERT(false);
	}

	return rv;
}

static unsigned int
bpfjit_optimization_hints(struct bpf_insn *insns, size_t insn_count)
{
	unsigned int rv = BPFJIT_INIT_A;
	struct bpf_insn *pc;
	unsigned int minm, maxm;

	BPFJIT_ASSERT(BPF_MEMWORDS - 1 <= 0xff);

	maxm = 0;
	minm = BPF_MEMWORDS - 1;

	for (pc = insns; pc != insns + insn_count; pc++) {
		switch (BPF_CLASS(pc->code)) {
		case BPF_LD:
			if (BPF_MODE(pc->code) == BPF_IND)
				rv |= BPFJIT_INIT_X;
			if (BPF_MODE(pc->code) == BPF_MEM &&
			    (uint32_t)pc->k < BPF_MEMWORDS) {
				if (pc->k > maxm)
					maxm = pc->k;
				if (pc->k < minm)
					minm = pc->k;
			}
			continue;
		case BPF_LDX:
			rv |= BPFJIT_INIT_X;
			if (BPF_MODE(pc->code) == BPF_MEM &&
			    (uint32_t)pc->k < BPF_MEMWORDS) {
				if (pc->k > maxm)
					maxm = pc->k;
				if (pc->k < minm)
					minm = pc->k;
			}
			continue;
		case BPF_ST:
			if ((uint32_t)pc->k < BPF_MEMWORDS) {
				if (pc->k > maxm)
					maxm = pc->k;
				if (pc->k < minm)
					minm = pc->k;
			}
			continue;
		case BPF_STX:
			rv |= BPFJIT_INIT_X;
			if ((uint32_t)pc->k < BPF_MEMWORDS) {
				if (pc->k > maxm)
					maxm = pc->k;
				if (pc->k < minm)
					minm = pc->k;
			}
			continue;
		case BPF_ALU:
			if (pc->code == (BPF_ALU|BPF_NEG))
				continue;
			if (BPF_SRC(pc->code) == BPF_X)
				rv |= BPFJIT_INIT_X;
			continue;
		case BPF_JMP:
			if (pc->code == (BPF_JMP|BPF_JA))
				continue;
			if (BPF_SRC(pc->code) == BPF_X)
				rv |= BPFJIT_INIT_X;
			continue;
		case BPF_RET:
			continue;
		case BPF_MISC:
			rv |= BPFJIT_INIT_X;
			continue;
		default:
			BPFJIT_ASSERT(false);
		}
	}

	return rv | (maxm << 8) | minm;
}

/*
 * Convert BPF_K and BPF_X to sljit register.
 */
static int
kx_to_reg(struct bpf_insn *pc)
{

	switch (BPF_SRC(pc->code)) {
	case BPF_K: return SLJIT_IMM;
	case BPF_X: return BPFJIT_X;
	default:
		BPFJIT_ASSERT(false);
		return 0;
	}
}

static sljit_w
kx_to_reg_arg(struct bpf_insn *pc)
{

	switch (BPF_SRC(pc->code)) {
	case BPF_K: return (uint32_t)pc->k; /* SLJIT_IMM, pc->k, */
	case BPF_X: return 0;               /* BPFJIT_X, 0,      */
	default:
		BPFJIT_ASSERT(false);
		return 0;
	}
}

bpfjit_function_t
bpfjit_generate_code(struct bpf_insn *insns, size_t insn_count)
{
	void *rv;
	size_t i;
	int status;
	int branching, negate;
	unsigned int rval, mode, src;
	int ntmp;
	unsigned int locals_size;
	unsigned int minm, maxm; /* min/max k for M[k] */
	size_t mem_locals_start; /* start of M[] array */
	unsigned int opts;
	struct bpf_insn *pc;
	struct sljit_compiler* compiler;

	/* a list of jumps to a normal return from a generated function */
	struct sljit_jump **returns;
	size_t returns_size, returns_maxsize;

	/* a list of jumps to out-of-bound return from a generated function */
	struct sljit_jump **ret0;
	size_t ret0_size, ret0_maxsize;

	struct bpfjit_insn_data *insn_dat;

	/* for local use */
	struct sljit_label *label;
	struct sljit_jump *jump;
	struct bpfjit_jump *bjump, *jtf;

	struct sljit_jump *to_mchain_jump;

	uint32_t jt, jf;

	rv = NULL;
	compiler = NULL;
	insn_dat = NULL;
	returns = NULL;
	ret0 = NULL;

	opts = bpfjit_optimization_hints(insns, insn_count);
	minm = opts & 0xff;
	maxm = (opts >> 8) & 0xff;
	mem_locals_start = mem_local_offset(0, 0);
	locals_size = (minm <= maxm) ?
	    mem_local_offset(maxm + 1, minm) : mem_locals_start;

	ntmp = 4;
#ifdef _KERNEL
	ntmp += 1; /* for BPFJIT_KERN_TMP */
#endif

	returns_maxsize = count_returns(insns, insn_count);
	if (returns_maxsize  == 0)
		goto fail;

	insn_dat = BPFJIT_MALLOC(insn_count * sizeof(insn_dat[0]));
	if (insn_dat == NULL)
		goto fail;

	if (optimize(insns, insn_dat, insn_count) < 0)
		goto fail;

	ret0_size = 0;
	ret0_maxsize = get_ret0_size(insns, insn_dat, insn_count);
	if (ret0_maxsize > 0) {
		ret0 = BPFJIT_MALLOC(ret0_maxsize * sizeof(ret0[0]));
		if (ret0 == NULL)
			goto fail;
	}

	returns_size = 0;
	returns = BPFJIT_MALLOC(returns_maxsize * sizeof(returns[0]));
	if (returns == NULL)
		goto fail;

	compiler = sljit_create_compiler();
	if (compiler == NULL)
		goto fail;

#if !defined(_KERNEL) && defined(SLJIT_VERBOSE) && SLJIT_VERBOSE
	sljit_compiler_verbose(compiler, stderr);
#endif

	status = sljit_emit_enter(compiler, 3, ntmp, 3, locals_size);
	if (status != SLJIT_SUCCESS)
		goto fail;

	for (i = mem_locals_start; i < locals_size; i+= sizeof(uint32_t)) {
		status = sljit_emit_op1(compiler,
		    SLJIT_MOV_UI,
		    SLJIT_MEM1(SLJIT_LOCALS_REG), i,
		    SLJIT_IMM, 0);
		if (status != SLJIT_SUCCESS)
			goto fail;
	}

	if (opts & BPFJIT_INIT_A) {
		/* A = 0; */
		status = sljit_emit_op1(compiler,
		    SLJIT_MOV,
		    BPFJIT_A, 0,
		    SLJIT_IMM, 0);
		if (status != SLJIT_SUCCESS)
			goto fail;
	}

	if (opts & BPFJIT_INIT_X) {
		/* X = 0; */
		status = sljit_emit_op1(compiler,
		    SLJIT_MOV,
		    BPFJIT_X, 0,
		    SLJIT_IMM, 0);
		if (status != SLJIT_SUCCESS)
			goto fail;
	}

	for (i = 0; i < insn_count; i++) {
		if (insn_dat[i].bj_unreachable)
			continue;

		to_mchain_jump = NULL;

		/*
		 * Resolve jumps to the current insn.
		 */
		label = NULL;
		SLIST_FOREACH(bjump, &insn_dat[i].bj_jumps, bj_entries) {
			if (bjump->bj_jump != NULL) {
				if (label == NULL)
					label = sljit_emit_label(compiler);
				if (label == NULL)
					goto fail;
				sljit_set_label(bjump->bj_jump, label);
			}
		}

		if (read_pkt_insn(&insns[i], NULL) &&
		    insn_dat[i].bj_aux.bj_rdata.bj_check_length > 0) {
			/* if (buflen < bj_check_length) return 0; */
			jump = sljit_emit_cmp(compiler,
			    SLJIT_C_LESS,
			    BPFJIT_BUFLEN, 0,
			    SLJIT_IMM,
			    insn_dat[i].bj_aux.bj_rdata.bj_check_length);
			if (jump == NULL)
		  		goto fail;
#ifdef _KERNEL
			to_mchain_jump = jump;
#else
			ret0[ret0_size++] = jump;
#endif
		}

		pc = &insns[i];
		switch (BPF_CLASS(pc->code)) {

		default:
			goto fail;

		case BPF_LD:
			/* BPF_LD+BPF_IMM          A <- k */
			if (pc->code == (BPF_LD|BPF_IMM)) {
				status = sljit_emit_op1(compiler,
				    SLJIT_MOV,
				    BPFJIT_A, 0,
				    SLJIT_IMM, (uint32_t)pc->k);
				if (status != SLJIT_SUCCESS)
					goto fail;

				continue;
			}

			/* BPF_LD+BPF_MEM          A <- M[k] */
			if (pc->code == (BPF_LD|BPF_MEM)) {
				if (pc->k < minm || pc->k > maxm)
					goto fail;
				status = sljit_emit_op1(compiler,
				    SLJIT_MOV_UI,
				    BPFJIT_A, 0,
				    SLJIT_MEM1(SLJIT_LOCALS_REG),
				    mem_local_offset(pc->k, minm));
				if (status != SLJIT_SUCCESS)
					goto fail;

				continue;
			}

			/* BPF_LD+BPF_W+BPF_LEN    A <- len */
			if (pc->code == (BPF_LD|BPF_W|BPF_LEN)) {
				status = sljit_emit_op1(compiler,
				    SLJIT_MOV,
				    BPFJIT_A, 0,
				    BPFJIT_WIRELEN, 0);
				if (status != SLJIT_SUCCESS)
					goto fail;

				continue;
			}

			mode = BPF_MODE(pc->code);
			if (mode != BPF_ABS && mode != BPF_IND)
				goto fail;

			status = emit_pkt_read(compiler, pc,
			    to_mchain_jump, ret0, &ret0_size);
			if (status != SLJIT_SUCCESS)
				goto fail;

			continue;

		case BPF_LDX:
			mode = BPF_MODE(pc->code);

			/* BPF_LDX+BPF_W+BPF_IMM    X <- k */
			if (mode == BPF_IMM) {
				if (BPF_SIZE(pc->code) != BPF_W)
					goto fail;
				status = sljit_emit_op1(compiler,
				    SLJIT_MOV,
				    BPFJIT_X, 0,
				    SLJIT_IMM, (uint32_t)pc->k);
				if (status != SLJIT_SUCCESS)
					goto fail;

				continue;
			}

			/* BPF_LDX+BPF_W+BPF_LEN    X <- len */
			if (mode == BPF_LEN) {
				if (BPF_SIZE(pc->code) != BPF_W)
					goto fail;
				status = sljit_emit_op1(compiler,
				    SLJIT_MOV,
				    BPFJIT_X, 0,
				    BPFJIT_WIRELEN, 0);
				if (status != SLJIT_SUCCESS)
					goto fail;

				continue;
			}

			/* BPF_LDX+BPF_W+BPF_MEM    X <- M[k] */
			if (mode == BPF_MEM) {
				if (BPF_SIZE(pc->code) != BPF_W)
					goto fail;
				if (pc->k < minm || pc->k > maxm)
					goto fail;
				status = sljit_emit_op1(compiler,
				    SLJIT_MOV_UI,
				    BPFJIT_X, 0,
				    SLJIT_MEM1(SLJIT_LOCALS_REG),
				    mem_local_offset(pc->k, minm));
				if (status != SLJIT_SUCCESS)
					goto fail;

				continue;
			}

			/* BPF_LDX+BPF_B+BPF_MSH    X <- 4*(P[k:1]&0xf) */
			if (mode != BPF_MSH || BPF_SIZE(pc->code) != BPF_B)
				goto fail;

			status = emit_msh(compiler, pc,
			    to_mchain_jump, ret0, &ret0_size);
			if (status != SLJIT_SUCCESS)
				goto fail;

			continue;

		case BPF_ST:
			if (pc->code != BPF_ST || pc->k < minm || pc->k > maxm)
				goto fail;

			status = sljit_emit_op1(compiler,
			    SLJIT_MOV_UI,
			    SLJIT_MEM1(SLJIT_LOCALS_REG),
			    mem_local_offset(pc->k, minm),
			    BPFJIT_A, 0);
			if (status != SLJIT_SUCCESS)
				goto fail;

			continue;

		case BPF_STX:
			if (pc->code != BPF_STX || pc->k < minm || pc->k > maxm)
				goto fail;

			status = sljit_emit_op1(compiler,
			    SLJIT_MOV_UI,
			    SLJIT_MEM1(SLJIT_LOCALS_REG),
			    mem_local_offset(pc->k, minm),
			    BPFJIT_X, 0);
			if (status != SLJIT_SUCCESS)
				goto fail;

			continue;

		case BPF_ALU:

			if (pc->code == (BPF_ALU|BPF_NEG)) {
				status = sljit_emit_op1(compiler,
				    SLJIT_NEG,
				    BPFJIT_A, 0,
				    BPFJIT_A, 0);
				if (status != SLJIT_SUCCESS)
					goto fail;

				continue;
			}

			if (BPF_OP(pc->code) != BPF_DIV) {
				status = sljit_emit_op2(compiler,
				    bpf_alu_to_sljit_op(pc),
				    BPFJIT_A, 0,
				    BPFJIT_A, 0,
				    kx_to_reg(pc), kx_to_reg_arg(pc));
				if (status != SLJIT_SUCCESS)
					goto fail;

				continue;
			}

			/* BPF_DIV */

			src = BPF_SRC(pc->code);
			if (src != BPF_X && src != BPF_K)
				goto fail;

			/* division by zero? */
			if (src == BPF_X) {
				jump = sljit_emit_cmp(compiler,
				    SLJIT_C_EQUAL|SLJIT_INT_OP,
				    BPFJIT_X, 0, 
				    SLJIT_IMM, 0);
				if (jump == NULL)
					goto fail;
				ret0[ret0_size++] = jump;
			} else if (pc->k == 0) {
				jump = sljit_emit_jump(compiler, SLJIT_JUMP);
				if (jump == NULL)
					goto fail;
				ret0[ret0_size++] = jump;
			}

			if (src == BPF_X) {
				status = emit_division(compiler, BPFJIT_X, 0);
				if (status != SLJIT_SUCCESS)
					goto fail;
			} else if (pc->k != 0) {
				if (pc->k & (pc->k - 1)) {
				    status = emit_division(compiler,
				        SLJIT_IMM, (uint32_t)pc->k);
				} else {
    				    status = emit_pow2_division(compiler,
				        (uint32_t)pc->k);
				}
				if (status != SLJIT_SUCCESS)
					goto fail;
			}

			continue;

		case BPF_JMP:

			if (pc->code == (BPF_JMP|BPF_JA)) {
				jt = jf = pc->k;
			} else {
				jt = pc->jt;
				jf = pc->jf;
			}

			negate = (jt == 0) ? 1 : 0;
			branching = (jt == jf) ? 0 : 1;
			jtf = insn_dat[i].bj_aux.bj_jdata.bj_jtf;

			if (branching) {
				if (BPF_OP(pc->code) != BPF_JSET) {
					jump = sljit_emit_cmp(compiler,
					    bpf_jmp_to_sljit_cond(pc, negate),
					    BPFJIT_A, 0,
					    kx_to_reg(pc), kx_to_reg_arg(pc));
				} else {
					status = sljit_emit_op2(compiler,
					    SLJIT_AND,
					    BPFJIT_TMP1, 0,
					    BPFJIT_A, 0,
					    kx_to_reg(pc), kx_to_reg_arg(pc));
					if (status != SLJIT_SUCCESS)
						goto fail;

					jump = sljit_emit_cmp(compiler,
					    bpf_jmp_to_sljit_cond(pc, negate),
					    BPFJIT_TMP1, 0,
					    SLJIT_IMM, 0);
				}

				if (jump == NULL)
					goto fail;

				BPFJIT_ASSERT(jtf[negate].bj_jump == NULL);
				jtf[negate].bj_jump = jump;
			}

			if (!branching || (jt != 0 && jf != 0)) {
				jump = sljit_emit_jump(compiler, SLJIT_JUMP);
				if (jump == NULL)
					goto fail;

				BPFJIT_ASSERT(jtf[branching].bj_jump == NULL);
				jtf[branching].bj_jump = jump;
			}

			continue;

		case BPF_RET:

			rval = BPF_RVAL(pc->code);
			if (rval == BPF_X)
				goto fail;

			/* BPF_RET+BPF_K    accept k bytes */
			if (rval == BPF_K) {
				status = sljit_emit_op1(compiler,
				    SLJIT_MOV,
				    BPFJIT_A, 0,
				    SLJIT_IMM, (uint32_t)pc->k);
				if (status != SLJIT_SUCCESS)
					goto fail;
			}

			/* BPF_RET+BPF_A    accept A bytes */
			if (rval == BPF_A) {
#if BPFJIT_A != SLJIT_RETURN_REG
				status = sljit_emit_op1(compiler,
				    SLJIT_MOV,
				    SLJIT_RETURN_REG, 0,
				    BPFJIT_A, 0);
				if (status != SLJIT_SUCCESS)
					goto fail;
#endif
			}

			/*
			 * Save a jump to a normal return. If the program
			 * ends with BPF_RET, no jump is needed because
			 * the normal return is generated right after the
			 * last instruction.
			 */
			if (i != insn_count - 1) {
				jump = sljit_emit_jump(compiler, SLJIT_JUMP);
				if (jump == NULL)
					goto fail;
				returns[returns_size++] = jump;
			}

			continue;

		case BPF_MISC:

			if (pc->code == (BPF_MISC|BPF_TAX)) {
				status = sljit_emit_op1(compiler,
				    SLJIT_MOV_UI,
				    BPFJIT_X, 0,
				    BPFJIT_A, 0);
				if (status != SLJIT_SUCCESS)
					goto fail;

				continue;
			}

			if (pc->code == (BPF_MISC|BPF_TXA)) {
				status = sljit_emit_op1(compiler,
				    SLJIT_MOV,
				    BPFJIT_A, 0,
				    BPFJIT_X, 0);
				if (status != SLJIT_SUCCESS)
					goto fail;

				continue;
			}

			goto fail;
		} /* switch */
	} /* main loop */

	BPFJIT_ASSERT(ret0_size == ret0_maxsize);
	BPFJIT_ASSERT(returns_size <= returns_maxsize);

	if (returns_size > 0) {
		label = sljit_emit_label(compiler);
		if (label == NULL)
			goto fail;
		for (i = 0; i < returns_size; i++)
			sljit_set_label(returns[i], label);
	}

	status = sljit_emit_return(compiler,
	    SLJIT_MOV_UI,
	    BPFJIT_A, 0);
	if (status != SLJIT_SUCCESS)
		goto fail;

	if (ret0_size > 0) {
		label = sljit_emit_label(compiler);
		if (label == NULL)
			goto fail;

		for (i = 0; i < ret0_size; i++)
			sljit_set_label(ret0[i], label);

		status = sljit_emit_op1(compiler,
		    SLJIT_MOV,
		    SLJIT_RETURN_REG, 0,
		    SLJIT_IMM, 0);
		if (status != SLJIT_SUCCESS)
			goto fail;

		status = sljit_emit_return(compiler,
		    SLJIT_MOV_UI,
		    SLJIT_RETURN_REG, 0);
		if (status != SLJIT_SUCCESS)
			goto fail;
	}

	rv = sljit_generate_code(compiler);

fail:
	if (compiler != NULL)
		sljit_free_compiler(compiler);

	if (insn_dat != NULL)
		BPFJIT_FREE(insn_dat);

	if (returns != NULL)
		BPFJIT_FREE(returns);

	if (ret0 != NULL)
		BPFJIT_FREE(ret0);

	return (bpfjit_function_t)rv;
}

void
bpfjit_free_code(bpfjit_function_t code)
{

	sljit_free_code((void *)code);
}
