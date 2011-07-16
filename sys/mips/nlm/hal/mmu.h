/*-
 * Copyright 2003-2011 Netlogic Microsystems (Netlogic). All rights
 * reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY Netlogic Microsystems ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL NETLOGIC OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 *
 * $FreeBSD$
 * NETLOGIC_BSD */

#ifndef __XLP_MMU_H__
#define __XLP_MMU_H__

#include <mips/nlm/hal/cop0.h>
#include <mips/nlm/hal/mips-extns.h>

#define XLP_MMU_SETUP_REG		0x400
#define XLP_MMU_LFSRSEED_REG		0x401
#define XLP_MMU_HPW_NUM_PAGE_LVL_REG	0x410
#define XLP_MMU_PGWKR_PGDBASE_REG	0x411
#define XLP_MMU_PGWKR_PGDSHFT_REG	0x412
#define XLP_MMU_PGWKR_PGDMASK_REG	0x413
#define XLP_MMU_PGWKR_PUDSHFT_REG	0x414
#define XLP_MMU_PGWKR_PUDMASK_REG	0x415
#define XLP_MMU_PGWKR_PMDSHFT_REG	0x416
#define XLP_MMU_PGWKR_PMDMASK_REG	0x417
#define XLP_MMU_PGWKR_PTESHFT_REG	0x418
#define XLP_MMU_PGWKR_PTEMASK_REG	0x419

typedef struct hw_pagewalker {
	int pgd_present;
	int pud_present;
	int pmd_present;
	int pte_present;
	uint64_t pgd_baseaddr;
	uint32_t pgd_shift;
	uint32_t pgd_mask;
	uint32_t pud_shift;
	uint32_t pud_mask;
	uint32_t pmd_shift;
	uint32_t pmd_mask;
	uint32_t pte_shift;
	uint32_t pte_mask;
} nlm_pagewalker;

/**
 * On power on reset, XLP comes up with 64 TLBs.
 * Large-variable-tlb's (ELVT) and extended TLB is disabled.
 * Enabling large-variable-tlb's sets up the standard
 * TLB size from 64 to 128 TLBs.
 * Enabling fixed TLB (EFT) sets up an additional 2048 tlbs.
 * ELVT + EFT = 128 + 2048 = 2176 TLB entries.
 * threads  64-entry-standard-tlb    128-entry-standard-tlb
 * per      std-tlb-only| std+EFT  | std-tlb-only| std+EFT
 * core                 |          |             |
 * --------------------------------------------------------
 * 1         64           64+2048     128          128+2048
 * 2         64           64+1024      64           64+1024
 * 4         32           32+512       32           32+512
 *
 * 1(G)      64           64+2048     128          128+2048
 * 2(G)      128         128+2048     128          128+2048
 * 4(G)      128         128+2048     128          128+2048
 * (G) = Global mode
 */


/* en = 1 to enable
 * en = 0 to disable
 */
static __inline__ void nlm_large_variable_tlb_en (int en)
{
	unsigned int val;

	val = nlm_read_c0_config6();
	val |= (en << 5);
	nlm_write_c0_config6(val);
	return;
}

/* en = 1 to enable
 * en = 0 to disable
 */
static __inline__ void nlm_pagewalker_en (int en)
{
	unsigned int val;

	val = nlm_read_c0_config6();
	val |= (en << 3);
	nlm_write_c0_config6(val);
	return;
}

/* en = 1 to enable
 * en = 0 to disable
 */
static __inline__ void nlm_extended_tlb_en (int en)
{
	unsigned int val;

	val = nlm_read_c0_config6();
	val |= (en << 2);
	nlm_write_c0_config6(val);
	return;
}

static __inline__ int nlm_get_num_combined_tlbs(void)
{
	return (((nlm_read_c0_config6() >> 16) & 0xffff) + 1);
}

/* get number of variable TLB entries */
static __inline__ int nlm_get_num_vtlbs(void)
{
	return (((nlm_read_c0_config6() >> 6) & 0x3ff) + 1);
}

static __inline__ void nlm_setup_extended_pagemask (int mask)
{
	nlm_write_c0_config7(mask);
}

/* hashindex_en = 1 to enable hash mode, hashindex_en=0 to disable
 * global_mode = 1 to enable global mode, global_mode=0 to disable
 * clk_gating = 0 to enable clock gating, clk_gating=1 to disable
 */
static __inline__ void nlm_mmu_setup(int hashindex_en, int global_mode,
		int clk_gating)
{
	/*uint32_t mmusetup = nlm_mfcr(XLP_MMU_SETUP_REG);*/

	uint32_t mmusetup = 0;
	mmusetup |= (hashindex_en << 13);
	mmusetup |= (clk_gating << 3);
	mmusetup |= (global_mode << 0);
	nlm_mtcr(XLP_MMU_SETUP_REG, mmusetup);
}

static __inline__ void nlm_mmu_lfsr_seed (int thr0_seed, int thr1_seed,
		int thr2_seed, int thr3_seed)
{
	uint32_t seed = nlm_mfcr(XLP_MMU_LFSRSEED_REG);
	seed |= ((thr3_seed & 0x7f) << 23);
	seed |= ((thr2_seed & 0x7f) << 16);
	seed |= ((thr1_seed & 0x7f) << 7);
	seed |= ((thr0_seed & 0x7f) << 0);
	nlm_mtcr(XLP_MMU_LFSRSEED_REG, seed);
}

static __inline__ void nlm_pagewalker_setup (nlm_pagewalker *walker)
{
	uint64_t val;

	if (!walker->pgd_present)
		return;

	val = nlm_mfcr(XLP_MMU_HPW_NUM_PAGE_LVL_REG);

	if (walker->pgd_present)
		val |= (1 << 3);

	if (walker->pud_present)
		val |= (1 << 2);

	if (walker->pmd_present)
		val |= (1 << 1);

	if (walker->pte_present)
		val |= (1 << 0);

	nlm_mtcr(XLP_MMU_HPW_NUM_PAGE_LVL_REG, val);

	nlm_mtcr(XLP_MMU_PGWKR_PGDBASE_REG, walker->pgd_baseaddr);
	nlm_mtcr(XLP_MMU_PGWKR_PGDSHFT_REG, walker->pgd_shift);
	nlm_mtcr(XLP_MMU_PGWKR_PGDMASK_REG, walker->pgd_mask);
	nlm_mtcr(XLP_MMU_PGWKR_PUDSHFT_REG, walker->pud_shift);
	nlm_mtcr(XLP_MMU_PGWKR_PUDMASK_REG, walker->pud_mask);
	nlm_mtcr(XLP_MMU_PGWKR_PMDSHFT_REG, walker->pmd_shift);
	nlm_mtcr(XLP_MMU_PGWKR_PMDMASK_REG, walker->pmd_mask);
	nlm_mtcr(XLP_MMU_PGWKR_PTESHFT_REG, walker->pte_shift);
	nlm_mtcr(XLP_MMU_PGWKR_PTEMASK_REG, walker->pte_mask);
}

#endif
