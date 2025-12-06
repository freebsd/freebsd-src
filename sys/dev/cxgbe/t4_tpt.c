/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2023 Chelsio Communications, Inc.
 * Written by: John Baldwin <jhb@FreeBSD.org>
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
 */

#include "common/common.h"

/*
 * Support routines to manage TPT entries used for both RDMA and NVMe
 * offloads.  This includes allocating STAG indices and managing the
 * PBL pool.
 */

#define T4_ULPTX_MIN_IO 32
#define T4_MAX_INLINE_SIZE 96
#define T4_ULPTX_MAX_DMA 1024

/* PBL and STAG Memory Managers. */

#define MIN_PBL_SHIFT 5			/* 32B == min PBL size (4 entries) */

uint32_t
t4_pblpool_alloc(struct adapter *sc, int size)
{
	vmem_addr_t addr;

	if (vmem_xalloc(sc->pbl_arena, roundup(size, (1 << MIN_PBL_SHIFT)),
	    4, 0, 0, VMEM_ADDR_MIN, VMEM_ADDR_MAX, M_FIRSTFIT | M_NOWAIT,
	    &addr) != 0)
		return (0);
#ifdef VERBOSE_TRACES
	CTR(KTR_CXGBE, "%s: addr 0x%lx size %d", __func__, addr, size);
#endif
	return (addr);
}

void
t4_pblpool_free(struct adapter *sc, uint32_t addr, int size)
{
#ifdef VERBOSE_TRACES
	CTR(KTR_CXGBE, "%s: addr 0x%x size %d", __func__, addr, size);
#endif
	vmem_xfree(sc->pbl_arena, addr, roundup(size, (1 << MIN_PBL_SHIFT)));
}

uint32_t
t4_stag_alloc(struct adapter *sc, int size)
{
	vmem_addr_t stag_idx;

	if (vmem_alloc(sc->stag_arena, size, M_FIRSTFIT | M_NOWAIT,
	    &stag_idx) != 0)
		return (T4_STAG_UNSET);
#ifdef VERBOSE_TRACES
	CTR(KTR_CXGBE, "%s: idx 0x%lx size %d", __func__, stag_idx, size);
#endif
	return (stag_idx);
}

void
t4_stag_free(struct adapter *sc, uint32_t stag_idx, int size)
{
#ifdef VERBOSE_TRACES
	CTR(KTR_CXGBE, "%s: idx 0x%x size %d", __func__, stag_idx, size);
#endif
	vmem_free(sc->stag_arena, stag_idx, size);
}

void
t4_init_tpt(struct adapter *sc)
{
	if (sc->vres.pbl.size != 0)
		sc->pbl_arena = vmem_create("PBL_MEM_POOL", sc->vres.pbl.start,
		    sc->vres.pbl.size, 1, 0, M_FIRSTFIT | M_WAITOK);
	if (sc->vres.stag.size != 0)
		sc->stag_arena = vmem_create("STAG", 1,
		    sc->vres.stag.size >> 5, 1, 0, M_FIRSTFIT | M_WAITOK);
}

void
t4_free_tpt(struct adapter *sc)
{
	if (sc->pbl_arena != NULL)
		vmem_destroy(sc->pbl_arena);
	if (sc->stag_arena != NULL)
		vmem_destroy(sc->stag_arena);
}

/*
 * TPT support routines.  TPT entries are stored in the STAG adapter
 * memory region and are written to via ULP_TX_MEM_WRITE commands in
 * FW_ULPTX_WR work requests.
 */

void
t4_write_mem_dma_wr(struct adapter *sc, void *wr, int wr_len, int tid,
    uint32_t addr, uint32_t len, vm_paddr_t data, uint64_t cookie)
{
	struct ulp_mem_io *ulpmc;
	struct ulptx_sgl *sgl;

	MPASS(wr_len == T4_WRITE_MEM_DMA_LEN);

	addr &= 0x7FFFFFF;

	memset(wr, 0, wr_len);
	ulpmc = wr;
	INIT_ULPTX_WR(ulpmc, wr_len, 0, tid);
	if (cookie != 0) {
		ulpmc->wr.wr_hi |= htobe32(F_FW_WR_COMPL);
		ulpmc->wr.wr_lo = cookie;
	}
	ulpmc->cmd = htobe32(V_ULPTX_CMD(ULP_TX_MEM_WRITE) |
	    V_T5_ULP_MEMIO_ORDER(1) |
	    V_T5_ULP_MEMIO_FID(sc->sge.ofld_rxq[0].iq.abs_id));
	if (chip_id(sc) >= CHELSIO_T7)
		ulpmc->dlen = htobe32(V_T7_ULP_MEMIO_DATA_LEN(len >> 5));
	else
		ulpmc->dlen = htobe32(V_ULP_MEMIO_DATA_LEN(len >> 5));
	ulpmc->len16 = htobe32((tid << 8) |
	    DIV_ROUND_UP(wr_len - sizeof(ulpmc->wr), 16));
	ulpmc->lock_addr = htobe32(V_ULP_MEMIO_ADDR(addr));

	sgl = (struct ulptx_sgl *)(ulpmc + 1);
	sgl->cmd_nsge = htobe32(V_ULPTX_CMD(ULP_TX_SC_DSGL) | V_ULPTX_NSGE(1));
	sgl->len0 = htobe32(len);
	sgl->addr0 = htobe64(data);
}

void
t4_write_mem_inline_wr(struct adapter *sc, void *wr, int wr_len, int tid,
    uint32_t addr, uint32_t len, void *data, uint64_t cookie)
{
	struct ulp_mem_io *ulpmc;
	struct ulptx_idata *ulpsc;

	MPASS(len > 0 && len <= T4_MAX_INLINE_SIZE);
	MPASS(wr_len == T4_WRITE_MEM_INLINE_LEN(len));

	addr &= 0x7FFFFFF;

	memset(wr, 0, wr_len);
	ulpmc = wr;
	INIT_ULPTX_WR(ulpmc, wr_len, 0, tid);

	if (cookie != 0) {
		ulpmc->wr.wr_hi |= htobe32(F_FW_WR_COMPL);
		ulpmc->wr.wr_lo = cookie;
	}

	ulpmc->cmd = htobe32(V_ULPTX_CMD(ULP_TX_MEM_WRITE) |
	    F_T5_ULP_MEMIO_IMM);

	if (chip_id(sc) >= CHELSIO_T7)
		ulpmc->dlen = htobe32(V_T7_ULP_MEMIO_DATA_LEN(
			DIV_ROUND_UP(len, T4_ULPTX_MIN_IO)));
	else
		ulpmc->dlen = htobe32(V_ULP_MEMIO_DATA_LEN(
			DIV_ROUND_UP(len, T4_ULPTX_MIN_IO)));
	ulpmc->len16 = htobe32((tid << 8) |
	    DIV_ROUND_UP(wr_len - sizeof(ulpmc->wr), 16));
	ulpmc->lock_addr = htobe32(V_ULP_MEMIO_ADDR(addr));

	ulpsc = (struct ulptx_idata *)(ulpmc + 1);
	ulpsc->cmd_more = htobe32(V_ULPTX_CMD(ULP_TX_SC_IMM));
	ulpsc->len = htobe32(roundup(len, T4_ULPTX_MIN_IO));

	if (data != NULL)
		memcpy(ulpsc + 1, data, len);
}
