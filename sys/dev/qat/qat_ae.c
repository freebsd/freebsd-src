/* SPDX-License-Identifier: BSD-2-Clause-NetBSD AND BSD-3-Clause */
/*	$NetBSD: qat_ae.c,v 1.1 2019/11/20 09:37:46 hikaru Exp $	*/

/*
 * Copyright (c) 2019 Internet Initiative Japan, Inc.
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
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/*
 *   Copyright(c) 2007-2019 Intel Corporation. All rights reserved.
 *
 *   Redistribution and use in source and binary forms, with or without
 *   modification, are permitted provided that the following conditions
 *   are met:
 *
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in
 *       the documentation and/or other materials provided with the
 *       distribution.
 *     * Neither the name of Intel Corporation nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 *   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *   "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 *   A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 *   OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 *   SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 *   LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 *   DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 *   THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 *   (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 *   OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");
#if 0
__KERNEL_RCSID(0, "$NetBSD: qat_ae.c,v 1.1 2019/11/20 09:37:46 hikaru Exp $");
#endif

#include <sys/param.h>
#include <sys/bus.h>
#include <sys/firmware.h>
#include <sys/limits.h>
#include <sys/systm.h>

#include <machine/bus.h>

#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>

#include "qatreg.h"
#include "qatvar.h"
#include "qat_aevar.h"

static int	qat_ae_write_4(struct qat_softc *, u_char, bus_size_t,
		    uint32_t);
static int	qat_ae_read_4(struct qat_softc *, u_char, bus_size_t,
		    uint32_t *);
static void	qat_ae_ctx_indr_write(struct qat_softc *, u_char, uint32_t,
		    bus_size_t, uint32_t);
static int	qat_ae_ctx_indr_read(struct qat_softc *, u_char, uint32_t,
		    bus_size_t, uint32_t *);

static u_short	qat_aereg_get_10bit_addr(enum aereg_type, u_short);
static int	qat_aereg_rel_data_write(struct qat_softc *, u_char, u_char,
		    enum aereg_type, u_short, uint32_t);
static int	qat_aereg_rel_data_read(struct qat_softc *, u_char, u_char,
		    enum aereg_type, u_short, uint32_t *);
static int	qat_aereg_rel_rdxfer_write(struct qat_softc *, u_char, u_char,
		    enum aereg_type, u_short, uint32_t);
static int	qat_aereg_rel_wrxfer_write(struct qat_softc *, u_char, u_char,
		    enum aereg_type, u_short, uint32_t);
static int	qat_aereg_rel_nn_write(struct qat_softc *, u_char, u_char,
		    enum aereg_type, u_short, uint32_t);
static int	qat_aereg_abs_to_rel(struct qat_softc *, u_char, u_short,
		    u_short *, u_char *);
static int	qat_aereg_abs_data_write(struct qat_softc *, u_char,
		    enum aereg_type, u_short, uint32_t);

static void	qat_ae_enable_ctx(struct qat_softc *, u_char, u_int);
static void	qat_ae_disable_ctx(struct qat_softc *, u_char, u_int);
static void	qat_ae_write_ctx_mode(struct qat_softc *, u_char, u_char);
static void	qat_ae_write_nn_mode(struct qat_softc *, u_char, u_char);
static void	qat_ae_write_lm_mode(struct qat_softc *, u_char,
		    enum aereg_type, u_char);
static void	qat_ae_write_shared_cs_mode0(struct qat_softc *, u_char,
		    u_char);
static void	qat_ae_write_shared_cs_mode(struct qat_softc *, u_char, u_char);
static int	qat_ae_set_reload_ustore(struct qat_softc *, u_char, u_int, int,
		    u_int);

static enum qat_ae_status qat_ae_get_status(struct qat_softc *, u_char);
static int	qat_ae_is_active(struct qat_softc *, u_char);
static int	qat_ae_wait_num_cycles(struct qat_softc *, u_char, int, int);

static int	qat_ae_clear_reset(struct qat_softc *);
static int	qat_ae_check(struct qat_softc *);
static int	qat_ae_reset_timestamp(struct qat_softc *);
static void	qat_ae_clear_xfer(struct qat_softc *);
static int	qat_ae_clear_gprs(struct qat_softc *);

static void	qat_ae_get_shared_ustore_ae(u_char, u_char *);
static u_int	qat_ae_ucode_parity64(uint64_t);
static uint64_t	qat_ae_ucode_set_ecc(uint64_t);
static int	qat_ae_ucode_write(struct qat_softc *, u_char, u_int, u_int,
		    const uint64_t *);
static int	qat_ae_ucode_read(struct qat_softc *, u_char, u_int, u_int,
		    uint64_t *);
static u_int	qat_ae_concat_ucode(uint64_t *, u_int, u_int, u_int, u_int *);
static int	qat_ae_exec_ucode(struct qat_softc *, u_char, u_char,
		    uint64_t *, u_int, int, u_int, u_int *);
static int	qat_ae_exec_ucode_init_lm(struct qat_softc *, u_char, u_char,
		    int *, uint64_t *, u_int,
		    u_int *, u_int *, u_int *, u_int *, u_int *);
static int	qat_ae_restore_init_lm_gprs(struct qat_softc *, u_char, u_char,
		    u_int, u_int, u_int, u_int, u_int);
static int	qat_ae_get_inst_num(int);
static int	qat_ae_batch_put_lm(struct qat_softc *, u_char,
		    struct qat_ae_batch_init_list *, size_t);
static int	qat_ae_write_pc(struct qat_softc *, u_char, u_int, u_int);

static u_int	qat_aefw_csum(char *, int);
static const char *qat_aefw_uof_string(struct qat_softc *, size_t);
static struct uof_chunk_hdr *qat_aefw_uof_find_chunk(struct qat_softc *,
		    const char *, struct uof_chunk_hdr *);

static int	qat_aefw_load_mof(struct qat_softc *);
static void	qat_aefw_unload_mof(struct qat_softc *);
static int	qat_aefw_load_mmp(struct qat_softc *);
static void	qat_aefw_unload_mmp(struct qat_softc *);

static int	qat_aefw_mof_find_uof0(struct qat_softc *,
		    struct mof_uof_hdr *, struct mof_uof_chunk_hdr *,
		    u_int, size_t, const char *,
		    size_t *, void **);
static int	qat_aefw_mof_find_uof(struct qat_softc *);
static int	qat_aefw_mof_parse(struct qat_softc *);

static int	qat_aefw_uof_parse_image(struct qat_softc *,
		    struct qat_uof_image *, struct uof_chunk_hdr *uch);
static int	qat_aefw_uof_parse_images(struct qat_softc *);
static int	qat_aefw_uof_parse(struct qat_softc *);

static int	qat_aefw_alloc_auth_dmamem(struct qat_softc *, char *, size_t,
		    struct qat_dmamem *);
static int	qat_aefw_auth(struct qat_softc *, struct qat_dmamem *);
static int	qat_aefw_suof_load(struct qat_softc *sc,
		    struct qat_dmamem *dma);
static int	qat_aefw_suof_parse_image(struct qat_softc *,
		    struct qat_suof_image *, struct suof_chunk_hdr *);
static int	qat_aefw_suof_parse(struct qat_softc *);
static int	qat_aefw_suof_write(struct qat_softc *);

static int	qat_aefw_uof_assign_image(struct qat_softc *, struct qat_ae *,
		    struct qat_uof_image *);
static int	qat_aefw_uof_init_ae(struct qat_softc *, u_char);
static int	qat_aefw_uof_init(struct qat_softc *);

static int	qat_aefw_init_memory_one(struct qat_softc *,
		    struct uof_init_mem *);
static void	qat_aefw_free_lm_init(struct qat_softc *, u_char);
static int	qat_aefw_init_ustore(struct qat_softc *);
static int	qat_aefw_init_reg(struct qat_softc *, u_char, u_char,
		    enum aereg_type, u_short, u_int);
static int	qat_aefw_init_reg_sym_expr(struct qat_softc *, u_char,
		    struct qat_uof_image *);
static int	qat_aefw_init_memory(struct qat_softc *);
static int	qat_aefw_init_globals(struct qat_softc *);
static uint64_t	qat_aefw_get_uof_inst(struct qat_softc *,
		    struct qat_uof_page *, u_int);
static int	qat_aefw_do_pagein(struct qat_softc *, u_char,
		    struct qat_uof_page *);
static int	qat_aefw_uof_write_one(struct qat_softc *,
		    struct qat_uof_image *);
static int	qat_aefw_uof_write(struct qat_softc *);

static int
qat_ae_write_4(struct qat_softc *sc, u_char ae, bus_size_t offset,
	uint32_t value)
{
	int times = TIMEOUT_AE_CSR;

	do {
		qat_ae_local_write_4(sc, ae, offset, value);
		if ((qat_ae_local_read_4(sc, ae, LOCAL_CSR_STATUS) &
		    LOCAL_CSR_STATUS_STATUS) == 0)
			return 0;

	} while (times--);

	device_printf(sc->sc_dev,
	    "couldn't write AE CSR: ae 0x%hhx offset 0x%lx\n", ae, (long)offset);
	return EFAULT;
}

static int
qat_ae_read_4(struct qat_softc *sc, u_char ae, bus_size_t offset,
	uint32_t *value)
{
	int times = TIMEOUT_AE_CSR;
	uint32_t v;

	do {
		v = qat_ae_local_read_4(sc, ae, offset);
		if ((qat_ae_local_read_4(sc, ae, LOCAL_CSR_STATUS) &
		    LOCAL_CSR_STATUS_STATUS) == 0) {
			*value = v;
			return 0;
		}
	} while (times--);

	device_printf(sc->sc_dev,
	    "couldn't read AE CSR: ae 0x%hhx offset 0x%lx\n", ae, (long)offset);
	return EFAULT;
}

static void
qat_ae_ctx_indr_write(struct qat_softc *sc, u_char ae, uint32_t ctx_mask,
    bus_size_t offset, uint32_t value)
{
	int ctx;
	uint32_t ctxptr;

	MPASS(offset == CTX_FUTURE_COUNT_INDIRECT ||
	    offset == FUTURE_COUNT_SIGNAL_INDIRECT ||
	    offset == CTX_STS_INDIRECT ||
	    offset == CTX_WAKEUP_EVENTS_INDIRECT ||
	    offset == CTX_SIG_EVENTS_INDIRECT ||
	    offset == LM_ADDR_0_INDIRECT ||
	    offset == LM_ADDR_1_INDIRECT ||
	    offset == INDIRECT_LM_ADDR_0_BYTE_INDEX ||
	    offset == INDIRECT_LM_ADDR_1_BYTE_INDEX);

	qat_ae_read_4(sc, ae, CSR_CTX_POINTER, &ctxptr);
	for (ctx = 0; ctx < MAX_AE_CTX; ctx++) {
		if ((ctx_mask & (1 << ctx)) == 0)
			continue;
		qat_ae_write_4(sc, ae, CSR_CTX_POINTER, ctx);
		qat_ae_write_4(sc, ae, offset, value);
	}
	qat_ae_write_4(sc, ae, CSR_CTX_POINTER, ctxptr);
}

static int
qat_ae_ctx_indr_read(struct qat_softc *sc, u_char ae, uint32_t ctx,
    bus_size_t offset, uint32_t *value)
{
	int error;
	uint32_t ctxptr;

	MPASS(offset == CTX_FUTURE_COUNT_INDIRECT ||
	    offset == FUTURE_COUNT_SIGNAL_INDIRECT ||
	    offset == CTX_STS_INDIRECT ||
	    offset == CTX_WAKEUP_EVENTS_INDIRECT ||
	    offset == CTX_SIG_EVENTS_INDIRECT ||
	    offset == LM_ADDR_0_INDIRECT ||
	    offset == LM_ADDR_1_INDIRECT ||
	    offset == INDIRECT_LM_ADDR_0_BYTE_INDEX ||
	    offset == INDIRECT_LM_ADDR_1_BYTE_INDEX);

	/* save the ctx ptr */
	qat_ae_read_4(sc, ae, CSR_CTX_POINTER, &ctxptr);
	if ((ctxptr & CSR_CTX_POINTER_CONTEXT) !=
	    (ctx & CSR_CTX_POINTER_CONTEXT))
		qat_ae_write_4(sc, ae, CSR_CTX_POINTER, ctx);

	error = qat_ae_read_4(sc, ae, offset, value);

	/* restore ctx ptr */
	if ((ctxptr & CSR_CTX_POINTER_CONTEXT) !=
	    (ctx & CSR_CTX_POINTER_CONTEXT))
		qat_ae_write_4(sc, ae, CSR_CTX_POINTER, ctxptr);

	return error;
}

static u_short
qat_aereg_get_10bit_addr(enum aereg_type regtype, u_short reg)
{
	u_short addr;

	switch (regtype) {
	case AEREG_GPA_ABS:
	case AEREG_GPB_ABS:
		addr = (reg & 0x7f) | 0x80;
		break;
	case AEREG_GPA_REL:
	case AEREG_GPB_REL:
		addr = reg & 0x1f;
		break;
	case AEREG_SR_RD_REL:
	case AEREG_SR_WR_REL:
	case AEREG_SR_REL:
		addr = 0x180 | (reg & 0x1f);
		break;
	case AEREG_SR_INDX:
		addr = 0x140 | ((reg & 0x3) << 1);
		break;
	case AEREG_DR_RD_REL:
	case AEREG_DR_WR_REL:
	case AEREG_DR_REL:
		addr = 0x1c0 | (reg & 0x1f);
		break;
	case AEREG_DR_INDX:
		addr = 0x100 | ((reg & 0x3) << 1);
		break;
	case AEREG_NEIGH_INDX:
		addr = 0x241 | ((reg & 0x3) << 1);
		break;
	case AEREG_NEIGH_REL:
		addr = 0x280 | (reg & 0x1f);
		break;
	case AEREG_LMEM0:
		addr = 0x200;
		break;
	case AEREG_LMEM1:
		addr = 0x220;
		break;
	case AEREG_NO_DEST:
		addr = 0x300 | (reg & 0xff);
		break;
	default:
		addr = AEREG_BAD_REGADDR;
		break;
	}
	return (addr);
}

static int
qat_aereg_rel_data_write(struct qat_softc *sc, u_char ae, u_char ctx,
    enum aereg_type regtype, u_short relreg, uint32_t value)
{
	uint16_t srchi, srclo, destaddr, data16hi, data16lo;
	uint64_t inst[] = {
		0x0F440000000ull,	/* immed_w1[reg, val_hi16] */
		0x0F040000000ull,	/* immed_w0[reg, val_lo16] */
		0x0F0000C0300ull,	/* nop */
		0x0E000010000ull	/* ctx_arb[kill] */
	};
	const int ninst = nitems(inst);
	const int imm_w1 = 0, imm_w0 = 1;
	unsigned int ctxen;
	uint16_t mask;

	/* This logic only works for GPRs and LM index registers,
	   not NN or XFER registers! */
	MPASS(regtype == AEREG_GPA_REL || regtype == AEREG_GPB_REL ||
	    regtype == AEREG_LMEM0 || regtype == AEREG_LMEM1);

	if ((regtype == AEREG_GPA_REL) || (regtype == AEREG_GPB_REL)) {
		/* determine the context mode */
		qat_ae_read_4(sc, ae, CTX_ENABLES, &ctxen);
		if (ctxen & CTX_ENABLES_INUSE_CONTEXTS) {
			/* 4-ctx mode */
			if (ctx & 0x1)
				return EINVAL;
			mask = 0x1f;
		} else {
			/* 8-ctx mode */
			mask = 0x0f;
		}
		if (relreg & ~mask)
			return EINVAL;
	}
	if ((destaddr = qat_aereg_get_10bit_addr(regtype, relreg)) ==
	    AEREG_BAD_REGADDR) {
		return EINVAL;
	}

	data16lo = 0xffff & value;
	data16hi = 0xffff & (value >> 16);
	srchi = qat_aereg_get_10bit_addr(AEREG_NO_DEST,
		(uint16_t)(0xff & data16hi));
	srclo = qat_aereg_get_10bit_addr(AEREG_NO_DEST,
		(uint16_t)(0xff & data16lo));

	switch (regtype) {
	case AEREG_GPA_REL:	/* A rel source */
		inst[imm_w1] = inst[imm_w1] | ((data16hi >> 8) << 20) |
		    ((srchi & 0x3ff) << 10) | (destaddr & 0x3ff);
		inst[imm_w0] = inst[imm_w0] | ((data16lo >> 8) << 20) |
		    ((srclo & 0x3ff) << 10) | (destaddr & 0x3ff);
		break;
	default:
		inst[imm_w1] = inst[imm_w1] | ((data16hi >> 8) << 20) |
		    ((destaddr & 0x3ff) << 10) | (srchi & 0x3ff);
		inst[imm_w0] = inst[imm_w0] | ((data16lo >> 8) << 20) |
		    ((destaddr & 0x3ff) << 10) | (srclo & 0x3ff);
		break;
	}

	return qat_ae_exec_ucode(sc, ae, ctx, inst, ninst, 1, ninst * 5, NULL);
}

static int
qat_aereg_rel_data_read(struct qat_softc *sc, u_char ae, u_char ctx,
    enum aereg_type regtype, u_short relreg, uint32_t *value)
{
	uint64_t inst, savucode;
	uint32_t ctxen, misc, nmisc, savctx, ctxarbctl, ulo, uhi;
	u_int uaddr, ustore_addr;
	int error;
	u_short mask, regaddr;
	u_char nae;

	MPASS(regtype == AEREG_GPA_REL || regtype == AEREG_GPB_REL ||
	    regtype == AEREG_SR_REL || regtype == AEREG_SR_RD_REL ||
	    regtype == AEREG_DR_REL || regtype == AEREG_DR_RD_REL ||
	    regtype == AEREG_LMEM0 || regtype == AEREG_LMEM1);

	if ((regtype == AEREG_GPA_REL) || (regtype == AEREG_GPB_REL) ||
	    (regtype == AEREG_SR_REL) || (regtype == AEREG_SR_RD_REL) ||
	    (regtype == AEREG_DR_REL) || (regtype == AEREG_DR_RD_REL))
	{
		/* determine the context mode */
		qat_ae_read_4(sc, ae, CTX_ENABLES, &ctxen);
		if (ctxen & CTX_ENABLES_INUSE_CONTEXTS) {
			/* 4-ctx mode */
			if (ctx & 0x1)
				return EINVAL;
			mask = 0x1f;
		} else {
			/* 8-ctx mode */
			mask = 0x0f;
		}
		if (relreg & ~mask)
			return EINVAL;
	}
	if ((regaddr = qat_aereg_get_10bit_addr(regtype, relreg)) ==
	    AEREG_BAD_REGADDR) {
		return EINVAL;
	}

	/* instruction -- alu[--, --, B, reg] */
	switch (regtype) {
	case AEREG_GPA_REL:
		/* A rel source */
		inst = 0xA070000000ull | (regaddr & 0x3ff);
		break;
	default:
		inst = (0xA030000000ull | ((regaddr & 0x3ff) << 10));
		break;
	}

	/* backup shared control store bit, and force AE to
	 * none-shared mode before executing ucode snippet */
	qat_ae_read_4(sc, ae, AE_MISC_CONTROL, &misc);
	if (misc & AE_MISC_CONTROL_SHARE_CS) {
		qat_ae_get_shared_ustore_ae(ae, &nae);
		if ((1 << nae) & sc->sc_ae_mask && qat_ae_is_active(sc, nae))
			return EBUSY;
	}

	nmisc = misc & ~AE_MISC_CONTROL_SHARE_CS;
	qat_ae_write_4(sc, ae, AE_MISC_CONTROL, nmisc);

	/* read current context */
	qat_ae_read_4(sc, ae, ACTIVE_CTX_STATUS, &savctx);
	qat_ae_read_4(sc, ae, CTX_ARB_CNTL, &ctxarbctl);

	qat_ae_read_4(sc, ae, CTX_ENABLES, &ctxen);
	/* prevent clearing the W1C bits: the breakpoint bit,
	ECC error bit, and Parity error bit */
	ctxen &= CTX_ENABLES_IGNORE_W1C_MASK;

	/* change the context */
	if (ctx != (savctx & ACTIVE_CTX_STATUS_ACNO))
		qat_ae_write_4(sc, ae, ACTIVE_CTX_STATUS,
		    ctx & ACTIVE_CTX_STATUS_ACNO);
	/* save a ustore location */
	if ((error = qat_ae_ucode_read(sc, ae, 0, 1, &savucode)) != 0) {
		/* restore AE_MISC_CONTROL csr */
		qat_ae_write_4(sc, ae, AE_MISC_CONTROL, misc);

		/* restore the context */
		if (ctx != (savctx & ACTIVE_CTX_STATUS_ACNO)) {
			qat_ae_write_4(sc, ae, ACTIVE_CTX_STATUS,
			    savctx & ACTIVE_CTX_STATUS_ACNO);
		}
		qat_ae_write_4(sc, ae, CTX_ARB_CNTL, ctxarbctl);

		return (error);
	}

	/* turn off ustore parity */
	qat_ae_write_4(sc, ae, CTX_ENABLES,
	    ctxen & (~CTX_ENABLES_CNTL_STORE_PARITY_ENABLE));

	/* save ustore-addr csr */
	qat_ae_read_4(sc, ae, USTORE_ADDRESS, &ustore_addr);

	/* write the ALU instruction to ustore, enable ecs bit */
	uaddr = 0 | USTORE_ADDRESS_ECS;

	/* set the uaddress */
	qat_ae_write_4(sc, ae, USTORE_ADDRESS, uaddr);
	inst = qat_ae_ucode_set_ecc(inst);

	ulo = (uint32_t)(inst & 0xffffffff);
	uhi = (uint32_t)(inst >> 32);

	qat_ae_write_4(sc, ae, USTORE_DATA_LOWER, ulo);

	/* this will auto increment the address */
	qat_ae_write_4(sc, ae, USTORE_DATA_UPPER, uhi);

	/* set the uaddress */
	qat_ae_write_4(sc, ae, USTORE_ADDRESS, uaddr);

	/* delay for at least 8 cycles */
	qat_ae_wait_num_cycles(sc, ae, 0x8, 0);

	/* read ALU output -- the instruction should have been executed
	prior to clearing the ECS in putUwords */
	qat_ae_read_4(sc, ae, ALU_OUT, value);

	/* restore ustore-addr csr */
	qat_ae_write_4(sc, ae, USTORE_ADDRESS, ustore_addr);

	/* restore the ustore */
	error = qat_ae_ucode_write(sc, ae, 0, 1, &savucode);

	/* restore the context */
	if (ctx != (savctx & ACTIVE_CTX_STATUS_ACNO)) {
		qat_ae_write_4(sc, ae, ACTIVE_CTX_STATUS,
		    savctx & ACTIVE_CTX_STATUS_ACNO);
	}

	qat_ae_write_4(sc, ae, CTX_ARB_CNTL, ctxarbctl);

	/* restore AE_MISC_CONTROL csr */
	qat_ae_write_4(sc, ae, AE_MISC_CONTROL, misc);

	qat_ae_write_4(sc, ae, CTX_ENABLES, ctxen);

	return error;
}

static int
qat_aereg_rel_rdxfer_write(struct qat_softc *sc, u_char ae, u_char ctx,
    enum aereg_type regtype, u_short relreg, uint32_t value)
{
	bus_size_t addr;
	int error;
	uint32_t ctxen;
	u_short mask;
	u_short dr_offset;

	MPASS(regtype == AEREG_SR_REL || regtype == AEREG_DR_REL ||
	    regtype == AEREG_SR_RD_REL || regtype == AEREG_DR_RD_REL);

	error = qat_ae_read_4(sc, ae, CTX_ENABLES, &ctxen);
	if (ctxen & CTX_ENABLES_INUSE_CONTEXTS) {
		if (ctx & 0x1) {
			device_printf(sc->sc_dev,
			    "bad ctx argument in 4-ctx mode,ctx=0x%x\n", ctx);
			return EINVAL;
		}
		mask = 0x1f;
		dr_offset = 0x20;

	} else {
		mask = 0x0f;
		dr_offset = 0x10;
	}

	if (relreg & ~mask)
		return EINVAL;

	addr = relreg + (ctx << 0x5);

	switch (regtype) {
	case AEREG_SR_REL:
	case AEREG_SR_RD_REL:
		qat_ae_xfer_write_4(sc, ae, addr, value);
		break;
	case AEREG_DR_REL:
	case AEREG_DR_RD_REL:
		qat_ae_xfer_write_4(sc, ae, addr + dr_offset, value);
		break;
	default:
		error = EINVAL;
	}

	return error;
}

static int
qat_aereg_rel_wrxfer_write(struct qat_softc *sc, u_char ae, u_char ctx,
    enum aereg_type regtype, u_short relreg, uint32_t value)
{

	panic("notyet");

	return 0;
}

static int
qat_aereg_rel_nn_write(struct qat_softc *sc, u_char ae, u_char ctx,
    enum aereg_type regtype, u_short relreg, uint32_t value)
{

	panic("notyet");

	return 0;
}

static int
qat_aereg_abs_to_rel(struct qat_softc *sc, u_char ae,
	u_short absreg, u_short *relreg, u_char *ctx)
{
	uint32_t ctxen;

	qat_ae_read_4(sc, ae, CTX_ENABLES, &ctxen);
	if (ctxen & CTX_ENABLES_INUSE_CONTEXTS) {
		/* 4-ctx mode */
		*relreg = absreg & 0x1f;
		*ctx = (absreg >> 0x4) & 0x6;
	} else {
		/* 8-ctx mode */
		*relreg = absreg & 0x0f;
		*ctx = (absreg >> 0x4) & 0x7;
	}

	return 0;
}

static int
qat_aereg_abs_data_write(struct qat_softc *sc, u_char ae,
	enum aereg_type regtype, u_short absreg, uint32_t value)
{
	int error;
	u_short relreg;
	u_char ctx;

	qat_aereg_abs_to_rel(sc, ae, absreg, &relreg, &ctx);

	switch (regtype) {
	case AEREG_GPA_ABS:
		MPASS(absreg < MAX_GPR_REG);
		error = qat_aereg_rel_data_write(sc, ae, ctx, AEREG_GPA_REL,
		    relreg, value);
		break;
	case AEREG_GPB_ABS:
		MPASS(absreg < MAX_GPR_REG);
		error = qat_aereg_rel_data_write(sc, ae, ctx, AEREG_GPB_REL,
		    relreg, value);
		break;
	case AEREG_DR_RD_ABS:
		MPASS(absreg < MAX_XFER_REG);
		error = qat_aereg_rel_rdxfer_write(sc, ae, ctx, AEREG_DR_RD_REL,
		    relreg, value);
		break;
	case AEREG_SR_RD_ABS:
		MPASS(absreg < MAX_XFER_REG);
		error = qat_aereg_rel_rdxfer_write(sc, ae, ctx, AEREG_SR_RD_REL,
		    relreg, value);
		break;
	case AEREG_DR_WR_ABS:
		MPASS(absreg < MAX_XFER_REG);
		error = qat_aereg_rel_wrxfer_write(sc, ae, ctx, AEREG_DR_WR_REL,
		    relreg, value);
		break;
	case AEREG_SR_WR_ABS:
		MPASS(absreg < MAX_XFER_REG);
		error = qat_aereg_rel_wrxfer_write(sc, ae, ctx, AEREG_SR_WR_REL,
		    relreg, value);
		break;
	case AEREG_NEIGH_ABS:
		MPASS(absreg < MAX_NN_REG);
		if (absreg >= MAX_NN_REG)
			return EINVAL;
		error = qat_aereg_rel_nn_write(sc, ae, ctx, AEREG_NEIGH_REL,
		    relreg, value);
		break;
	default:
		panic("Invalid Register Type");
	}

	return error;
}

static void
qat_ae_enable_ctx(struct qat_softc *sc, u_char ae, u_int ctx_mask)
{
	uint32_t ctxen;

	qat_ae_read_4(sc, ae, CTX_ENABLES, &ctxen);
	ctxen &= CTX_ENABLES_IGNORE_W1C_MASK;

	if (ctxen & CTX_ENABLES_INUSE_CONTEXTS) {
		ctx_mask &= 0x55;
	} else {
		ctx_mask &= 0xff;
	}

	ctxen |= __SHIFTIN(ctx_mask, CTX_ENABLES_ENABLE);
	qat_ae_write_4(sc, ae, CTX_ENABLES, ctxen);
}

static void
qat_ae_disable_ctx(struct qat_softc *sc, u_char ae, u_int ctx_mask)
{
	uint32_t ctxen;

	qat_ae_read_4(sc, ae, CTX_ENABLES, &ctxen);
	ctxen &= CTX_ENABLES_IGNORE_W1C_MASK;
	ctxen &= ~(__SHIFTIN(ctx_mask & AE_ALL_CTX, CTX_ENABLES_ENABLE));
	qat_ae_write_4(sc, ae, CTX_ENABLES, ctxen);
}

static void
qat_ae_write_ctx_mode(struct qat_softc *sc, u_char ae, u_char mode)
{
	uint32_t val, nval;

	qat_ae_read_4(sc, ae, CTX_ENABLES, &val);
	val &= CTX_ENABLES_IGNORE_W1C_MASK;

	if (mode == 4)
		nval = val | CTX_ENABLES_INUSE_CONTEXTS;
	else
		nval = val & ~CTX_ENABLES_INUSE_CONTEXTS;

	if (val != nval)
		qat_ae_write_4(sc, ae, CTX_ENABLES, nval);
}

static void
qat_ae_write_nn_mode(struct qat_softc *sc, u_char ae, u_char mode)
{
	uint32_t val, nval;

	qat_ae_read_4(sc, ae, CTX_ENABLES, &val);
	val &= CTX_ENABLES_IGNORE_W1C_MASK;

	if (mode)
		nval = val | CTX_ENABLES_NN_MODE;
	else
		nval = val & ~CTX_ENABLES_NN_MODE;

	if (val != nval)
		qat_ae_write_4(sc, ae, CTX_ENABLES, nval);
}

static void
qat_ae_write_lm_mode(struct qat_softc *sc, u_char ae,
	enum aereg_type lm, u_char mode)
{
	uint32_t val, nval;
	uint32_t bit;

	qat_ae_read_4(sc, ae, CTX_ENABLES, &val);
	val &= CTX_ENABLES_IGNORE_W1C_MASK;

	switch (lm) {
	case AEREG_LMEM0:
		bit = CTX_ENABLES_LMADDR_0_GLOBAL;
		break;
	case AEREG_LMEM1:
		bit = CTX_ENABLES_LMADDR_1_GLOBAL;
		break;
	default:
		panic("invalid lmem reg type");
		break;
	}

	if (mode)
		nval = val | bit;
	else
		nval = val & ~bit;

	if (val != nval)
		qat_ae_write_4(sc, ae, CTX_ENABLES, nval);
}

static void
qat_ae_write_shared_cs_mode0(struct qat_softc *sc, u_char ae, u_char mode)
{
	uint32_t val, nval;

	qat_ae_read_4(sc, ae, AE_MISC_CONTROL, &val);

	if (mode == 1)
		nval = val | AE_MISC_CONTROL_SHARE_CS;
	else
		nval = val & ~AE_MISC_CONTROL_SHARE_CS;

	if (val != nval)
		qat_ae_write_4(sc, ae, AE_MISC_CONTROL, nval);
}

static void
qat_ae_write_shared_cs_mode(struct qat_softc *sc, u_char ae, u_char mode)
{
	u_char nae;

	qat_ae_get_shared_ustore_ae(ae, &nae);

	qat_ae_write_shared_cs_mode0(sc, ae, mode);

	if ((sc->sc_ae_mask & (1 << nae))) {
		qat_ae_write_shared_cs_mode0(sc, nae, mode);
	}
}

static int
qat_ae_set_reload_ustore(struct qat_softc *sc, u_char ae,
	u_int reload_size, int shared_mode, u_int ustore_dram_addr)
{
	uint32_t val, cs_reload;

	switch (reload_size) {
	case 0:
		cs_reload = 0x0;
		break;
	case QAT_2K:
		cs_reload = 0x1;
		break;
	case QAT_4K:
		cs_reload = 0x2;
		break;
	case QAT_8K:
		cs_reload = 0x3;
		break;
	default:
		return EINVAL;
	}

	if (cs_reload)
		QAT_AE(sc, ae).qae_ustore_dram_addr = ustore_dram_addr;

	QAT_AE(sc, ae).qae_reload_size = reload_size;

	qat_ae_read_4(sc, ae, AE_MISC_CONTROL, &val);
	val &= ~(AE_MISC_CONTROL_ONE_CTX_RELOAD |
	    AE_MISC_CONTROL_CS_RELOAD | AE_MISC_CONTROL_SHARE_CS);
	val |= __SHIFTIN(cs_reload, AE_MISC_CONTROL_CS_RELOAD) |
	    __SHIFTIN(shared_mode, AE_MISC_CONTROL_ONE_CTX_RELOAD);
	qat_ae_write_4(sc, ae, AE_MISC_CONTROL, val);

	return 0;
}

static enum qat_ae_status
qat_ae_get_status(struct qat_softc *sc, u_char ae)
{
	int error;
	uint32_t val = 0;

	error = qat_ae_read_4(sc, ae, CTX_ENABLES, &val);
	if (error || val & CTX_ENABLES_ENABLE)
		return QAT_AE_ENABLED;

	qat_ae_read_4(sc, ae, ACTIVE_CTX_STATUS, &val);
	if (val & ACTIVE_CTX_STATUS_ABO)
		return QAT_AE_ACTIVE;

	return QAT_AE_DISABLED;
}


static int
qat_ae_is_active(struct qat_softc *sc, u_char ae)
{
	uint32_t val;

	if (qat_ae_get_status(sc, ae) != QAT_AE_DISABLED)
		return 1;

	qat_ae_read_4(sc, ae, ACTIVE_CTX_STATUS, &val);
	if (val & ACTIVE_CTX_STATUS_ABO)
		return 1;
	else
		return 0;
}

/* returns 1 if actually waited for specified number of cycles */
static int
qat_ae_wait_num_cycles(struct qat_softc *sc, u_char ae, int cycles, int check)
{
	uint32_t cnt, actx;
	int pcnt, ccnt, elapsed, times;

	qat_ae_read_4(sc, ae, PROFILE_COUNT, &cnt);
	pcnt = cnt & 0xffff;

	times = TIMEOUT_AE_CHECK;
	do {
		qat_ae_read_4(sc, ae, PROFILE_COUNT, &cnt);
		ccnt = cnt & 0xffff;

		elapsed = ccnt - pcnt;
		if (elapsed == 0) {
			times--;
		}
		if (times <= 0) {
			device_printf(sc->sc_dev,
			    "qat_ae_wait_num_cycles timeout\n");
			return -1;
		}

		if (elapsed < 0)
			elapsed += 0x10000;

		if (elapsed >= CYCLES_FROM_READY2EXE && check) {
			if (qat_ae_read_4(sc, ae, ACTIVE_CTX_STATUS,
			    &actx) == 0) {
				if ((actx & ACTIVE_CTX_STATUS_ABO) == 0)
					return 0;
			}
		}
	} while (cycles > elapsed);

	if (check && qat_ae_read_4(sc, ae, ACTIVE_CTX_STATUS, &actx) == 0) {
		if ((actx & ACTIVE_CTX_STATUS_ABO) == 0)
			return 0;
	}

	return 1;
}

int
qat_ae_init(struct qat_softc *sc)
{
	int error;
	uint32_t mask, val = 0;
	u_char ae;

	/* XXX adf_initSysMemInfo */

	/* XXX Disable clock gating for some chip if debug mode */

	for (ae = 0, mask = sc->sc_ae_mask; mask; ae++, mask >>= 1) {
		struct qat_ae *qae = &sc->sc_ae[ae];
		if (!(mask & 1))
			continue;

		qae->qae_ustore_size = USTORE_SIZE;

		qae->qae_free_addr = 0;
		qae->qae_free_size = USTORE_SIZE;
		qae->qae_live_ctx_mask = AE_ALL_CTX;
		qae->qae_ustore_dram_addr = 0;
		qae->qae_reload_size = 0;
	}

	/* XXX Enable attention interrupt */

	error = qat_ae_clear_reset(sc);
	if (error)
		return error;

	qat_ae_clear_xfer(sc);

	if (!sc->sc_hw.qhw_fw_auth) {
		error = qat_ae_clear_gprs(sc);
		if (error)
			return error;
	}

	/* Set SIGNATURE_ENABLE[0] to 0x1 in order to enable ALU_OUT csr */
	for (ae = 0, mask = sc->sc_ae_mask; mask; ae++, mask >>= 1) {
		if (!(mask & 1))
			continue;
		qat_ae_read_4(sc, ae, SIGNATURE_ENABLE, &val);
		val |= 0x1;
		qat_ae_write_4(sc, ae, SIGNATURE_ENABLE, val);
	}

	error = qat_ae_clear_reset(sc);
	if (error)
		return error;

	/* XXX XXX XXX Clean MMP memory if mem scrub is supported */
	/* halMem_ScrubMMPMemory */

	return 0;
}

int
qat_ae_start(struct qat_softc *sc)
{
	int error;
	u_char ae;

	for (ae = 0; ae < sc->sc_ae_num; ae++) {
		if ((sc->sc_ae_mask & (1 << ae)) == 0)
			continue;

		error = qat_aefw_start(sc, ae, 0xff);
		if (error)
			return error;
	}

	return 0;
}

void
qat_ae_cluster_intr(void *arg)
{
	/* Nothing to implement until we support SRIOV. */
	printf("qat_ae_cluster_intr\n");
}

static int
qat_ae_clear_reset(struct qat_softc *sc)
{
	int error;
	uint32_t times, reset, clock, reg, mask;
	u_char ae;

	reset = qat_cap_global_read_4(sc, CAP_GLOBAL_CTL_RESET);
	reset &= ~(__SHIFTIN(sc->sc_ae_mask, CAP_GLOBAL_CTL_RESET_AE_MASK));
	reset &= ~(__SHIFTIN(sc->sc_accel_mask, CAP_GLOBAL_CTL_RESET_ACCEL_MASK));
	times = TIMEOUT_AE_RESET;
	do {
		qat_cap_global_write_4(sc, CAP_GLOBAL_CTL_RESET, reset);
		if ((times--) == 0) {
			device_printf(sc->sc_dev, "couldn't reset AEs\n");
			return EBUSY;
		}
		reg = qat_cap_global_read_4(sc, CAP_GLOBAL_CTL_RESET);
	} while ((__SHIFTIN(sc->sc_ae_mask, CAP_GLOBAL_CTL_RESET_AE_MASK) |
	    __SHIFTIN(sc->sc_accel_mask, CAP_GLOBAL_CTL_RESET_ACCEL_MASK))
	    & reg);

	/* Enable clock for AE and QAT */
	clock = qat_cap_global_read_4(sc, CAP_GLOBAL_CTL_CLK_EN);
	clock |= __SHIFTIN(sc->sc_ae_mask, CAP_GLOBAL_CTL_CLK_EN_AE_MASK);
	clock |= __SHIFTIN(sc->sc_accel_mask, CAP_GLOBAL_CTL_CLK_EN_ACCEL_MASK);
	qat_cap_global_write_4(sc, CAP_GLOBAL_CTL_CLK_EN, clock);

	error = qat_ae_check(sc);
	if (error)
		return error;

	/*
	 * Set undefined power-up/reset states to reasonable default values...
	 * just to make sure we're starting from a known point
	 */
	for (ae = 0, mask = sc->sc_ae_mask; mask; ae++, mask >>= 1) {
		if (!(mask & 1))
			continue;

		/* init the ctx_enable */
		qat_ae_write_4(sc, ae, CTX_ENABLES,
		    CTX_ENABLES_INIT);

		/* initialize the PCs */
		qat_ae_ctx_indr_write(sc, ae, AE_ALL_CTX,
		    CTX_STS_INDIRECT,
		    UPC_MASK & CTX_STS_INDIRECT_UPC_INIT);

		/* init the ctx_arb */
		qat_ae_write_4(sc, ae, CTX_ARB_CNTL,
		    CTX_ARB_CNTL_INIT);

		/* enable cc */
		qat_ae_write_4(sc, ae, CC_ENABLE,
		    CC_ENABLE_INIT);
		qat_ae_ctx_indr_write(sc, ae, AE_ALL_CTX,
		    CTX_WAKEUP_EVENTS_INDIRECT,
		    CTX_WAKEUP_EVENTS_INDIRECT_INIT);
		qat_ae_ctx_indr_write(sc, ae, AE_ALL_CTX,
		    CTX_SIG_EVENTS_INDIRECT,
		    CTX_SIG_EVENTS_INDIRECT_INIT);
	}

	if ((sc->sc_ae_mask != 0) &&
	    sc->sc_flags & QAT_FLAG_ESRAM_ENABLE_AUTO_INIT) {
		/* XXX XXX XXX init eSram only when this is boot time */
	}

	if ((sc->sc_ae_mask != 0) &&
	    sc->sc_flags & QAT_FLAG_SHRAM_WAIT_READY) {
		/* XXX XXX XXX wait shram to complete initialization */
	}

	qat_ae_reset_timestamp(sc);

	return 0;
}

static int
qat_ae_check(struct qat_softc *sc)
{
	int error, times, ae;
	uint32_t cnt, pcnt, mask;

	for (ae = 0, mask = sc->sc_ae_mask; mask; ae++, mask >>= 1) {
		if (!(mask & 1))
			continue;

		times = TIMEOUT_AE_CHECK;
		error = qat_ae_read_4(sc, ae, PROFILE_COUNT, &cnt);
		if (error) {
			device_printf(sc->sc_dev,
			    "couldn't access AE %d CSR\n", ae);
			return error;
		}
		pcnt = cnt & 0xffff;

		while (1) {
			error = qat_ae_read_4(sc, ae,
			    PROFILE_COUNT, &cnt);
			if (error) {
				device_printf(sc->sc_dev,
				    "couldn't access AE %d CSR\n", ae);
				return error;
			}
			cnt &= 0xffff;
			if (cnt == pcnt)
				times--;
			else
				break;
			if (times <= 0) {
				device_printf(sc->sc_dev,
				    "AE %d CSR is useless\n", ae);
				return EFAULT;
			}
		}
	}

	return 0;
}

static int
qat_ae_reset_timestamp(struct qat_softc *sc)
{
	uint32_t misc, mask;
	u_char ae;

	/* stop the timestamp timers */
	misc = qat_cap_global_read_4(sc, CAP_GLOBAL_CTL_MISC);
	if (misc & CAP_GLOBAL_CTL_MISC_TIMESTAMP_EN) {
		qat_cap_global_write_4(sc, CAP_GLOBAL_CTL_MISC,
		    misc & (~CAP_GLOBAL_CTL_MISC_TIMESTAMP_EN));
	}

	for (ae = 0, mask = sc->sc_ae_mask; mask; ae++, mask >>= 1) {
		if (!(mask & 1))
			continue;
		qat_ae_write_4(sc, ae, TIMESTAMP_LOW, 0);
		qat_ae_write_4(sc, ae, TIMESTAMP_HIGH, 0);
	}

	/* start timestamp timers */
	qat_cap_global_write_4(sc, CAP_GLOBAL_CTL_MISC,
	    misc | CAP_GLOBAL_CTL_MISC_TIMESTAMP_EN);

	return 0;
}

static void
qat_ae_clear_xfer(struct qat_softc *sc)
{
	u_int mask, reg;
	u_char ae;

	for (ae = 0, mask = sc->sc_ae_mask; mask; ae++, mask >>= 1) {
		if (!(mask & 1))
			continue;

		for (reg = 0; reg < MAX_GPR_REG; reg++) {
			qat_aereg_abs_data_write(sc, ae, AEREG_SR_RD_ABS,
			    reg, 0);
			qat_aereg_abs_data_write(sc, ae, AEREG_DR_RD_ABS,
			    reg, 0);
		}
	}
}

static int
qat_ae_clear_gprs(struct qat_softc *sc)
{
	uint32_t val;
	uint32_t saved_ctx = 0;
	int times = TIMEOUT_AE_CHECK, rv;
	u_char ae;
	u_int mask;

	for (ae = 0, mask = sc->sc_ae_mask; mask; ae++, mask >>= 1) {
		if (!(mask & 1))
			continue;

		/* turn off share control store bit */
		val = qat_ae_read_4(sc, ae, AE_MISC_CONTROL, &val);
		val &= ~AE_MISC_CONTROL_SHARE_CS;
		qat_ae_write_4(sc, ae, AE_MISC_CONTROL, val);

		/* turn off ucode parity */
		/* make sure nn_mode is set to self */
		qat_ae_read_4(sc, ae, CTX_ENABLES, &val);
		val &= CTX_ENABLES_IGNORE_W1C_MASK;
		val |= CTX_ENABLES_NN_MODE;
		val &= ~CTX_ENABLES_CNTL_STORE_PARITY_ENABLE;
		qat_ae_write_4(sc, ae, CTX_ENABLES, val);

		/* copy instructions to ustore */
		qat_ae_ucode_write(sc, ae, 0, nitems(ae_clear_gprs_inst),
		    ae_clear_gprs_inst);

		/* set PC */
		qat_ae_ctx_indr_write(sc, ae, AE_ALL_CTX, CTX_STS_INDIRECT,
		    UPC_MASK & CTX_STS_INDIRECT_UPC_INIT);

		/* save current context */
		qat_ae_read_4(sc, ae, ACTIVE_CTX_STATUS, &saved_ctx);
		/* change the active context */
		/* start the context from ctx 0 */
		qat_ae_write_4(sc, ae, ACTIVE_CTX_STATUS, 0);

		/* wakeup-event voluntary */
		qat_ae_ctx_indr_write(sc, ae, AE_ALL_CTX,
		    CTX_WAKEUP_EVENTS_INDIRECT,
		    CTX_WAKEUP_EVENTS_INDIRECT_VOLUNTARY);
		/* clean signals */
		qat_ae_ctx_indr_write(sc, ae, AE_ALL_CTX,
		    CTX_SIG_EVENTS_INDIRECT, 0);
		qat_ae_write_4(sc, ae, CTX_SIG_EVENTS_ACTIVE, 0);

		qat_ae_enable_ctx(sc, ae, AE_ALL_CTX);
	}

	for (ae = 0, mask = sc->sc_ae_mask; mask; ae++, mask >>= 1) {
		if (!(mask & 1))
			continue;
		/* wait for AE to finish */
		do {
			rv = qat_ae_wait_num_cycles(sc, ae, AE_EXEC_CYCLE, 1);
		} while (rv && times--);
		if (times <= 0) {
			device_printf(sc->sc_dev,
			    "qat_ae_clear_gprs timeout");
			return ETIMEDOUT;
		}
		qat_ae_disable_ctx(sc, ae, AE_ALL_CTX);
		/* change the active context */
		qat_ae_write_4(sc, ae, ACTIVE_CTX_STATUS,
		    saved_ctx & ACTIVE_CTX_STATUS_ACNO);
		/* init the ctx_enable */
		qat_ae_write_4(sc, ae, CTX_ENABLES, CTX_ENABLES_INIT);
		/* initialize the PCs */
		qat_ae_ctx_indr_write(sc, ae, AE_ALL_CTX,
		    CTX_STS_INDIRECT, UPC_MASK & CTX_STS_INDIRECT_UPC_INIT);
		/* init the ctx_arb */
		qat_ae_write_4(sc, ae, CTX_ARB_CNTL, CTX_ARB_CNTL_INIT);
		/* enable cc */
		qat_ae_write_4(sc, ae, CC_ENABLE, CC_ENABLE_INIT);
		qat_ae_ctx_indr_write(sc, ae, AE_ALL_CTX,
		    CTX_WAKEUP_EVENTS_INDIRECT, CTX_WAKEUP_EVENTS_INDIRECT_INIT);
		qat_ae_ctx_indr_write(sc, ae, AE_ALL_CTX, CTX_SIG_EVENTS_INDIRECT,
		    CTX_SIG_EVENTS_INDIRECT_INIT);
	}

	return 0;
}

static void
qat_ae_get_shared_ustore_ae(u_char ae, u_char *nae)
{
	if (ae & 0x1)
		*nae = ae - 1;
	else
		*nae = ae + 1;
}

static u_int
qat_ae_ucode_parity64(uint64_t ucode)
{

	ucode ^= ucode >> 1;
	ucode ^= ucode >> 2;
	ucode ^= ucode >> 4;
	ucode ^= ucode >> 8;
	ucode ^= ucode >> 16;
	ucode ^= ucode >> 32;

	return ((u_int)(ucode & 1));
}

static uint64_t
qat_ae_ucode_set_ecc(uint64_t ucode)
{
	static const uint64_t
		bit0mask=0xff800007fffULL, bit1mask=0x1f801ff801fULL,
		bit2mask=0xe387e0781e1ULL, bit3mask=0x7cb8e388e22ULL,
		bit4mask=0xaf5b2c93244ULL, bit5mask=0xf56d5525488ULL,
		bit6mask=0xdaf69a46910ULL;

	/* clear the ecc bits */
	ucode &= ~(0x7fULL << USTORE_ECC_BIT_0);

	ucode |= (uint64_t)qat_ae_ucode_parity64(bit0mask & ucode) <<
	    USTORE_ECC_BIT_0;
	ucode |= (uint64_t)qat_ae_ucode_parity64(bit1mask & ucode) <<
	    USTORE_ECC_BIT_1;
	ucode |= (uint64_t)qat_ae_ucode_parity64(bit2mask & ucode) <<
	    USTORE_ECC_BIT_2;
	ucode |= (uint64_t)qat_ae_ucode_parity64(bit3mask & ucode) <<
	    USTORE_ECC_BIT_3;
	ucode |= (uint64_t)qat_ae_ucode_parity64(bit4mask & ucode) <<
	    USTORE_ECC_BIT_4;
	ucode |= (uint64_t)qat_ae_ucode_parity64(bit5mask & ucode) <<
	    USTORE_ECC_BIT_5;
	ucode |= (uint64_t)qat_ae_ucode_parity64(bit6mask & ucode) <<
	    USTORE_ECC_BIT_6;

	return (ucode);
}

static int
qat_ae_ucode_write(struct qat_softc *sc, u_char ae, u_int uaddr, u_int ninst,
	const uint64_t *ucode)
{
	uint64_t tmp;
	uint32_t ustore_addr, ulo, uhi;
	int i;

	qat_ae_read_4(sc, ae, USTORE_ADDRESS, &ustore_addr);
	uaddr |= USTORE_ADDRESS_ECS;

	qat_ae_write_4(sc, ae, USTORE_ADDRESS, uaddr);
	for (i = 0; i < ninst; i++) {
		tmp = qat_ae_ucode_set_ecc(ucode[i]);
		ulo = (uint32_t)(tmp & 0xffffffff);
		uhi = (uint32_t)(tmp >> 32);

		qat_ae_write_4(sc, ae, USTORE_DATA_LOWER, ulo);
		/* this will auto increment the address */
		qat_ae_write_4(sc, ae, USTORE_DATA_UPPER, uhi);
	}
	qat_ae_write_4(sc, ae, USTORE_ADDRESS, ustore_addr);

	return 0;
}

static int
qat_ae_ucode_read(struct qat_softc *sc, u_char ae, u_int uaddr, u_int ninst,
    uint64_t *ucode)
{
	uint32_t misc, ustore_addr, ulo, uhi;
	u_int ii;
	u_char nae;

	if (qat_ae_get_status(sc, ae) != QAT_AE_DISABLED)
		return EBUSY;

	/* determine whether it neighbour AE runs in shared control store
	 * status */
	qat_ae_read_4(sc, ae, AE_MISC_CONTROL, &misc);
	if (misc & AE_MISC_CONTROL_SHARE_CS) {
		qat_ae_get_shared_ustore_ae(ae, &nae);
		if ((sc->sc_ae_mask & (1 << nae)) && qat_ae_is_active(sc, nae))
			return EBUSY;
	}

	/* if reloadable, then get it all from dram-ustore */
	if (__SHIFTOUT(misc, AE_MISC_CONTROL_CS_RELOAD))
		panic("notyet"); /* XXX getReloadUwords */

	/* disable SHARE_CS bit to workaround silicon bug */
	qat_ae_write_4(sc, ae, AE_MISC_CONTROL, misc & 0xfffffffb);

	MPASS(uaddr + ninst <= USTORE_SIZE);

	/* save ustore-addr csr */
	qat_ae_read_4(sc, ae, USTORE_ADDRESS, &ustore_addr);

	uaddr |= USTORE_ADDRESS_ECS;	/* enable ecs bit */
	for (ii = 0; ii < ninst; ii++) {
		qat_ae_write_4(sc, ae, USTORE_ADDRESS, uaddr);

		uaddr++;
		qat_ae_read_4(sc, ae, USTORE_DATA_LOWER, &ulo);
		qat_ae_read_4(sc, ae, USTORE_DATA_UPPER, &uhi);
		ucode[ii] = uhi;
		ucode[ii] = (ucode[ii] << 32) | ulo;
	}

	/* restore SHARE_CS bit to workaround silicon bug */
	qat_ae_write_4(sc, ae, AE_MISC_CONTROL, misc);
	qat_ae_write_4(sc, ae, USTORE_ADDRESS, ustore_addr);

	return 0;
}

static u_int
qat_ae_concat_ucode(uint64_t *ucode, u_int ninst, u_int size, u_int addr,
    u_int *value)
{
	const uint64_t *inst_arr;
	u_int ninst0, curvalue;
	int ii, vali, fixup, usize = 0;

	if (size == 0)
		return 0;

	ninst0 = ninst;
	vali = 0;
	curvalue = value[vali++];

	switch (size) {
	case 0x1:
		inst_arr = ae_inst_1b;
		usize = nitems(ae_inst_1b);
		break;
	case 0x2:
		inst_arr = ae_inst_2b;
		usize = nitems(ae_inst_2b);
		break;
	case 0x3:
		inst_arr = ae_inst_3b;
		usize = nitems(ae_inst_3b);
		break;
	default:
		inst_arr = ae_inst_4b;
		usize = nitems(ae_inst_4b);
		break;
	}

	fixup = ninst;
	for (ii = 0; ii < usize; ii++)
		ucode[ninst++] = inst_arr[ii];

	INSERT_IMMED_GPRA_CONST(ucode[fixup], (addr));
	fixup++;
	INSERT_IMMED_GPRA_CONST(ucode[fixup], 0);
	fixup++;
	INSERT_IMMED_GPRB_CONST(ucode[fixup], (curvalue >> 0));
	fixup++;
	INSERT_IMMED_GPRB_CONST(ucode[fixup], (curvalue >> 16));
	/* XXX fixup++ ? */

	if (size <= 0x4)
		return (ninst - ninst0);

	size -= sizeof(u_int);
	while (size >= sizeof(u_int)) {
		curvalue = value[vali++];
		fixup = ninst;
		ucode[ninst++] = ae_inst_4b[0x2];
		ucode[ninst++] = ae_inst_4b[0x3];
		ucode[ninst++] = ae_inst_4b[0x8];
		INSERT_IMMED_GPRB_CONST(ucode[fixup], (curvalue >> 16));
		fixup++;
		INSERT_IMMED_GPRB_CONST(ucode[fixup], (curvalue >> 0));
		/* XXX fixup++ ? */

		addr += sizeof(u_int);
		size -= sizeof(u_int);
	}
	/* call this function recusive when the left size less than 4 */
	ninst +=
	    qat_ae_concat_ucode(ucode, ninst, size, addr, value + vali);

	return (ninst - ninst0);
}

static int
qat_ae_exec_ucode(struct qat_softc *sc, u_char ae, u_char ctx,
    uint64_t *ucode, u_int ninst, int cond_code_off, u_int max_cycles,
    u_int *endpc)
{
	int error = 0, share_cs = 0;
	uint64_t savucode[MAX_EXEC_INST];
	uint32_t indr_lm_addr_0, indr_lm_addr_1;
	uint32_t indr_lm_addr_byte_0, indr_lm_addr_byte_1;
	uint32_t indr_future_cnt_sig;
	uint32_t indr_sig, active_sig;
	uint32_t wakeup_ev, savpc, savcc, savctx, ctxarbctl;
	uint32_t misc, nmisc, ctxen;
	u_char nae;

	MPASS(ninst <= USTORE_SIZE);

	if (qat_ae_is_active(sc, ae))
		return EBUSY;

	/* save current LM addr */
	qat_ae_ctx_indr_read(sc, ae, ctx, LM_ADDR_0_INDIRECT, &indr_lm_addr_0);
	qat_ae_ctx_indr_read(sc, ae, ctx, LM_ADDR_1_INDIRECT, &indr_lm_addr_1);
	qat_ae_ctx_indr_read(sc, ae, ctx, INDIRECT_LM_ADDR_0_BYTE_INDEX,
	    &indr_lm_addr_byte_0);
	qat_ae_ctx_indr_read(sc, ae, ctx, INDIRECT_LM_ADDR_1_BYTE_INDEX,
	    &indr_lm_addr_byte_1);

	/* backup shared control store bit, and force AE to
	   none-shared mode before executing ucode snippet */
	qat_ae_read_4(sc, ae, AE_MISC_CONTROL, &misc);
	if (misc & AE_MISC_CONTROL_SHARE_CS) {
		share_cs = 1;
		qat_ae_get_shared_ustore_ae(ae, &nae);
		if ((sc->sc_ae_mask & (1 << nae)) && qat_ae_is_active(sc, nae))
			return EBUSY;
	}
	nmisc = misc & ~AE_MISC_CONTROL_SHARE_CS;
	qat_ae_write_4(sc, ae, AE_MISC_CONTROL, nmisc);

	/* save current states: */
	if (ninst <= MAX_EXEC_INST) {
		error = qat_ae_ucode_read(sc, ae, 0, ninst, savucode);
		if (error) {
			qat_ae_write_4(sc, ae, AE_MISC_CONTROL, misc);
			return error;
		}
	}

	/* save wakeup-events */
	qat_ae_ctx_indr_read(sc, ae, ctx, CTX_WAKEUP_EVENTS_INDIRECT,
	    &wakeup_ev);
	/* save PC */
	qat_ae_ctx_indr_read(sc, ae, ctx, CTX_STS_INDIRECT, &savpc);
	savpc &= UPC_MASK;

	/* save ctx enables */
	qat_ae_read_4(sc, ae, CTX_ENABLES, &ctxen);
	ctxen &= CTX_ENABLES_IGNORE_W1C_MASK;
	/* save conditional-code */
	qat_ae_read_4(sc, ae, CC_ENABLE, &savcc);
	/* save current context */
	qat_ae_read_4(sc, ae, ACTIVE_CTX_STATUS, &savctx);
	qat_ae_read_4(sc, ae, CTX_ARB_CNTL, &ctxarbctl);

	/* save indirect csrs */
	qat_ae_ctx_indr_read(sc, ae, ctx, FUTURE_COUNT_SIGNAL_INDIRECT,
	    &indr_future_cnt_sig);
	qat_ae_ctx_indr_read(sc, ae, ctx, CTX_SIG_EVENTS_INDIRECT, &indr_sig);
	qat_ae_read_4(sc, ae, CTX_SIG_EVENTS_ACTIVE, &active_sig);

	/* turn off ucode parity */
	qat_ae_write_4(sc, ae, CTX_ENABLES,
	    ctxen & ~CTX_ENABLES_CNTL_STORE_PARITY_ENABLE);

	/* copy instructions to ustore */
	qat_ae_ucode_write(sc, ae, 0, ninst, ucode);
	/* set PC */
	qat_ae_ctx_indr_write(sc, ae, 1 << ctx, CTX_STS_INDIRECT, 0);
	/* change the active context */
	qat_ae_write_4(sc, ae, ACTIVE_CTX_STATUS,
	    ctx & ACTIVE_CTX_STATUS_ACNO);

	if (cond_code_off) {
		/* disable conditional-code*/
		qat_ae_write_4(sc, ae, CC_ENABLE, savcc & 0xffffdfff);
	}

	/* wakeup-event voluntary */
	qat_ae_ctx_indr_write(sc, ae, 1 << ctx,
	    CTX_WAKEUP_EVENTS_INDIRECT, CTX_WAKEUP_EVENTS_INDIRECT_VOLUNTARY);

	/* clean signals */
	qat_ae_ctx_indr_write(sc, ae, 1 << ctx, CTX_SIG_EVENTS_INDIRECT, 0);
	qat_ae_write_4(sc, ae, CTX_SIG_EVENTS_ACTIVE, 0);

	/* enable context */
	qat_ae_enable_ctx(sc, ae, 1 << ctx);

	/* wait for it to finish */
	if (qat_ae_wait_num_cycles(sc, ae, max_cycles, 1) != 0)
		error = ETIMEDOUT;

	/* see if we need to get the current PC */
	if (endpc != NULL) {
		uint32_t ctx_status;

		qat_ae_ctx_indr_read(sc, ae, ctx, CTX_STS_INDIRECT,
		    &ctx_status);
		*endpc = ctx_status & UPC_MASK;
	}
#if 0
	{
		uint32_t ctx_status;

		qat_ae_ctx_indr_read(sc, ae, ctx, CTX_STS_INDIRECT,
		    &ctx_status);
		printf("%s: endpc 0x%08x\n", __func__,
		    ctx_status & UPC_MASK);
	}
#endif

	/* retore to previous states: */
	/* disable context */
	qat_ae_disable_ctx(sc, ae, 1 << ctx);
	if (ninst <= MAX_EXEC_INST) {
		/* instructions */
		qat_ae_ucode_write(sc, ae, 0, ninst, savucode);
	}
	/* wakeup-events */
	qat_ae_ctx_indr_write(sc, ae, 1 << ctx, CTX_WAKEUP_EVENTS_INDIRECT,
	    wakeup_ev);
	qat_ae_ctx_indr_write(sc, ae, 1 << ctx, CTX_STS_INDIRECT, savpc);

	/* only restore shared control store bit,
	   other bit might be changed by AE code snippet */
	qat_ae_read_4(sc, ae, AE_MISC_CONTROL, &misc);
	if (share_cs)
		nmisc = misc | AE_MISC_CONTROL_SHARE_CS;
	else
		nmisc = misc & ~AE_MISC_CONTROL_SHARE_CS;
	qat_ae_write_4(sc, ae, AE_MISC_CONTROL, nmisc);
	/* conditional-code */
	qat_ae_write_4(sc, ae, CC_ENABLE, savcc);
	/* change the active context */
	qat_ae_write_4(sc, ae, ACTIVE_CTX_STATUS,
	    savctx & ACTIVE_CTX_STATUS_ACNO);
	/* restore the nxt ctx to run */
	qat_ae_write_4(sc, ae, CTX_ARB_CNTL, ctxarbctl);
	/* restore current LM addr */
	qat_ae_ctx_indr_write(sc, ae, 1 << ctx, LM_ADDR_0_INDIRECT,
	    indr_lm_addr_0);
	qat_ae_ctx_indr_write(sc, ae, 1 << ctx, LM_ADDR_1_INDIRECT,
	    indr_lm_addr_1);
	qat_ae_ctx_indr_write(sc, ae, 1 << ctx, INDIRECT_LM_ADDR_0_BYTE_INDEX,
	    indr_lm_addr_byte_0);
	qat_ae_ctx_indr_write(sc, ae, 1 << ctx, INDIRECT_LM_ADDR_1_BYTE_INDEX,
	    indr_lm_addr_byte_1);

	/* restore indirect csrs */
	qat_ae_ctx_indr_write(sc, ae, 1 << ctx, FUTURE_COUNT_SIGNAL_INDIRECT,
	    indr_future_cnt_sig);
	qat_ae_ctx_indr_write(sc, ae, 1 << ctx, CTX_SIG_EVENTS_INDIRECT,
	    indr_sig);
	qat_ae_write_4(sc, ae, CTX_SIG_EVENTS_ACTIVE, active_sig);

	/* ctx-enables */
	qat_ae_write_4(sc, ae, CTX_ENABLES, ctxen);

	return error;
}

static int
qat_ae_exec_ucode_init_lm(struct qat_softc *sc, u_char ae, u_char ctx,
    int *first_exec, uint64_t *ucode, u_int ninst,
    u_int *gpr_a0, u_int *gpr_a1, u_int *gpr_a2, u_int *gpr_b0, u_int *gpr_b1)
{

	if (*first_exec) {
		qat_aereg_rel_data_read(sc, ae, ctx, AEREG_GPA_REL, 0, gpr_a0);
		qat_aereg_rel_data_read(sc, ae, ctx, AEREG_GPA_REL, 1, gpr_a1);
		qat_aereg_rel_data_read(sc, ae, ctx, AEREG_GPA_REL, 2, gpr_a2);
		qat_aereg_rel_data_read(sc, ae, ctx, AEREG_GPB_REL, 0, gpr_b0);
		qat_aereg_rel_data_read(sc, ae, ctx, AEREG_GPB_REL, 1, gpr_b1);
		*first_exec = 0;
	}

	return qat_ae_exec_ucode(sc, ae, ctx, ucode, ninst, 1, ninst * 5, NULL);
}

static int
qat_ae_restore_init_lm_gprs(struct qat_softc *sc, u_char ae, u_char ctx,
    u_int gpr_a0, u_int gpr_a1, u_int gpr_a2, u_int gpr_b0, u_int gpr_b1)
{
	qat_aereg_rel_data_write(sc, ae, ctx, AEREG_GPA_REL, 0, gpr_a0);
	qat_aereg_rel_data_write(sc, ae, ctx, AEREG_GPA_REL, 1, gpr_a1);
	qat_aereg_rel_data_write(sc, ae, ctx, AEREG_GPA_REL, 2, gpr_a2);
	qat_aereg_rel_data_write(sc, ae, ctx, AEREG_GPB_REL, 0, gpr_b0);
	qat_aereg_rel_data_write(sc, ae, ctx, AEREG_GPB_REL, 1, gpr_b1);

	return 0;
}

static int
qat_ae_get_inst_num(int lmsize)
{
	int ninst, left;

	if (lmsize == 0)
		return 0;

	left = lmsize % sizeof(u_int);

	if (left) {
		ninst = nitems(ae_inst_1b) +
		    qat_ae_get_inst_num(lmsize - left);
	} else {
		/* 3 instruction is needed for further code */
		ninst = (lmsize - sizeof(u_int)) * 3 / 4 + nitems(ae_inst_4b);
	}

	return (ninst);
}

static int
qat_ae_batch_put_lm(struct qat_softc *sc, u_char ae,
    struct qat_ae_batch_init_list *qabi_list, size_t nqabi)
{
	struct qat_ae_batch_init *qabi;
	size_t alloc_ninst, ninst;
	uint64_t *ucode;
	u_int gpr_a0, gpr_a1, gpr_a2, gpr_b0, gpr_b1;
	int insnsz, error = 0, execed = 0, first_exec = 1;

	if (STAILQ_FIRST(qabi_list) == NULL)
		return 0;

	alloc_ninst = min(USTORE_SIZE, nqabi);
	ucode = qat_alloc_mem(sizeof(uint64_t) * alloc_ninst);

	ninst = 0;
	STAILQ_FOREACH(qabi, qabi_list, qabi_next) {
		insnsz = qat_ae_get_inst_num(qabi->qabi_size);
		if (insnsz + ninst > alloc_ninst) {
			/* add ctx_arb[kill] */
			ucode[ninst++] = 0x0E000010000ull;
			execed = 1;

			error = qat_ae_exec_ucode_init_lm(sc, ae, 0,
			    &first_exec, ucode, ninst,
			    &gpr_a0, &gpr_a1, &gpr_a2, &gpr_b0, &gpr_b1);
			if (error) {
				qat_ae_restore_init_lm_gprs(sc, ae, 0,
				    gpr_a0, gpr_a1, gpr_a2, gpr_b0, gpr_b1);
				qat_free_mem(ucode);
				return error;
			}
			/* run microExec to execute the microcode */
			ninst = 0;
		}
		ninst += qat_ae_concat_ucode(ucode, ninst,
		    qabi->qabi_size, qabi->qabi_addr, qabi->qabi_value);
	}

	if (ninst > 0) {
		ucode[ninst++] = 0x0E000010000ull;
		execed = 1;

		error = qat_ae_exec_ucode_init_lm(sc, ae, 0,
		    &first_exec, ucode, ninst,
		    &gpr_a0, &gpr_a1, &gpr_a2, &gpr_b0, &gpr_b1);
	}
	if (execed) {
		qat_ae_restore_init_lm_gprs(sc, ae, 0,
		    gpr_a0, gpr_a1, gpr_a2, gpr_b0, gpr_b1);
	}

	qat_free_mem(ucode);

	return error;
}

static int
qat_ae_write_pc(struct qat_softc *sc, u_char ae, u_int ctx_mask, u_int upc)
{

	if (qat_ae_is_active(sc, ae))
		return EBUSY;

	qat_ae_ctx_indr_write(sc, ae, ctx_mask, CTX_STS_INDIRECT,
	    UPC_MASK & upc);
	return 0;
}

static inline u_int
qat_aefw_csum_calc(u_int reg, int ch)
{
	int i;
	u_int topbit = CRC_BITMASK(CRC_WIDTH - 1);
	u_int inbyte = (u_int)((reg >> 0x18) ^ ch);

	reg ^= inbyte << (CRC_WIDTH - 0x8);
	for (i = 0; i < 0x8; i++) {
		if (reg & topbit)
			reg = (reg << 1) ^ CRC_POLY;
		else
			reg <<= 1;
	}

	return (reg & CRC_WIDTHMASK(CRC_WIDTH));
}

static u_int
qat_aefw_csum(char *buf, int size)
{
	u_int csum = 0;

	while (size--) {
		csum = qat_aefw_csum_calc(csum, *buf++);
	}

	return csum;
}

static const char *
qat_aefw_uof_string(struct qat_softc *sc, size_t offset)
{
	if (offset >= sc->sc_aefw_uof.qafu_str_tab_size)
		return NULL;
	if (sc->sc_aefw_uof.qafu_str_tab == NULL)
		return NULL;

	return (const char *)((uintptr_t)sc->sc_aefw_uof.qafu_str_tab + offset);
}

static struct uof_chunk_hdr *
qat_aefw_uof_find_chunk(struct qat_softc *sc,
	const char *id, struct uof_chunk_hdr *cur)
{
	struct uof_obj_hdr *uoh = sc->sc_aefw_uof.qafu_obj_hdr;
	struct uof_chunk_hdr *uch;
	int i;

	uch = (struct uof_chunk_hdr *)(uoh + 1);
	for (i = 0; i < uoh->uoh_num_chunks; i++, uch++) {
		if (uch->uch_offset + uch->uch_size > sc->sc_aefw_uof.qafu_size)
			return NULL;

		if (cur < uch && !strncmp(uch->uch_id, id, UOF_OBJ_ID_LEN))
			return uch;
	}

	return NULL;
}

static int
qat_aefw_load_mof(struct qat_softc *sc)
{
	const struct firmware *fw;

	fw = firmware_get(sc->sc_hw.qhw_mof_fwname);
	if (fw == NULL) {
		device_printf(sc->sc_dev, "couldn't load MOF firmware %s\n",
		    sc->sc_hw.qhw_mof_fwname);
		return ENXIO;
	}

	sc->sc_fw_mof = qat_alloc_mem(fw->datasize);
	sc->sc_fw_mof_size = fw->datasize;
	memcpy(sc->sc_fw_mof, fw->data, fw->datasize);
	firmware_put(fw, FIRMWARE_UNLOAD);
	return 0;
}

static void
qat_aefw_unload_mof(struct qat_softc *sc)
{
	if (sc->sc_fw_mof != NULL) {
		qat_free_mem(sc->sc_fw_mof);
		sc->sc_fw_mof = NULL;
	}
}

static int
qat_aefw_load_mmp(struct qat_softc *sc)
{
	const struct firmware *fw;

	fw = firmware_get(sc->sc_hw.qhw_mmp_fwname);
	if (fw == NULL) {
		device_printf(sc->sc_dev, "couldn't load MOF firmware %s\n",
		    sc->sc_hw.qhw_mmp_fwname);
		return ENXIO;
	}

	sc->sc_fw_mmp = qat_alloc_mem(fw->datasize);
	sc->sc_fw_mmp_size = fw->datasize;
	memcpy(sc->sc_fw_mmp, fw->data, fw->datasize);
	firmware_put(fw, FIRMWARE_UNLOAD);
	return 0;
}

static void
qat_aefw_unload_mmp(struct qat_softc *sc)
{
	if (sc->sc_fw_mmp != NULL) {
		qat_free_mem(sc->sc_fw_mmp);
		sc->sc_fw_mmp = NULL;
	}
}

static int
qat_aefw_mof_find_uof0(struct qat_softc *sc,
	struct mof_uof_hdr *muh, struct mof_uof_chunk_hdr *head,
	u_int nchunk, size_t size, const char *id,
	size_t *fwsize, void **fwptr)
{
	int i;
	char *uof_name;

	for (i = 0; i < nchunk; i++) {
		struct mof_uof_chunk_hdr *much = &head[i];

		if (strncmp(much->much_id, id, MOF_OBJ_ID_LEN))
			return EINVAL;

		if (much->much_offset + much->much_size > size)
			return EINVAL;

		if (sc->sc_mof.qmf_sym_size <= much->much_name)
			return EINVAL;

		uof_name = (char *)((uintptr_t)sc->sc_mof.qmf_sym +
		    much->much_name);

		if (!strcmp(uof_name, sc->sc_fw_uof_name)) {
			*fwptr = (void *)((uintptr_t)muh +
			    (uintptr_t)much->much_offset);
			*fwsize = (size_t)much->much_size;
			return 0;
		}
	}

	return ENOENT;
}

static int
qat_aefw_mof_find_uof(struct qat_softc *sc)
{
	struct mof_uof_hdr *uof_hdr, *suof_hdr;
	u_int nuof_chunks = 0, nsuof_chunks = 0;
	int error;

	uof_hdr = sc->sc_mof.qmf_uof_objs;
	suof_hdr = sc->sc_mof.qmf_suof_objs;

	if (uof_hdr != NULL) {
		if (uof_hdr->muh_max_chunks < uof_hdr->muh_num_chunks) {
			return EINVAL;
		}
		nuof_chunks = uof_hdr->muh_num_chunks;
	}
	if (suof_hdr != NULL) {
		if (suof_hdr->muh_max_chunks < suof_hdr->muh_num_chunks)
			return EINVAL;
		nsuof_chunks = suof_hdr->muh_num_chunks;
	}

	if (nuof_chunks + nsuof_chunks == 0)
		return EINVAL;

	if (uof_hdr != NULL) {
		error = qat_aefw_mof_find_uof0(sc, uof_hdr,
		    (struct mof_uof_chunk_hdr *)(uof_hdr + 1), nuof_chunks,
		    sc->sc_mof.qmf_uof_objs_size, UOF_IMAG,
		    &sc->sc_fw_uof_size, &sc->sc_fw_uof);
		if (error && error != ENOENT)
			return error;
	}

	if (suof_hdr != NULL) {
		error = qat_aefw_mof_find_uof0(sc, suof_hdr,
		    (struct mof_uof_chunk_hdr *)(suof_hdr + 1), nsuof_chunks,
		    sc->sc_mof.qmf_suof_objs_size, SUOF_IMAG,
		    &sc->sc_fw_suof_size, &sc->sc_fw_suof);
		if (error && error != ENOENT)
			return error;
	}

	if (sc->sc_fw_uof == NULL && sc->sc_fw_suof == NULL)
		return ENOENT;

	return 0;
}

static int
qat_aefw_mof_parse(struct qat_softc *sc)
{
	const struct mof_file_hdr *mfh;
	const struct mof_file_chunk_hdr *mfch;
	size_t size;
	u_int csum;
	int error, i;

	size = sc->sc_fw_mof_size;

	if (size < sizeof(struct mof_file_hdr))
		return EINVAL;
	size -= sizeof(struct mof_file_hdr);

	mfh = sc->sc_fw_mof;

	if (mfh->mfh_fid != MOF_FID)
		return EINVAL;

	csum = qat_aefw_csum((char *)((uintptr_t)sc->sc_fw_mof +
	    offsetof(struct mof_file_hdr, mfh_min_ver)),
	    sc->sc_fw_mof_size -
	    offsetof(struct mof_file_hdr, mfh_min_ver));
	if (mfh->mfh_csum != csum)
		return EINVAL;

	if (mfh->mfh_min_ver != MOF_MIN_VER ||
	    mfh->mfh_maj_ver != MOF_MAJ_VER)
		return EINVAL;

	if (mfh->mfh_max_chunks < mfh->mfh_num_chunks)
		return EINVAL;

	if (size < sizeof(struct mof_file_chunk_hdr) * mfh->mfh_num_chunks)
		return EINVAL;
	mfch = (const struct mof_file_chunk_hdr *)(mfh + 1);

	for (i = 0; i < mfh->mfh_num_chunks; i++, mfch++) {
		if (mfch->mfch_offset + mfch->mfch_size > sc->sc_fw_mof_size)
			return EINVAL;

		if (!strncmp(mfch->mfch_id, SYM_OBJS, MOF_OBJ_ID_LEN)) {
			if (sc->sc_mof.qmf_sym != NULL)
				return EINVAL;

			sc->sc_mof.qmf_sym =
			    (void *)((uintptr_t)sc->sc_fw_mof +
			    (uintptr_t)mfch->mfch_offset + sizeof(u_int));
			sc->sc_mof.qmf_sym_size =
			    *(u_int *)((uintptr_t)sc->sc_fw_mof +
			    (uintptr_t)mfch->mfch_offset);

			if (sc->sc_mof.qmf_sym_size % sizeof(u_int) != 0)
				return EINVAL;
			if (mfch->mfch_size != sc->sc_mof.qmf_sym_size +
			    sizeof(u_int) || mfch->mfch_size == 0)
				return EINVAL;
			if (*(char *)((uintptr_t)sc->sc_mof.qmf_sym +
			    sc->sc_mof.qmf_sym_size - 1) != '\0')
				return EINVAL;

		} else if (!strncmp(mfch->mfch_id, UOF_OBJS, MOF_OBJ_ID_LEN)) {
			if (sc->sc_mof.qmf_uof_objs != NULL)
				return EINVAL;

			sc->sc_mof.qmf_uof_objs =
			    (void *)((uintptr_t)sc->sc_fw_mof +
			    (uintptr_t)mfch->mfch_offset);
			sc->sc_mof.qmf_uof_objs_size = mfch->mfch_size;

		} else if (!strncmp(mfch->mfch_id, SUOF_OBJS, MOF_OBJ_ID_LEN)) {
			if (sc->sc_mof.qmf_suof_objs != NULL)
				return EINVAL;

			sc->sc_mof.qmf_suof_objs =
			    (void *)((uintptr_t)sc->sc_fw_mof +
			    (uintptr_t)mfch->mfch_offset);
			sc->sc_mof.qmf_suof_objs_size = mfch->mfch_size;
		}
	}

	if (sc->sc_mof.qmf_sym == NULL ||
	    (sc->sc_mof.qmf_uof_objs == NULL &&
	    sc->sc_mof.qmf_suof_objs == NULL))
		return EINVAL;

	error = qat_aefw_mof_find_uof(sc);
	if (error)
		return error;
	return 0;
}

static int
qat_aefw_uof_parse_image(struct qat_softc *sc,
	struct qat_uof_image *qui, struct uof_chunk_hdr *uch)
{
	struct uof_image *image;
	struct uof_code_page *page;
	uintptr_t base = (uintptr_t)sc->sc_aefw_uof.qafu_obj_hdr;
	size_t lim = uch->uch_offset + uch->uch_size, size;
	int i, p;

	size = uch->uch_size;
	if (size < sizeof(struct uof_image))
		return EINVAL;
	size -= sizeof(struct uof_image);

	qui->qui_image = image =
	    (struct uof_image *)(base + uch->uch_offset);

#define ASSIGN_OBJ_TAB(np, typep, type, base, off, lim)			\
do {									\
	u_int nent;							\
	nent = ((struct uof_obj_table *)((base) + (off)))->uot_nentries;\
	if ((lim) < off + sizeof(struct uof_obj_table) +		\
	    sizeof(type) * nent)					\
		return EINVAL;						\
	*(np) = nent;							\
	if (nent > 0)							\
		*(typep) = (type)((struct uof_obj_table *)		\
		    ((base) + (off)) + 1);				\
	else								\
		*(typep) = NULL;					\
} while (0)

	ASSIGN_OBJ_TAB(&qui->qui_num_ae_reg, &qui->qui_ae_reg,
	    struct uof_ae_reg *, base, image->ui_reg_tab, lim);
	ASSIGN_OBJ_TAB(&qui->qui_num_init_reg_sym, &qui->qui_init_reg_sym,
	    struct uof_init_reg_sym *, base, image->ui_init_reg_sym_tab, lim);
	ASSIGN_OBJ_TAB(&qui->qui_num_sbreak, &qui->qui_sbreak,
	    struct qui_sbreak *, base, image->ui_sbreak_tab, lim);

	if (size < sizeof(struct uof_code_page) * image->ui_num_pages)
		return EINVAL;
	if (nitems(qui->qui_pages) < image->ui_num_pages)
		return EINVAL;

	page = (struct uof_code_page *)(image + 1);

	for (p = 0; p < image->ui_num_pages; p++, page++) {
		struct qat_uof_page *qup = &qui->qui_pages[p];
		struct uof_code_area *uca;

		qup->qup_page_num = page->ucp_page_num;
		qup->qup_def_page = page->ucp_def_page;
		qup->qup_page_region = page->ucp_page_region;
		qup->qup_beg_vaddr = page->ucp_beg_vaddr;
		qup->qup_beg_paddr = page->ucp_beg_paddr;

		ASSIGN_OBJ_TAB(&qup->qup_num_uc_var, &qup->qup_uc_var,
		    struct uof_uword_fixup *, base,
		    page->ucp_uc_var_tab, lim);
		ASSIGN_OBJ_TAB(&qup->qup_num_imp_var, &qup->qup_imp_var,
		    struct uof_import_var *, base,
		    page->ucp_imp_var_tab, lim);
		ASSIGN_OBJ_TAB(&qup->qup_num_imp_expr, &qup->qup_imp_expr,
		    struct uof_uword_fixup *, base,
		    page->ucp_imp_expr_tab, lim);
		ASSIGN_OBJ_TAB(&qup->qup_num_neigh_reg, &qup->qup_neigh_reg,
		    struct uof_uword_fixup *, base,
		    page->ucp_neigh_reg_tab, lim);

		if (lim < page->ucp_code_area + sizeof(struct uof_code_area))
			return EINVAL;

		uca = (struct uof_code_area *)(base + page->ucp_code_area);
		qup->qup_num_micro_words = uca->uca_num_micro_words;

		ASSIGN_OBJ_TAB(&qup->qup_num_uw_blocks, &qup->qup_uw_blocks,
		    struct qat_uof_uword_block *, base,
		    uca->uca_uword_block_tab, lim);

		for (i = 0; i < qup->qup_num_uw_blocks; i++) {
			u_int uwordoff = ((struct uof_uword_block *)(
			    &qup->qup_uw_blocks[i]))->uub_uword_offset;

			if (lim < uwordoff)
				return EINVAL;

			qup->qup_uw_blocks[i].quub_micro_words =
			    (base + uwordoff);
		}
	}

#undef ASSIGN_OBJ_TAB

	return 0;
}

static int
qat_aefw_uof_parse_images(struct qat_softc *sc)
{
	struct uof_chunk_hdr *uch = NULL;
	u_int assigned_ae;
	int i, error;

	for (i = 0; i < MAX_NUM_AE * MAX_AE_CTX; i++) {
		uch = qat_aefw_uof_find_chunk(sc, UOF_IMAG, uch);
		if (uch == NULL)
			break;

		if (i >= nitems(sc->sc_aefw_uof.qafu_imgs))
			return ENOENT;

		error = qat_aefw_uof_parse_image(sc, &sc->sc_aefw_uof.qafu_imgs[i], uch);
		if (error)
			return error;

		sc->sc_aefw_uof.qafu_num_imgs++;
	}

	assigned_ae = 0;
	for (i = 0; i < sc->sc_aefw_uof.qafu_num_imgs; i++) {
		assigned_ae |= sc->sc_aefw_uof.qafu_imgs[i].qui_image->ui_ae_assigned;
	}

	return 0;
}

static int
qat_aefw_uof_parse(struct qat_softc *sc)
{
	struct uof_file_hdr *ufh;
	struct uof_file_chunk_hdr *ufch;
	struct uof_obj_hdr *uoh;
	struct uof_chunk_hdr *uch;
	void *uof = NULL;
	size_t size, uof_size, hdr_size;
	uintptr_t base;
	u_int csum;
	int i;

	size = sc->sc_fw_uof_size;
	if (size < MIN_UOF_SIZE)
		return EINVAL;
	size -= sizeof(struct uof_file_hdr);

	ufh = sc->sc_fw_uof;

	if (ufh->ufh_id != UOF_FID)
		return EINVAL;
	if (ufh->ufh_min_ver != UOF_MIN_VER || ufh->ufh_maj_ver != UOF_MAJ_VER)
		return EINVAL;

	if (ufh->ufh_max_chunks < ufh->ufh_num_chunks)
		return EINVAL;
	if (size < sizeof(struct uof_file_chunk_hdr) * ufh->ufh_num_chunks)
		return EINVAL;
	ufch = (struct uof_file_chunk_hdr *)(ufh + 1);

	uof_size = 0;
	for (i = 0; i < ufh->ufh_num_chunks; i++, ufch++) {
		if (ufch->ufch_offset + ufch->ufch_size > sc->sc_fw_uof_size)
			return EINVAL;

		if (!strncmp(ufch->ufch_id, UOF_OBJS, UOF_OBJ_ID_LEN)) {
			if (uof != NULL)
				return EINVAL;

			uof =
			    (void *)((uintptr_t)sc->sc_fw_uof +
			    ufch->ufch_offset);
			uof_size = ufch->ufch_size;

			csum = qat_aefw_csum(uof, uof_size);
			if (csum != ufch->ufch_csum)
				return EINVAL;
		}
	}

	if (uof == NULL)
		return ENOENT;

	size = uof_size;
	if (size < sizeof(struct uof_obj_hdr))
		return EINVAL;
	size -= sizeof(struct uof_obj_hdr);

	uoh = uof;

	if (size < sizeof(struct uof_chunk_hdr) * uoh->uoh_num_chunks)
		return EINVAL;

	/* Check if the UOF objects are compatible with the chip */
	if ((uoh->uoh_cpu_type & sc->sc_hw.qhw_prod_type) == 0)
		return ENOTSUP;

	if (uoh->uoh_min_cpu_ver > sc->sc_rev ||
	    uoh->uoh_max_cpu_ver < sc->sc_rev)
		return ENOTSUP;

	sc->sc_aefw_uof.qafu_size = uof_size;
	sc->sc_aefw_uof.qafu_obj_hdr = uoh;

	base = (uintptr_t)sc->sc_aefw_uof.qafu_obj_hdr;

	/* map uof string-table */
	uch = qat_aefw_uof_find_chunk(sc, UOF_STRT, NULL);
	if (uch != NULL) {
		hdr_size = offsetof(struct uof_str_tab, ust_strings);
		sc->sc_aefw_uof.qafu_str_tab =
		    (void *)(base + uch->uch_offset + hdr_size);
		sc->sc_aefw_uof.qafu_str_tab_size = uch->uch_size - hdr_size;
	}

	/* get ustore mem inits table -- should be only one */
	uch = qat_aefw_uof_find_chunk(sc, UOF_IMEM, NULL);
	if (uch != NULL) {
		if (uch->uch_size < sizeof(struct uof_obj_table))
			return EINVAL;
		sc->sc_aefw_uof.qafu_num_init_mem = ((struct uof_obj_table *)(base +
		    uch->uch_offset))->uot_nentries;
		if (sc->sc_aefw_uof.qafu_num_init_mem) {
			sc->sc_aefw_uof.qafu_init_mem =
			    (struct uof_init_mem *)(base + uch->uch_offset +
			    sizeof(struct uof_obj_table));
			sc->sc_aefw_uof.qafu_init_mem_size =
			    uch->uch_size - sizeof(struct uof_obj_table);
		}
	}

	uch = qat_aefw_uof_find_chunk(sc, UOF_MSEG, NULL);
	if (uch != NULL) {
		if (uch->uch_size < sizeof(struct uof_obj_table) +
		    sizeof(struct uof_var_mem_seg))
			return EINVAL;
		sc->sc_aefw_uof.qafu_var_mem_seg =
		    (struct uof_var_mem_seg *)(base + uch->uch_offset +
		    sizeof(struct uof_obj_table));
	}

	return qat_aefw_uof_parse_images(sc);
}

static int
qat_aefw_suof_parse_image(struct qat_softc *sc, struct qat_suof_image *qsi,
    struct suof_chunk_hdr *sch)
{
	struct qat_aefw_suof *qafs = &sc->sc_aefw_suof;
	struct simg_ae_mode *ae_mode;
	u_int maj_ver;

	qsi->qsi_simg_buf = qafs->qafs_suof_buf + sch->sch_offset +
	    sizeof(struct suof_obj_hdr);
	qsi->qsi_simg_len =
	    ((struct suof_obj_hdr *)
	    (qafs->qafs_suof_buf + sch->sch_offset))->soh_img_length;

	qsi->qsi_css_header = qsi->qsi_simg_buf;
	qsi->qsi_css_key = qsi->qsi_css_header + sizeof(struct css_hdr);
	qsi->qsi_css_signature = qsi->qsi_css_key +
	    CSS_FWSK_MODULUS_LEN + CSS_FWSK_EXPONENT_LEN;
	qsi->qsi_css_simg = qsi->qsi_css_signature + CSS_SIGNATURE_LEN;

	ae_mode = (struct simg_ae_mode *)qsi->qsi_css_simg;
	qsi->qsi_ae_mask = ae_mode->sam_ae_mask;
	qsi->qsi_simg_name = (u_long)&ae_mode->sam_simg_name;
	qsi->qsi_appmeta_data = (u_long)&ae_mode->sam_appmeta_data;
	qsi->qsi_fw_type = ae_mode->sam_fw_type;

	if (ae_mode->sam_dev_type != sc->sc_hw.qhw_prod_type)
		return EINVAL;

	maj_ver = (QAT_PID_MAJOR_REV | (sc->sc_rev & QAT_PID_MINOR_REV)) & 0xff;
	if ((maj_ver > ae_mode->sam_devmax_ver) ||
	    (maj_ver < ae_mode->sam_devmin_ver)) {
		return EINVAL;
	}

	return 0;
}

static int
qat_aefw_suof_parse(struct qat_softc *sc)
{
	struct suof_file_hdr *sfh;
	struct suof_chunk_hdr *sch;
	struct qat_aefw_suof *qafs = &sc->sc_aefw_suof;
	struct qat_suof_image *qsi;
	size_t size;
	u_int csum;
	int ae0_img = MAX_AE;
	int i, error;

	size = sc->sc_fw_suof_size;
	if (size < sizeof(struct suof_file_hdr))
		return EINVAL;

	sfh = sc->sc_fw_suof;

	if (sfh->sfh_file_id != SUOF_FID)
		return EINVAL;
	if (sfh->sfh_fw_type != 0)
		return EINVAL;
	if (sfh->sfh_num_chunks <= 1)
		return EINVAL;
	if (sfh->sfh_min_ver != SUOF_MIN_VER ||
	    sfh->sfh_maj_ver != SUOF_MAJ_VER)
		return EINVAL;

	csum = qat_aefw_csum((char *)&sfh->sfh_min_ver,
	    size - offsetof(struct suof_file_hdr, sfh_min_ver));
	if (csum != sfh->sfh_check_sum)
		return EINVAL;

	size -= sizeof(struct suof_file_hdr);

	qafs->qafs_file_id = SUOF_FID;
	qafs->qafs_suof_buf = sc->sc_fw_suof;
	qafs->qafs_suof_size = sc->sc_fw_suof_size;
	qafs->qafs_check_sum = sfh->sfh_check_sum;
	qafs->qafs_min_ver = sfh->sfh_min_ver;
	qafs->qafs_maj_ver = sfh->sfh_maj_ver;
	qafs->qafs_fw_type = sfh->sfh_fw_type;

	if (size < sizeof(struct suof_chunk_hdr))
		return EINVAL;
	sch = (struct suof_chunk_hdr *)(sfh + 1);
	size -= sizeof(struct suof_chunk_hdr);

	if (size < sizeof(struct suof_str_tab))
		return EINVAL;
	size -= offsetof(struct suof_str_tab, sst_strings);

	qafs->qafs_sym_size = ((struct suof_str_tab *)
	    (qafs->qafs_suof_buf + sch->sch_offset))->sst_tab_length;
	if (size < qafs->qafs_sym_size)
		return EINVAL;
	qafs->qafs_sym_str = qafs->qafs_suof_buf + sch->sch_offset +
	    offsetof(struct suof_str_tab, sst_strings);

	qafs->qafs_num_simgs = sfh->sfh_num_chunks - 1;
	if (qafs->qafs_num_simgs == 0)
		return EINVAL;

	qsi = qat_alloc_mem(
	    sizeof(struct qat_suof_image) * qafs->qafs_num_simgs);
	qafs->qafs_simg = qsi;

	for (i = 0; i < qafs->qafs_num_simgs; i++) {
		error = qat_aefw_suof_parse_image(sc, &qsi[i], &sch[i + 1]);
		if (error)
			return error;
		if ((qsi[i].qsi_ae_mask & 0x1) != 0)
			ae0_img = i;
	}

	if (ae0_img != qafs->qafs_num_simgs - 1) {
		struct qat_suof_image last_qsi;

		memcpy(&last_qsi, &qsi[qafs->qafs_num_simgs - 1],
		    sizeof(struct qat_suof_image));
		memcpy(&qsi[qafs->qafs_num_simgs - 1], &qsi[ae0_img],
		    sizeof(struct qat_suof_image));
		memcpy(&qsi[ae0_img], &last_qsi,
		    sizeof(struct qat_suof_image));
	}

	return 0;
}

static int
qat_aefw_alloc_auth_dmamem(struct qat_softc *sc, char *image, size_t size,
    struct qat_dmamem *dma)
{
	struct css_hdr *css = (struct css_hdr *)image;
	struct auth_chunk *auth_chunk;
	struct fw_auth_desc *auth_desc;
	size_t mapsize, simg_offset = sizeof(struct auth_chunk);
	bus_size_t bus_addr;
	uintptr_t virt_addr;
	int error;

	if (size > AE_IMG_OFFSET + CSS_MAX_IMAGE_LEN)
		return EINVAL;

	mapsize = (css->css_fw_type == CSS_AE_FIRMWARE) ?
	    CSS_AE_SIMG_LEN + simg_offset :
	    size + CSS_FWSK_PAD_LEN + simg_offset;
	error = qat_alloc_dmamem(sc, dma, 1, mapsize, PAGE_SIZE);
	if (error)
		return error;

	memset(dma->qdm_dma_vaddr, 0, mapsize);

	auth_chunk = dma->qdm_dma_vaddr;
	auth_chunk->ac_chunk_size = mapsize;
	auth_chunk->ac_chunk_bus_addr = dma->qdm_dma_seg.ds_addr;

	virt_addr = (uintptr_t)dma->qdm_dma_vaddr;
	virt_addr += simg_offset;
	bus_addr = auth_chunk->ac_chunk_bus_addr;
	bus_addr += simg_offset;

	auth_desc = &auth_chunk->ac_fw_auth_desc;
	auth_desc->fad_css_hdr_high = (uint64_t)bus_addr >> 32;
	auth_desc->fad_css_hdr_low = bus_addr;

	memcpy((void *)virt_addr, image, sizeof(struct css_hdr));
	/* pub key */
	virt_addr += sizeof(struct css_hdr);
	bus_addr += sizeof(struct css_hdr);
	image += sizeof(struct css_hdr);

	auth_desc->fad_fwsk_pub_high = (uint64_t)bus_addr >> 32;
	auth_desc->fad_fwsk_pub_low = bus_addr;

	memcpy((void *)virt_addr, image, CSS_FWSK_MODULUS_LEN);
	memset((void *)(virt_addr + CSS_FWSK_MODULUS_LEN), 0, CSS_FWSK_PAD_LEN);
	memcpy((void *)(virt_addr + CSS_FWSK_MODULUS_LEN + CSS_FWSK_PAD_LEN),
	    image + CSS_FWSK_MODULUS_LEN, sizeof(uint32_t));

	virt_addr += CSS_FWSK_PUB_LEN;
	bus_addr += CSS_FWSK_PUB_LEN;
	image += CSS_FWSK_MODULUS_LEN + CSS_FWSK_EXPONENT_LEN;

	auth_desc->fad_signature_high = (uint64_t)bus_addr >> 32;
	auth_desc->fad_signature_low = bus_addr;

	memcpy((void *)virt_addr, image, CSS_SIGNATURE_LEN);

	virt_addr += CSS_SIGNATURE_LEN;
	bus_addr += CSS_SIGNATURE_LEN;
	image += CSS_SIGNATURE_LEN;

	auth_desc->fad_img_high = (uint64_t)bus_addr >> 32;
	auth_desc->fad_img_low = bus_addr;
	auth_desc->fad_img_len = size - AE_IMG_OFFSET;

	memcpy((void *)virt_addr, image, auth_desc->fad_img_len);

	if (css->css_fw_type == CSS_AE_FIRMWARE) {
		auth_desc->fad_img_ae_mode_data_high = auth_desc->fad_img_high;
		auth_desc->fad_img_ae_mode_data_low = auth_desc->fad_img_low;

		bus_addr += sizeof(struct simg_ae_mode);

		auth_desc->fad_img_ae_init_data_high = (uint64_t)bus_addr >> 32;
		auth_desc->fad_img_ae_init_data_low = bus_addr;

		bus_addr += SIMG_AE_INIT_SEQ_LEN;

		auth_desc->fad_img_ae_insts_high = (uint64_t)bus_addr >> 32;
		auth_desc->fad_img_ae_insts_low = bus_addr;
	} else {
		auth_desc->fad_img_ae_insts_high = auth_desc->fad_img_high;
		auth_desc->fad_img_ae_insts_low = auth_desc->fad_img_low;
	}

	bus_dmamap_sync(dma->qdm_dma_tag, dma->qdm_dma_map,
	    BUS_DMASYNC_PREWRITE | BUS_DMASYNC_PREREAD);

	return 0;
}

static int
qat_aefw_auth(struct qat_softc *sc, struct qat_dmamem *dma)
{
	bus_addr_t addr;
	uint32_t fcu, sts;
	int retry = 0;

	addr = dma->qdm_dma_seg.ds_addr;
	qat_cap_global_write_4(sc, FCU_DRAM_ADDR_HI, (uint64_t)addr >> 32);
	qat_cap_global_write_4(sc, FCU_DRAM_ADDR_LO, addr);
	qat_cap_global_write_4(sc, FCU_CTRL, FCU_CTRL_CMD_AUTH);

	do {
		DELAY(FW_AUTH_WAIT_PERIOD * 1000);
		fcu = qat_cap_global_read_4(sc, FCU_STATUS);
		sts = __SHIFTOUT(fcu, FCU_STATUS_STS);
		if (sts == FCU_STATUS_STS_VERI_FAIL)
			goto fail;
		if (fcu & FCU_STATUS_AUTHFWLD &&
		    sts == FCU_STATUS_STS_VERI_DONE) {
			return 0;
		}
	} while (retry++ < FW_AUTH_MAX_RETRY);

fail:
	device_printf(sc->sc_dev,
	   "firmware authentication error: status 0x%08x retry %d\n",
	   fcu, retry);
	return EINVAL;
}

static int
qat_aefw_suof_load(struct qat_softc *sc, struct qat_dmamem *dma)
{
	struct simg_ae_mode *ae_mode;
	uint32_t fcu, sts, loaded;
	u_int mask;
	u_char ae;
	int retry = 0;

	ae_mode = (struct simg_ae_mode *)((uintptr_t)dma->qdm_dma_vaddr +
	    sizeof(struct auth_chunk) + sizeof(struct css_hdr) +
	    CSS_FWSK_PUB_LEN + CSS_SIGNATURE_LEN);

	for (ae = 0, mask = sc->sc_ae_mask; mask; ae++, mask >>= 1) {
		if (!(mask & 1))
			continue;
		if (!((ae_mode->sam_ae_mask >> ae) & 0x1))
			continue;
		if (qat_ae_is_active(sc, ae)) {
			device_printf(sc->sc_dev, "AE %d is active\n", ae);
			return EINVAL;
		}
		qat_cap_global_write_4(sc, FCU_CTRL,
		    FCU_CTRL_CMD_LOAD | __SHIFTIN(ae, FCU_CTRL_AE));
		do {
			DELAY(FW_AUTH_WAIT_PERIOD * 1000);
			fcu = qat_cap_global_read_4(sc, FCU_STATUS);
			sts = __SHIFTOUT(fcu, FCU_STATUS_STS);
			loaded = __SHIFTOUT(fcu, FCU_STATUS_LOADED_AE);
			if (sts == FCU_STATUS_STS_LOAD_DONE &&
			    (loaded & (1 << ae))) {
				break;
			}
		} while (retry++ < FW_AUTH_MAX_RETRY);

		if (retry > FW_AUTH_MAX_RETRY) {
			device_printf(sc->sc_dev,
			    "firmware load timeout: status %08x\n", fcu);
			return EINVAL;
		}
	}

	return 0;
}

static int
qat_aefw_suof_write(struct qat_softc *sc)
{
	struct qat_suof_image *qsi;
	int i, error = 0;

	for (i = 0; i < sc->sc_aefw_suof.qafs_num_simgs; i++) {
		qsi = &sc->sc_aefw_suof.qafs_simg[i];
		error = qat_aefw_alloc_auth_dmamem(sc, qsi->qsi_simg_buf,
		    qsi->qsi_simg_len, &qsi->qsi_dma);
		if (error)
			return error;
		error = qat_aefw_auth(sc, &qsi->qsi_dma);
		if (error) {
			qat_free_dmamem(sc, &qsi->qsi_dma);
			return error;
		}
		error = qat_aefw_suof_load(sc, &qsi->qsi_dma);
		if (error) {
			qat_free_dmamem(sc, &qsi->qsi_dma);
			return error;
		}
		qat_free_dmamem(sc, &qsi->qsi_dma);
	}
	qat_free_mem(sc->sc_aefw_suof.qafs_simg);

	return 0;
}

static int
qat_aefw_uof_assign_image(struct qat_softc *sc, struct qat_ae *qae,
	struct qat_uof_image *qui)
{
	struct qat_ae_slice *slice;
	int i, npages, nregions;

	if (qae->qae_num_slices >= nitems(qae->qae_slices))
		return ENOENT;

	if (qui->qui_image->ui_ae_mode &
	    (AE_MODE_RELOAD_CTX_SHARED | AE_MODE_SHARED_USTORE)) {
		/* XXX */
		device_printf(sc->sc_dev,
		    "shared ae mode is not supported yet\n");
		return ENOTSUP;
	}

	qae->qae_shareable_ustore = 0; /* XXX */
	qae->qae_effect_ustore_size = USTORE_SIZE;

	slice = &qae->qae_slices[qae->qae_num_slices];

	slice->qas_image = qui;
	slice->qas_assigned_ctx_mask = qui->qui_image->ui_ctx_assigned;

	nregions = qui->qui_image->ui_num_page_regions;
	npages = qui->qui_image->ui_num_pages;

	if (nregions > nitems(slice->qas_regions))
		return ENOENT;
	if (npages > nitems(slice->qas_pages))
		return ENOENT;

	for (i = 0; i < nregions; i++) {
		STAILQ_INIT(&slice->qas_regions[i].qar_waiting_pages);
	}
	for (i = 0; i < npages; i++) {
		struct qat_ae_page *page = &slice->qas_pages[i];
		int region;

		page->qap_page = &qui->qui_pages[i];
		region = page->qap_page->qup_page_region;
		if (region >= nregions)
			return EINVAL;

		page->qap_region = &slice->qas_regions[region];
	}

	qae->qae_num_slices++;

	return 0;
}

static int
qat_aefw_uof_init_ae(struct qat_softc *sc, u_char ae)
{
	struct uof_image *image;
	struct qat_ae *qae = &(QAT_AE(sc, ae));
	int s;
	u_char nn_mode;

	for (s = 0; s < qae->qae_num_slices; s++) {
		if (qae->qae_slices[s].qas_image == NULL)
			continue;

		image = qae->qae_slices[s].qas_image->qui_image;
		qat_ae_write_ctx_mode(sc, ae,
		    __SHIFTOUT(image->ui_ae_mode, AE_MODE_CTX_MODE));

		nn_mode = __SHIFTOUT(image->ui_ae_mode, AE_MODE_NN_MODE);
		if (nn_mode != AE_MODE_NN_MODE_DONTCARE)
			qat_ae_write_nn_mode(sc, ae, nn_mode);

		qat_ae_write_lm_mode(sc, ae, AEREG_LMEM0,
		    __SHIFTOUT(image->ui_ae_mode, AE_MODE_LMEM0));
		qat_ae_write_lm_mode(sc, ae, AEREG_LMEM1,
		    __SHIFTOUT(image->ui_ae_mode, AE_MODE_LMEM1));

		qat_ae_write_shared_cs_mode(sc, ae,
		    __SHIFTOUT(image->ui_ae_mode, AE_MODE_SHARED_USTORE));
		qat_ae_set_reload_ustore(sc, ae, image->ui_reloadable_size,
		    __SHIFTOUT(image->ui_ae_mode, AE_MODE_RELOAD_CTX_SHARED),
		    qae->qae_reloc_ustore_dram);
	}

	return 0;
}

static int
qat_aefw_uof_init(struct qat_softc *sc)
{
	int ae, i, error;
	uint32_t mask;

	for (ae = 0, mask = sc->sc_ae_mask; mask; ae++, mask >>= 1) {
		struct qat_ae *qae;

		if (!(mask & 1))
			continue;

		qae = &(QAT_AE(sc, ae));

		for (i = 0; i < sc->sc_aefw_uof.qafu_num_imgs; i++) {
			if ((sc->sc_aefw_uof.qafu_imgs[i].qui_image->ui_ae_assigned &
			    (1 << ae)) == 0)
				continue;

			error = qat_aefw_uof_assign_image(sc, qae,
			    &sc->sc_aefw_uof.qafu_imgs[i]);
			if (error)
				return error;
		}

		/* XXX UcLo_initNumUwordUsed */

		qae->qae_reloc_ustore_dram = UINT_MAX; /* XXX */

		error = qat_aefw_uof_init_ae(sc, ae);
		if (error)
			return error;
	}

	return 0;
}

int
qat_aefw_load(struct qat_softc *sc)
{
	int error;

	error = qat_aefw_load_mof(sc);
	if (error)
		return error;

	error = qat_aefw_load_mmp(sc);
	if (error)
		return error;

	error = qat_aefw_mof_parse(sc);
	if (error) {
		device_printf(sc->sc_dev, "couldn't parse mof: %d\n", error);
		return error;
	}

	if (sc->sc_hw.qhw_fw_auth) {
		error = qat_aefw_suof_parse(sc);
		if (error) {
			device_printf(sc->sc_dev, "couldn't parse suof: %d\n",
			    error);
			return error;
		}

		error = qat_aefw_suof_write(sc);
		if (error) {
			device_printf(sc->sc_dev,
			    "could not write firmware: %d\n", error);
			return error;
		}

	} else {
		error = qat_aefw_uof_parse(sc);
		if (error) {
			device_printf(sc->sc_dev, "couldn't parse uof: %d\n",
			    error);
			return error;
		}

		error = qat_aefw_uof_init(sc);
		if (error) {
			device_printf(sc->sc_dev,
			    "couldn't init for aefw: %d\n", error);
			return error;
		}

		error = qat_aefw_uof_write(sc);
		if (error) {
			device_printf(sc->sc_dev,
			    "Could not write firmware: %d\n", error);
			return error;
		}
	}

	return 0;
}

void
qat_aefw_unload(struct qat_softc *sc)
{
	qat_aefw_unload_mmp(sc);
	qat_aefw_unload_mof(sc);
}

int
qat_aefw_start(struct qat_softc *sc, u_char ae, u_int ctx_mask)
{
	uint32_t fcu;
	int retry = 0;

	if (sc->sc_hw.qhw_fw_auth) {
		qat_cap_global_write_4(sc, FCU_CTRL, FCU_CTRL_CMD_START);
		do {
			DELAY(FW_AUTH_WAIT_PERIOD * 1000);
			fcu = qat_cap_global_read_4(sc, FCU_STATUS);
			if (fcu & FCU_STATUS_DONE)
				return 0;
		} while (retry++ < FW_AUTH_MAX_RETRY);

		device_printf(sc->sc_dev,
		    "firmware start timeout: status %08x\n", fcu);
		return EINVAL;
	} else {
		qat_ae_ctx_indr_write(sc, ae, (~ctx_mask) & AE_ALL_CTX,
		    CTX_WAKEUP_EVENTS_INDIRECT,
		    CTX_WAKEUP_EVENTS_INDIRECT_SLEEP);
		qat_ae_enable_ctx(sc, ae, ctx_mask);
	}

	return 0;
}

static int
qat_aefw_init_memory_one(struct qat_softc *sc, struct uof_init_mem *uim)
{
	struct qat_aefw_uof *qafu = &sc->sc_aefw_uof;
	struct qat_ae_batch_init_list *qabi_list;
	struct uof_mem_val_attr *memattr;
	size_t *curinit;
	u_long ael;
	int i;
	const char *sym;
	char *ep;

	memattr = (struct uof_mem_val_attr *)(uim + 1);

	switch (uim->uim_region) {
	case LMEM_REGION:
		if ((uim->uim_addr + uim->uim_num_bytes) > MAX_LMEM_REG * 4) {
			device_printf(sc->sc_dev,
			    "Invalid lmem addr or bytes\n");
			return ENOBUFS;
		}
		if (uim->uim_scope != UOF_SCOPE_LOCAL)
			return EINVAL;
		sym = qat_aefw_uof_string(sc, uim->uim_sym_name);
		ael = strtoul(sym, &ep, 10);
		if (ep == sym || ael > MAX_AE)
			return EINVAL;
		if ((sc->sc_ae_mask & (1 << ael)) == 0)
			return 0; /* ae is fused out */

		curinit = &qafu->qafu_num_lm_init[ael];
		qabi_list = &qafu->qafu_lm_init[ael];

		for (i = 0; i < uim->uim_num_val_attr; i++, memattr++) {
			struct qat_ae_batch_init *qabi;

			qabi = qat_alloc_mem(sizeof(struct qat_ae_batch_init));
			if (*curinit == 0)
				STAILQ_INIT(qabi_list);
			STAILQ_INSERT_TAIL(qabi_list, qabi, qabi_next);

			qabi->qabi_ae = (u_int)ael;
			qabi->qabi_addr =
			    uim->uim_addr + memattr->umva_byte_offset;
			qabi->qabi_value = &memattr->umva_value;
			qabi->qabi_size = 4;
			qafu->qafu_num_lm_init_inst[ael] +=
			    qat_ae_get_inst_num(qabi->qabi_size);
			(*curinit)++;
			if (*curinit >= MAX_LMEM_REG) {
				device_printf(sc->sc_dev,
				    "Invalid lmem val attr\n");
				return ENOBUFS;
			}
		}
		break;
	case SRAM_REGION:
	case DRAM_REGION:
	case DRAM1_REGION:
	case SCRATCH_REGION:
	case UMEM_REGION:
		/* XXX */
		/* fallthrough */
	default:
		device_printf(sc->sc_dev,
		    "unsupported memory region to init: %d\n",
		    uim->uim_region);
		return ENOTSUP;
	}

	return 0;
}

static void
qat_aefw_free_lm_init(struct qat_softc *sc, u_char ae)
{
	struct qat_aefw_uof *qafu = &sc->sc_aefw_uof;
	struct qat_ae_batch_init *qabi;

	while ((qabi = STAILQ_FIRST(&qafu->qafu_lm_init[ae])) != NULL) {
		STAILQ_REMOVE_HEAD(&qafu->qafu_lm_init[ae], qabi_next);
		qat_free_mem(qabi);
	}

	qafu->qafu_num_lm_init[ae] = 0;
	qafu->qafu_num_lm_init_inst[ae] = 0;
}

static int
qat_aefw_init_ustore(struct qat_softc *sc)
{
	uint64_t *fill;
	uint32_t dont_init;
	int a, i, p;
	int error = 0;
	int usz, end, start;
	u_char ae, nae;

	fill = qat_alloc_mem(MAX_USTORE * sizeof(uint64_t));

	for (a = 0; a < sc->sc_aefw_uof.qafu_num_imgs; a++) {
		struct qat_uof_image *qui = &sc->sc_aefw_uof.qafu_imgs[a];
		struct uof_image *ui = qui->qui_image;

		for (i = 0; i < MAX_USTORE; i++)
			memcpy(&fill[i], ui->ui_fill_pattern, sizeof(uint64_t));
		/*
		 * Compute do_not_init value as a value that will not be equal
		 * to fill data when cast to an int
		 */
		dont_init = 0;
		if (dont_init == (uint32_t)fill[0])
			dont_init = 0xffffffff;

		for (p = 0; p < ui->ui_num_pages; p++) {
			struct qat_uof_page *qup = &qui->qui_pages[p];
			if (!qup->qup_def_page)
				continue;

			for (i = qup->qup_beg_paddr;
			    i < qup->qup_beg_paddr + qup->qup_num_micro_words;
			    i++ ) {
				fill[i] = (uint64_t)dont_init;
			}
		}

		for (ae = 0; ae < sc->sc_ae_num; ae++) {
			MPASS(ae < UOF_MAX_NUM_OF_AE);
			if ((ui->ui_ae_assigned & (1 << ae)) == 0)
				continue;

			if (QAT_AE(sc, ae).qae_shareable_ustore && (ae & 1)) {
				qat_ae_get_shared_ustore_ae(ae, &nae);
				if (ui->ui_ae_assigned & (1 << ae))
					continue;
			}
			usz = QAT_AE(sc, ae).qae_effect_ustore_size;

			/* initialize the areas not going to be overwritten */
			end = -1;
			do {
				/* find next uword that needs to be initialized */
				for (start = end + 1; start < usz; start++) {
					if ((uint32_t)fill[start] != dont_init)
						break;
				}
				/* see if there are no more such uwords */
				if (start >= usz)
					break;
				for (end = start + 1; end < usz; end++) {
					if ((uint32_t)fill[end] == dont_init)
						break;
				}
				if (QAT_AE(sc, ae).qae_shareable_ustore) {
					error = ENOTSUP; /* XXX */
					goto out;
				} else {
					error = qat_ae_ucode_write(sc, ae,
					    start, end - start, &fill[start]);
					if (error) {
						goto out;
					}
				}

			} while (end < usz);
		}
	}

out:
	qat_free_mem(fill);
	return error;
}

static int
qat_aefw_init_reg(struct qat_softc *sc, u_char ae, u_char ctx_mask,
    enum aereg_type regtype, u_short regaddr, u_int value)
{
	int error = 0;
	u_char ctx;

	switch (regtype) {
	case AEREG_GPA_REL:
	case AEREG_GPB_REL:
	case AEREG_SR_REL:
	case AEREG_SR_RD_REL:
	case AEREG_SR_WR_REL:
	case AEREG_DR_REL:
	case AEREG_DR_RD_REL:
	case AEREG_DR_WR_REL:
	case AEREG_NEIGH_REL:
		/* init for all valid ctx */
		for (ctx = 0; ctx < MAX_AE_CTX; ctx++) {
			if ((ctx_mask & (1 << ctx)) == 0)
				continue;
			error = qat_aereg_rel_data_write(sc, ae, ctx, regtype,
			    regaddr, value);
		}
		break;
	case AEREG_GPA_ABS:
	case AEREG_GPB_ABS:
	case AEREG_SR_ABS:
	case AEREG_SR_RD_ABS:
	case AEREG_SR_WR_ABS:
	case AEREG_DR_ABS:
	case AEREG_DR_RD_ABS:
	case AEREG_DR_WR_ABS:
		error = qat_aereg_abs_data_write(sc, ae, regtype,
		    regaddr, value);
		break;
	default:
		error = EINVAL;
		break;
	}

	return error;
}

static int
qat_aefw_init_reg_sym_expr(struct qat_softc *sc, u_char ae,
    struct qat_uof_image *qui)
{
	u_int i, expres;
	u_char ctx_mask;

	for (i = 0; i < qui->qui_num_init_reg_sym; i++) {
		struct uof_init_reg_sym *uirs = &qui->qui_init_reg_sym[i];

		if (uirs->uirs_value_type == EXPR_VAL) {
			/* XXX */
			device_printf(sc->sc_dev,
			    "does not support initializing EXPR_VAL\n");
			return ENOTSUP;
		} else {
			expres = uirs->uirs_value;
		}

		switch (uirs->uirs_init_type) {
		case INIT_REG:
			if (__SHIFTOUT(qui->qui_image->ui_ae_mode,
			    AE_MODE_CTX_MODE) == MAX_AE_CTX) {
				ctx_mask = 0xff; /* 8-ctx mode */
			} else {
				ctx_mask = 0x55; /* 4-ctx mode */
			}
			qat_aefw_init_reg(sc, ae, ctx_mask,
			    (enum aereg_type)uirs->uirs_reg_type,
			    (u_short)uirs->uirs_addr_offset, expres);
			break;
		case INIT_REG_CTX:
			if (__SHIFTOUT(qui->qui_image->ui_ae_mode,
			    AE_MODE_CTX_MODE) == MAX_AE_CTX) {
				ctx_mask = 0xff; /* 8-ctx mode */
			} else {
				ctx_mask = 0x55; /* 4-ctx mode */
			}
			if (((1 << uirs->uirs_ctx) & ctx_mask) == 0)
				return EINVAL;
			qat_aefw_init_reg(sc, ae, 1 << uirs->uirs_ctx,
			    (enum aereg_type)uirs->uirs_reg_type,
			    (u_short)uirs->uirs_addr_offset, expres);
			break;
		case INIT_EXPR:
		case INIT_EXPR_ENDIAN_SWAP:
		default:
			device_printf(sc->sc_dev,
			    "does not support initializing init_type %d\n",
			    uirs->uirs_init_type);
			return ENOTSUP;
		}
	}

	return 0;
}

static int
qat_aefw_init_memory(struct qat_softc *sc)
{
	struct qat_aefw_uof *qafu = &sc->sc_aefw_uof;
	size_t uimsz, initmemsz = qafu->qafu_init_mem_size;
	struct uof_init_mem *uim;
	int error, i;
	u_char ae;

	uim = qafu->qafu_init_mem;
	for (i = 0; i < qafu->qafu_num_init_mem; i++) {
		uimsz = sizeof(struct uof_init_mem) +
		    sizeof(struct uof_mem_val_attr) * uim->uim_num_val_attr;
		if (uimsz > initmemsz) {
			device_printf(sc->sc_dev,
			    "invalid uof_init_mem or uof_mem_val_attr size\n");
			return EINVAL;
		}

		if (uim->uim_num_bytes > 0) {
			error = qat_aefw_init_memory_one(sc, uim);
			if (error) {
				device_printf(sc->sc_dev,
				    "Could not init ae memory: %d\n", error);
				return error;
			}
		}
		uim = (struct uof_init_mem *)((uintptr_t)uim + uimsz);
		initmemsz -= uimsz;
	}

	/* run Batch put LM API */
	for (ae = 0; ae < MAX_AE; ae++) {
		error = qat_ae_batch_put_lm(sc, ae, &qafu->qafu_lm_init[ae],
		    qafu->qafu_num_lm_init_inst[ae]);
		if (error)
			device_printf(sc->sc_dev, "Could not put lm\n");

		qat_aefw_free_lm_init(sc, ae);
	}

	error = qat_aefw_init_ustore(sc);

	/* XXX run Batch put LM API */

	return error;
}

static int
qat_aefw_init_globals(struct qat_softc *sc)
{
	struct qat_aefw_uof *qafu = &sc->sc_aefw_uof;
	int error, i, p, s;
	u_char ae;

	/* initialize the memory segments */
	if (qafu->qafu_num_init_mem > 0) {
		error = qat_aefw_init_memory(sc);
		if (error)
			return error;
	} else {
		error = qat_aefw_init_ustore(sc);
		if (error)
			return error;
	}

	/* XXX bind import variables with ivd values */

	/* XXX bind the uC global variables
	 * local variables will done on-the-fly */
	for (i = 0; i < sc->sc_aefw_uof.qafu_num_imgs; i++) {
		for (p = 0; p < sc->sc_aefw_uof.qafu_imgs[i].qui_image->ui_num_pages; p++) {
			struct qat_uof_page *qup =
			    &sc->sc_aefw_uof.qafu_imgs[i].qui_pages[p];
			if (qup->qup_num_uw_blocks &&
			    (qup->qup_num_uc_var || qup->qup_num_imp_var)) {
				device_printf(sc->sc_dev,
				    "not support uC global variables\n");
				return ENOTSUP;
			}
		}
	}

	for (ae = 0; ae < sc->sc_ae_num; ae++) {
		struct qat_ae *qae = &(QAT_AE(sc, ae));

		for (s = 0; s < qae->qae_num_slices; s++) {
			struct qat_ae_slice *qas = &qae->qae_slices[s];

			if (qas->qas_image == NULL)
				continue;

			error =
			    qat_aefw_init_reg_sym_expr(sc, ae, qas->qas_image);
			if (error)
				return error;
		}
	}

	return 0;
}

static uint64_t
qat_aefw_get_uof_inst(struct qat_softc *sc, struct qat_uof_page *qup,
    u_int addr)
{
	uint64_t uinst = 0;
	u_int i;

	/* find the block */
	for (i = 0; i < qup->qup_num_uw_blocks; i++) {
		struct qat_uof_uword_block *quub = &qup->qup_uw_blocks[i];

		if ((addr >= quub->quub_start_addr) &&
		    (addr <= (quub->quub_start_addr +
		    (quub->quub_num_words - 1)))) {
			/* unpack n bytes and assigned to the 64-bit uword value.
			note: the microwords are stored as packed bytes.
			*/
			addr -= quub->quub_start_addr;
			addr *= AEV2_PACKED_UWORD_BYTES;
			memcpy(&uinst,
			    (void *)((uintptr_t)quub->quub_micro_words + addr),
			    AEV2_PACKED_UWORD_BYTES);
			uinst = uinst & UWORD_MASK;

			return uinst;
		}
	}

	return INVLD_UWORD;
}

static int
qat_aefw_do_pagein(struct qat_softc *sc, u_char ae, struct qat_uof_page *qup)
{
	struct qat_ae *qae = &(QAT_AE(sc, ae));
	uint64_t fill, *ucode_cpybuf;
	u_int error, i, upaddr, uraddr, ninst, cpylen;

	if (qup->qup_num_uc_var || qup->qup_num_neigh_reg ||
	    qup->qup_num_imp_var || qup->qup_num_imp_expr) {
		device_printf(sc->sc_dev,
		    "does not support fixup locals\n");
		return ENOTSUP;
	}

	ucode_cpybuf = qat_alloc_mem(UWORD_CPYBUF_SIZE * sizeof(uint64_t));

	/* XXX get fill-pattern from an image -- they are all the same */
	memcpy(&fill, sc->sc_aefw_uof.qafu_imgs[0].qui_image->ui_fill_pattern,
	    sizeof(uint64_t));

	upaddr = qup->qup_beg_paddr;
	uraddr = 0;
	ninst = qup->qup_num_micro_words;
	while (ninst > 0) {
		cpylen = min(ninst, UWORD_CPYBUF_SIZE);

		/* load the buffer */
		for (i = 0; i < cpylen; i++) {
			/* keep below code structure in case there are
			 * different handling for shared secnarios */
			if (!qae->qae_shareable_ustore) {
				/* qat_aefw_get_uof_inst() takes an address that
				 * is relative to the start of the page.
				 * So we don't need to add in the physical
				 * offset of the page. */
				if (qup->qup_page_region != 0) {
					/* XXX */
					device_printf(sc->sc_dev,
					    "region != 0 is not supported\n");
					qat_free_mem(ucode_cpybuf);
					return ENOTSUP;
				} else {
					/* for mixing case, it should take
					 * physical address */
					ucode_cpybuf[i] = qat_aefw_get_uof_inst(
					    sc, qup, upaddr + i);
					if (ucode_cpybuf[i] == INVLD_UWORD) {
					    /* fill hole in the uof */
					    ucode_cpybuf[i] = fill;
					}
				}
			} else {
				/* XXX */
				qat_free_mem(ucode_cpybuf);
				return ENOTSUP;
			}
		}

		/* copy the buffer to ustore */
		if (!qae->qae_shareable_ustore) {
			error = qat_ae_ucode_write(sc, ae, upaddr, cpylen,
			    ucode_cpybuf);
			if (error)
				return error;
		} else {
			/* XXX */
			qat_free_mem(ucode_cpybuf);
			return ENOTSUP;
		}
		upaddr += cpylen;
		uraddr += cpylen;
		ninst -= cpylen;
	}

	qat_free_mem(ucode_cpybuf);

	return 0;
}

static int
qat_aefw_uof_write_one(struct qat_softc *sc, struct qat_uof_image *qui)
{
	struct uof_image *ui = qui->qui_image;
	struct qat_ae_page *qap;
	u_int s, p, c;
	int error;
	u_char ae, ctx_mask;

	if (__SHIFTOUT(ui->ui_ae_mode, AE_MODE_CTX_MODE) == MAX_AE_CTX)
		ctx_mask = 0xff; /* 8-ctx mode */
	else
		ctx_mask = 0x55; /* 4-ctx mode */

	/* load the default page and set assigned CTX PC
	 * to the entrypoint address */
	for (ae = 0; ae < sc->sc_ae_num; ae++) {
		struct qat_ae *qae = &(QAT_AE(sc, ae));
		struct qat_ae_slice *qas;
		u_int metadata;

		MPASS(ae < UOF_MAX_NUM_OF_AE);

		if ((ui->ui_ae_assigned & (1 << ae)) == 0)
			continue;

		/* find the slice to which this image is assigned */
		for (s = 0; s < qae->qae_num_slices; s++) {
			qas = &qae->qae_slices[s];
			if (ui->ui_ctx_assigned & qas->qas_assigned_ctx_mask)
				break;
		}
		if (s >= qae->qae_num_slices)
			continue;

		qas = &qae->qae_slices[s];

		for (p = 0; p < ui->ui_num_pages; p++) {
			qap = &qas->qas_pages[p];

			/* Only load pages loaded by default */
			if (!qap->qap_page->qup_def_page)
				continue;

			error = qat_aefw_do_pagein(sc, ae, qap->qap_page);
			if (error)
				return error;
		}

		metadata = qas->qas_image->qui_image->ui_app_metadata;
		if (metadata != 0xffffffff && bootverbose) {
			device_printf(sc->sc_dev,
			    "loaded firmware: %s\n",
			    qat_aefw_uof_string(sc, metadata));
		}

		/* Assume starting page is page 0 */
		qap = &qas->qas_pages[0];
		for (c = 0; c < MAX_AE_CTX; c++) {
			if (ctx_mask & (1 << c))
				qas->qas_cur_pages[c] = qap;
			else
				qas->qas_cur_pages[c] = NULL;
		}

		/* set the live context */
		qae->qae_live_ctx_mask = ui->ui_ctx_assigned;

		/* set context PC to the image entrypoint address */
		error = qat_ae_write_pc(sc, ae, ui->ui_ctx_assigned,
		    ui->ui_entry_address);
		if (error)
			return error;
	}

	/* XXX store the checksum for convenience */

	return 0;
}

static int
qat_aefw_uof_write(struct qat_softc *sc)
{
	int error = 0;
	int i;

	error = qat_aefw_init_globals(sc);
	if (error) {
		device_printf(sc->sc_dev,
		    "Could not initialize globals\n");
		return error;
	}

	for (i = 0; i < sc->sc_aefw_uof.qafu_num_imgs; i++) {
		error = qat_aefw_uof_write_one(sc,
		    &sc->sc_aefw_uof.qafu_imgs[i]);
		if (error)
			break;
	}

	/* XXX UcLo_computeFreeUstore */

	return error;
}
