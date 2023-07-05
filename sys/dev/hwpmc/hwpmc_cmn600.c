/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2003-2008 Joseph Koshy
 * Copyright (c) 2007 The FreeBSD Foundation
 * Copyright (c) 2021 ARM Ltd
 *
 * Portions of this software were developed by A. Joseph Koshy under
 * sponsorship from the FreeBSD Foundation and Google, Inc.
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

/* Arm CoreLink CMN-600 Coherent Mesh Network PMU Driver */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/module.h>
#include <sys/mutex.h>
#include <sys/pmc.h>
#include <sys/pmckern.h>
#include <sys/systm.h>

#include <machine/cmn600_reg.h>

struct cmn600_descr {
	struct pmc_descr pd_descr;  /* "base class" */
	void		*pd_rw_arg; /* Argument to use with read/write */
	struct pmc	*pd_pmc;
	struct pmc_hw	*pd_phw;
	uint32_t	 pd_nodeid;
	int32_t		 pd_node_type;
	int		 pd_local_counter;

};

static struct cmn600_descr **cmn600_pmcdesc;

static struct cmn600_pmc cmn600_pmcs[CMN600_UNIT_MAX];
static int cmn600_units = 0;

static inline struct cmn600_descr *
cmn600desc(int ri)
{

	return (cmn600_pmcdesc[ri]);
}

static inline int
class_ri2unit(int ri)
{

	return (ri / CMN600_COUNTERS_N);
}

#define	EVENCNTR(x)	(((x) >> POR_DT_PMEVCNT_EVENCNT_SHIFT) << \
    POR_DTM_PMEVCNT_CNTR_WIDTH)
#define	ODDCNTR(x)	(((x) >> POR_DT_PMEVCNT_ODDCNT_SHIFT) << \
    POR_DTM_PMEVCNT_CNTR_WIDTH)

static uint64_t
cmn600_pmu_readcntr(void *arg, u_int nodeid, u_int xpcntr, u_int dtccntr,
    u_int width)
{
	uint64_t dtcval, xpval;

	KASSERT(xpcntr < 4, ("[cmn600,%d] XP counter number %d is too big."
	    " Max: 3", __LINE__, xpcntr));
	KASSERT(dtccntr < 8, ("[cmn600,%d] Global counter number %d is too"
	    " big. Max: 7", __LINE__, dtccntr));

	dtcval = pmu_cmn600_rd8(arg, nodeid, NODE_TYPE_DTC,
	    POR_DT_PMEVCNT(dtccntr >> 1));
	if (width == 4) {
		dtcval = (dtccntr & 1) ? ODDCNTR(dtcval) : EVENCNTR(dtcval);
		dtcval &= 0xffffffff0000UL;
	} else
		dtcval <<= POR_DTM_PMEVCNT_CNTR_WIDTH;

	xpval = pmu_cmn600_rd8(arg, nodeid, NODE_TYPE_XP, POR_DTM_PMEVCNT);
	xpval >>= xpcntr * POR_DTM_PMEVCNT_CNTR_WIDTH;
	xpval &= 0xffffUL;
	return (dtcval | xpval);
}

static void
cmn600_pmu_writecntr(void *arg, u_int nodeid, u_int xpcntr, u_int dtccntr,
    u_int width, uint64_t val)
{
	int shift;

	KASSERT(xpcntr < 4, ("[cmn600,%d] XP counter number %d is too big."
	    " Max: 3", __LINE__, xpcntr));
	KASSERT(dtccntr < 8, ("[cmn600,%d] Global counter number %d is too"
	    " big. Max: 7", __LINE__, dtccntr));

	if (width == 4) {
		shift = (dtccntr & 1) ? POR_DT_PMEVCNT_ODDCNT_SHIFT :
		    POR_DT_PMEVCNT_EVENCNT_SHIFT;
		pmu_cmn600_md8(arg, nodeid, NODE_TYPE_DTC,
		    POR_DT_PMEVCNT(dtccntr >> 1), 0xffffffffUL << shift,
		    ((val >> POR_DTM_PMEVCNT_CNTR_WIDTH) & 0xffffffff) << shift);
	} else
		pmu_cmn600_wr8(arg, nodeid, NODE_TYPE_DTC,
		    POR_DT_PMEVCNT(dtccntr & ~0x1), val >>
		    POR_DTM_PMEVCNT_CNTR_WIDTH);

	shift = xpcntr * POR_DTM_PMEVCNT_CNTR_WIDTH;
	val &= 0xffffUL;
	pmu_cmn600_md8(arg, nodeid, NODE_TYPE_XP, POR_DTM_PMEVCNT,
	    0xffffUL << shift, val << shift);
}

#undef	EVENCNTR
#undef	ODDCNTR

/*
 * read a pmc register
 */
static int
cmn600_read_pmc(int cpu, int ri, struct pmc *pm, pmc_value_t *v)
{
	int counter, local_counter, nodeid;
	struct cmn600_descr *desc;
	void *arg;

	KASSERT(cpu >= 0 && cpu < pmc_cpu_max(),
	    ("[cmn600,%d] illegal CPU value %d", __LINE__, cpu));
	KASSERT(ri >= 0, ("[cmn600,%d] row-index %d out of range", __LINE__,
	    ri));

	counter = ri % CMN600_COUNTERS_N;
	desc = cmn600desc(ri);
	arg = desc->pd_rw_arg;
	nodeid = pm->pm_md.pm_cmn600.pm_cmn600_nodeid;
	local_counter = pm->pm_md.pm_cmn600.pm_cmn600_local_counter;

	*v = cmn600_pmu_readcntr(arg, nodeid, local_counter, counter, 4);
	PMCDBG3(MDP, REA, 2, "%s id=%d -> %jd", __func__, ri, *v);

	return (0);
}

/*
 * Write a pmc register.
 */
static int
cmn600_write_pmc(int cpu, int ri, struct pmc *pm, pmc_value_t v)
{
	int counter, local_counter, nodeid;
	struct cmn600_descr *desc;
	void *arg;

	KASSERT(cpu >= 0 && cpu < pmc_cpu_max(),
	    ("[cmn600,%d] illegal CPU value %d", __LINE__, cpu));
	KASSERT(ri >= 0, ("[cmn600,%d] row-index %d out of range", __LINE__,
	    ri));

	counter = ri % CMN600_COUNTERS_N;
	desc = cmn600desc(ri);
	arg = desc->pd_rw_arg;
	nodeid = pm->pm_md.pm_cmn600.pm_cmn600_nodeid;
	local_counter = pm->pm_md.pm_cmn600.pm_cmn600_local_counter;

	KASSERT(pm != NULL,
	    ("[cmn600,%d] PMC not owned (cpu%d,pmc%d)", __LINE__,
		cpu, ri));

	PMCDBG4(MDP, WRI, 1, "%s cpu=%d ri=%d v=%jx", __func__, cpu, ri, v);

	cmn600_pmu_writecntr(arg, nodeid, local_counter, counter, 4, v);
	return (0);
}

/*
 * configure hardware pmc according to the configuration recorded in
 * pmc 'pm'.
 */
static int
cmn600_config_pmc(int cpu, int ri, struct pmc *pm)
{
	struct pmc_hw *phw;

	PMCDBG4(MDP, CFG, 1, "%s cpu=%d ri=%d pm=%p", __func__, cpu, ri, pm);

	KASSERT(cpu >= 0 && cpu < pmc_cpu_max(),
	    ("[cmn600,%d] illegal CPU value %d", __LINE__, cpu));
	KASSERT(ri >= 0, ("[cmn600,%d] row-index %d out of range", __LINE__,
	    ri));

	phw = cmn600desc(ri)->pd_phw;

	KASSERT(pm == NULL || phw->phw_pmc == NULL,
	    ("[cmn600,%d] pm=%p phw->pm=%p hwpmc not unconfigured",
		__LINE__, pm, phw->phw_pmc));

	phw->phw_pmc = pm;
	return (0);
}

/*
 * Retrieve a configured PMC pointer from hardware state.
 */
static int
cmn600_get_config(int cpu, int ri, struct pmc **ppm)
{

	*ppm = cmn600desc(ri)->pd_phw->phw_pmc;

	return (0);
}

#define	CASE_DN_VER_EVT(n, id) case PMC_EV_CMN600_PMU_ ## n: { *event = id; \
	return (0); }
static int
cmn600_map_ev2event(int ev, int rev, int *node_type, uint8_t *event)
{
	if (ev < PMC_EV_CMN600_PMU_dn_rxreq_dvmop ||
	    ev > PMC_EV_CMN600_PMU_rni_rdb_ord)
		return (EINVAL);
	if (ev <= PMC_EV_CMN600_PMU_dn_rxreq_trk_full) {
		*node_type = NODE_TYPE_DVM;
		if (rev < 0x200) {
			switch (ev) {
			CASE_DN_VER_EVT(dn_rxreq_dvmop, 1);
			CASE_DN_VER_EVT(dn_rxreq_dvmsync, 2);
			CASE_DN_VER_EVT(dn_rxreq_dvmop_vmid_filtered, 3);
			CASE_DN_VER_EVT(dn_rxreq_retried, 4);
			CASE_DN_VER_EVT(dn_rxreq_trk_occupancy, 5);
			}
		} else {
			switch (ev) {
			CASE_DN_VER_EVT(dn_rxreq_tlbi_dvmop, 0x01);
			CASE_DN_VER_EVT(dn_rxreq_bpi_dvmop, 0x02);
			CASE_DN_VER_EVT(dn_rxreq_pici_dvmop, 0x03);
			CASE_DN_VER_EVT(dn_rxreq_vivi_dvmop, 0x04);
			CASE_DN_VER_EVT(dn_rxreq_dvmsync, 0x05);
			CASE_DN_VER_EVT(dn_rxreq_dvmop_vmid_filtered, 0x06);
			CASE_DN_VER_EVT(dn_rxreq_dvmop_other_filtered, 0x07);
			CASE_DN_VER_EVT(dn_rxreq_retried, 0x08);
			CASE_DN_VER_EVT(dn_rxreq_snp_sent, 0x09);
			CASE_DN_VER_EVT(dn_rxreq_snp_stalled, 0x0a);
			CASE_DN_VER_EVT(dn_rxreq_trk_full, 0x0b);
			CASE_DN_VER_EVT(dn_rxreq_trk_occupancy, 0x0c);
			}
		}
		return (EINVAL);
	} else if (ev <= PMC_EV_CMN600_PMU_hnf_snp_fwded) {
		*node_type = NODE_TYPE_HN_F;
		*event = ev - PMC_EV_CMN600_PMU_hnf_cache_miss;
		return (0);
	} else if (ev <= PMC_EV_CMN600_PMU_hni_pcie_serialization) {
		*node_type = NODE_TYPE_HN_I;
		*event = ev - PMC_EV_CMN600_PMU_hni_rrt_rd_occ_cnt_ovfl;
		return (0);
	} else if (ev <= PMC_EV_CMN600_PMU_xp_partial_dat_flit) {
		*node_type = NODE_TYPE_XP;
		*event = ev - PMC_EV_CMN600_PMU_xp_txflit_valid;
		return (0);
	} else if (ev <= PMC_EV_CMN600_PMU_sbsx_txrsp_stall) {
		*node_type = NODE_TYPE_SBSX;
		*event = ev - PMC_EV_CMN600_PMU_sbsx_rd_req;
		return (0);
	} else if (ev <= PMC_EV_CMN600_PMU_rnd_rdb_ord) {
		*node_type = NODE_TYPE_RN_D;
		*event = ev - PMC_EV_CMN600_PMU_rnd_s0_rdata_beats;
		return (0);
	} else if (ev <= PMC_EV_CMN600_PMU_rni_rdb_ord) {
		*node_type = NODE_TYPE_RN_I;
		*event = ev - PMC_EV_CMN600_PMU_rni_s0_rdata_beats;
		return (0);
	} else if (ev <= PMC_EV_CMN600_PMU_cxha_snphaz_occ) {
		*node_type = NODE_TYPE_CXHA;
		*event = ev - PMC_EV_CMN600_PMU_cxha_rddatbyp;
		return (0);
	} else if (ev <= PMC_EV_CMN600_PMU_cxra_ext_dat_stall) {
		*node_type = NODE_TYPE_CXRA;
		*event = ev - PMC_EV_CMN600_PMU_cxra_req_trk_occ;
		return (0);
	} else if (ev <= PMC_EV_CMN600_PMU_cxla_avg_latency_form_tx_tlp) {
		*node_type = NODE_TYPE_CXLA;
		*event = ev - PMC_EV_CMN600_PMU_cxla_rx_tlp_link0;
		return (0);
	}
	return (EINVAL);
}

/*
 * Check if a given allocation is feasible.
 */

static int
cmn600_allocate_pmc(int cpu, int ri, struct pmc *pm,
    const struct pmc_op_pmcallocate *a)
{
	struct cmn600_descr *desc;
	const struct pmc_descr *pd;
	uint64_t caps __unused;
	int local_counter, node_type;
	enum pmc_event pe;
	void *arg;
	uint8_t e;
	int err;

	(void) cpu;

	KASSERT(cpu >= 0 && cpu < pmc_cpu_max(),
	    ("[cmn600,%d] illegal CPU value %d", __LINE__, cpu));
	KASSERT(ri >= 0, ("[cmn600,%d] row-index %d out of range", __LINE__,
	    ri));

	desc = cmn600desc(ri);
	arg = desc->pd_rw_arg;
	pd = &desc->pd_descr;
	if (cmn600_pmcs[class_ri2unit(ri)].domain != pcpu_find(cpu)->pc_domain)
		return (EINVAL);

	/* check class match */
	if (pd->pd_class != a->pm_class)
		return (EINVAL);

	caps = pm->pm_caps;

	PMCDBG3(MDP, ALL, 1, "%s ri=%d caps=0x%x", __func__, ri, caps);

	pe = a->pm_ev;
	err = cmn600_map_ev2event(pe, pmu_cmn600_rev(arg), &node_type, &e);
	if (err != 0)
		return (err);
	err = pmu_cmn600_alloc_localpmc(arg,
	    a->pm_md.pm_cmn600.pma_cmn600_nodeid, node_type, &local_counter);
	if (err != 0)
		return (err);

	pm->pm_md.pm_cmn600.pm_cmn600_config =
	    a->pm_md.pm_cmn600.pma_cmn600_config;
	pm->pm_md.pm_cmn600.pm_cmn600_occupancy =
	    a->pm_md.pm_cmn600.pma_cmn600_occupancy;
	desc->pd_nodeid = pm->pm_md.pm_cmn600.pm_cmn600_nodeid =
	    a->pm_md.pm_cmn600.pma_cmn600_nodeid;
	desc->pd_node_type = pm->pm_md.pm_cmn600.pm_cmn600_node_type =
	    node_type;
	pm->pm_md.pm_cmn600.pm_cmn600_event = e;
	desc->pd_local_counter = pm->pm_md.pm_cmn600.pm_cmn600_local_counter =
	    local_counter;

	return (0);
}

/* Release machine dependent state associated with a PMC. */

static int
cmn600_release_pmc(int cpu, int ri, struct pmc *pmc)
{
	struct cmn600_descr *desc;
	struct pmc_hw *phw;
	struct pmc *pm __diagused;
	int err;

	(void) pmc;

	KASSERT(cpu >= 0 && cpu < pmc_cpu_max(),
	    ("[cmn600,%d] illegal CPU value %d", __LINE__, cpu));
	KASSERT(ri >= 0, ("[cmn600,%d] row-index %d out of range", __LINE__,
	    ri));

	desc = cmn600desc(ri);
	phw = desc->pd_phw;
	pm  = phw->phw_pmc;
	err = pmu_cmn600_free_localpmc(desc->pd_rw_arg, desc->pd_nodeid,
	    desc->pd_node_type, desc->pd_local_counter);
	if (err != 0)
		return (err);

	KASSERT(pm == NULL, ("[cmn600,%d] PHW pmc %p non-NULL", __LINE__, pm));

	return (0);
}

static inline uint64_t
cmn600_encode_source(int node_type, int counter, int port, int sub)
{

	/* Calculate pmevcnt0_input_sel based on list in Table 3-794. */
	if (node_type == NODE_TYPE_XP)
		return (0x4 | counter);
	
	return (((port + 1) << 4) | (sub << 2) | counter);
}

/*
 * start a PMC.
 */

static int
cmn600_start_pmc(int cpu, int ri, struct pmc *pm)
{
	int counter, local_counter, node_type, shift;
	uint64_t config, occupancy, source, xp_pmucfg;
	struct cmn600_descr *desc;
	uint8_t event, port, sub;
	uint16_t nodeid;
	void *arg;

	KASSERT(cpu >= 0 && cpu < pmc_cpu_max(),
	    ("[cmn600,%d] illegal CPU value %d", __LINE__, cpu));
	KASSERT(ri >= 0, ("[cmn600,%d] row-index %d out of range", __LINE__,
	    ri));

	counter = ri % CMN600_COUNTERS_N;
	desc = cmn600desc(ri);
	arg = desc->pd_rw_arg;

	PMCDBG3(MDP, STA, 1, "%s cpu=%d ri=%d", __func__, cpu, ri);

	config = pm->pm_md.pm_cmn600.pm_cmn600_config;
	occupancy = pm->pm_md.pm_cmn600.pm_cmn600_occupancy;
	node_type = pm->pm_md.pm_cmn600.pm_cmn600_node_type;
	event = pm->pm_md.pm_cmn600.pm_cmn600_event;
	nodeid = pm->pm_md.pm_cmn600.pm_cmn600_nodeid;
	local_counter = pm->pm_md.pm_cmn600.pm_cmn600_local_counter;
	port = (nodeid >> 2) & 1;
	sub = nodeid & 3;

	switch (node_type) {
	case NODE_TYPE_DVM:
	case NODE_TYPE_HN_F:
	case NODE_TYPE_CXHA:
	case NODE_TYPE_CXRA:
		pmu_cmn600_md8(arg, nodeid, node_type,
		    CMN600_COMMON_PMU_EVENT_SEL,
		    CMN600_COMMON_PMU_EVENT_SEL_OCC_MASK,
		    occupancy << CMN600_COMMON_PMU_EVENT_SEL_OCC_SHIFT);
		break;
	case NODE_TYPE_XP:
		/* Set PC and Interface.*/
		event |= config;
	}

	/*
	 * 5.5.1 Set up PMU counters
	 * 1. Ensure that the NIDEN input is asserted. HW side. */
	/* 2. Select event of target node for one of four outputs. */
	pmu_cmn600_md8(arg, nodeid, node_type, CMN600_COMMON_PMU_EVENT_SEL,
	    0xff << (local_counter * 8),
	    event << (local_counter * 8));

	xp_pmucfg = pmu_cmn600_rd8(arg, nodeid, NODE_TYPE_XP,
	    POR_DTM_PMU_CONFIG);
	/*
	 * 3. configure XP to connect one of four target node outputs to local
	 * counter.
	 */
	source = cmn600_encode_source(node_type, local_counter, port, sub);
	shift = (local_counter * POR_DTM_PMU_CONFIG_VCNT_INPUT_SEL_WIDTH) +
	    POR_DTM_PMU_CONFIG_VCNT_INPUT_SEL_SHIFT;
	xp_pmucfg &= ~(0xffUL << shift);
	xp_pmucfg |= source << shift;

	/* 4. Pair with global counters A, B, C, ..., H. */
	shift = (local_counter * 4) + 16;
	xp_pmucfg &= ~(0xfUL << shift);
	xp_pmucfg |= counter << shift;
	/* Enable pairing.*/
	xp_pmucfg |= 1 << (local_counter + 4);

	/* 5. Combine local counters 0 with 1, 2 with 3 or all four. */
	xp_pmucfg &= ~0xeUL;

	/* 6. Enable XP's PMU function. */
	xp_pmucfg |= POR_DTM_PMU_CONFIG_PMU_EN;
	pmu_cmn600_wr8(arg, nodeid, NODE_TYPE_XP, POR_DTM_PMU_CONFIG, xp_pmucfg);
	if (node_type == NODE_TYPE_CXLA)
		pmu_cmn600_set8(arg, nodeid, NODE_TYPE_CXLA,
		    POR_CXG_RA_CFG_CTL, EN_CXLA_PMUCMD_PROP);

	/* 7. Enable DTM. */
	pmu_cmn600_set8(arg, nodeid, NODE_TYPE_XP, POR_DTM_CONTROL,
	    POR_DTM_CONTROL_DTM_ENABLE);

	/* 8. Reset grouping of global counters. Use 32 bits. */
	pmu_cmn600_clr8(arg, nodeid, NODE_TYPE_DTC, POR_DT_PMCR,
	    POR_DT_PMCR_CNTCFG_MASK);

	/* 9. Enable DTC. */
	pmu_cmn600_set8(arg, nodeid, NODE_TYPE_DTC, POR_DT_DTC_CTL,
	    POR_DT_DTC_CTL_DT_EN);

	/* 10. Enable Overflow Interrupt. */
	pmu_cmn600_set8(arg, nodeid, NODE_TYPE_DTC, POR_DT_PMCR,
	    POR_DT_PMCR_OVFL_INTR_EN);

	/* 11. Run PMC. */
	pmu_cmn600_set8(arg, nodeid, NODE_TYPE_DTC, POR_DT_PMCR,
	    POR_DT_PMCR_PMU_EN);

	return (0);
}

/*
 * Stop a PMC.
 */

static int
cmn600_stop_pmc(int cpu, int ri, struct pmc *pm)
{
	struct cmn600_descr *desc;
	int local_counter;
	uint64_t val;

	KASSERT(cpu >= 0 && cpu < pmc_cpu_max(),
	    ("[cmn600,%d] illegal CPU value %d", __LINE__, cpu));
	KASSERT(ri >= 0, ("[cmn600,%d] row-index %d out of range", __LINE__,
	    ri));

	desc = cmn600desc(ri);

	PMCDBG2(MDP, STO, 1, "%s ri=%d", __func__, ri);

	/* Disable pairing. */
	local_counter = pm->pm_md.pm_cmn600.pm_cmn600_local_counter;
	pmu_cmn600_clr8(desc->pd_rw_arg, pm->pm_md.pm_cmn600.pm_cmn600_nodeid,
	    NODE_TYPE_XP, POR_DTM_PMU_CONFIG, (1 << (local_counter + 4)));

	/* Shutdown XP's DTM function if no paired counters. */
	val = pmu_cmn600_rd8(desc->pd_rw_arg,
	    pm->pm_md.pm_cmn600.pm_cmn600_nodeid, NODE_TYPE_XP,
	    POR_DTM_PMU_CONFIG);
	if ((val & 0xf0) == 0)
		pmu_cmn600_clr8(desc->pd_rw_arg,
		    pm->pm_md.pm_cmn600.pm_cmn600_nodeid, NODE_TYPE_XP,
		    POR_DTM_PMU_CONFIG, POR_DTM_CONTROL_DTM_ENABLE);

	return (0);
}

/*
 * describe a PMC
 */
static int
cmn600_describe(int cpu, int ri, struct pmc_info *pi, struct pmc **ppmc)
{
	struct pmc_descr *pd;
	struct pmc_hw *phw;

	KASSERT(cpu >= 0 && cpu < pmc_cpu_max(),
	    ("[cmn600,%d] illegal CPU %d", __LINE__, cpu));
	KASSERT(ri >= 0, ("[cmn600,%d] row-index %d out of range", __LINE__,
	    ri));

	phw = cmn600desc(ri)->pd_phw;
	pd = &cmn600desc(ri)->pd_descr;

	strlcpy(pi->pm_name, pd->pd_name, sizeof(pi->pm_name));
	pi->pm_class = pd->pd_class;

	if (phw->phw_state & PMC_PHW_FLAG_IS_ENABLED) {
		pi->pm_enabled = TRUE;
		*ppmc          = phw->phw_pmc;
	} else {
		pi->pm_enabled = FALSE;
		*ppmc          = NULL;
	}

	return (0);
}

/*
 * processor dependent initialization.
 */

static int
cmn600_pcpu_init(struct pmc_mdep *md, int cpu)
{
	int first_ri, n, npmc;
	struct pmc_hw  *phw;
	struct pmc_cpu *pc;
	int mdep_class;

	mdep_class = PMC_MDEP_CLASS_INDEX_CMN600;
	KASSERT(cpu >= 0 && cpu < pmc_cpu_max(),
	    ("[cmn600,%d] insane cpu number %d", __LINE__, cpu));

	PMCDBG1(MDP, INI, 1, "cmn600-init cpu=%d", cpu);

	/*
	 * Set the content of the hardware descriptors to a known
	 * state and initialize pointers in the MI per-cpu descriptor.
	 */

	pc = pmc_pcpu[cpu];
	first_ri = md->pmd_classdep[mdep_class].pcd_ri;
	npmc = md->pmd_classdep[mdep_class].pcd_num;

	for (n = 0; n < npmc; n++, phw++) {
		phw = cmn600desc(n)->pd_phw;
		phw->phw_state = PMC_PHW_CPU_TO_STATE(cpu) |
		    PMC_PHW_INDEX_TO_STATE(n);
		/* Set enabled only if unit present. */
		if (cmn600_pmcs[class_ri2unit(n)].arg != NULL)
			phw->phw_state |= PMC_PHW_FLAG_IS_ENABLED;
		phw->phw_pmc = NULL;
		pc->pc_hwpmcs[n + first_ri] = phw;
	}
	return (0);
}

/*
 * processor dependent cleanup prior to the KLD
 * being unloaded
 */

static int
cmn600_pcpu_fini(struct pmc_mdep *md, int cpu)
{

	return (0);
}

static int
cmn600_pmu_intr(struct trapframe *tf, int unit, int i)
{
	struct pmc_cpu *pc __diagused;
	struct pmc_hw *phw;
	struct pmc *pm;
	int error, cpu, ri;

	ri = i + unit * CMN600_COUNTERS_N;
	cpu = curcpu;
	KASSERT(cpu >= 0 && cpu < pmc_cpu_max(),
	    ("[cmn600,%d] CPU %d out of range", __LINE__, cpu));
	pc = pmc_pcpu[cpu];
	KASSERT(pc != NULL, ("pc != NULL"));

	phw = cmn600desc(ri)->pd_phw;
	KASSERT(phw != NULL, ("phw != NULL"));
	pm  = phw->phw_pmc;
	if (pm == NULL)
		return (0);

	if (!PMC_IS_SAMPLING_MODE(PMC_TO_MODE(pm))) {
		/* Always CPU0. */
		pm->pm_pcpu_state[0].pps_overflowcnt += 1;
		return (0);
	}

	if (pm->pm_state != PMC_STATE_RUNNING)
		return (0);

	error = pmc_process_interrupt(PMC_HR, pm, tf);
	if (error)
		cmn600_stop_pmc(cpu, ri, pm);

	/* Reload sampling count */
	cmn600_write_pmc(cpu, ri, pm, pm->pm_sc.pm_reloadcount);

	return (0);
}

/*
 * Initialize ourselves.
 */
static int
cmn600_init_pmc_units(void)
{
	int i;

	if (cmn600_units > 0) { /* Already initialized. */
		return (0);
	}

	cmn600_units = cmn600_pmc_nunits();
	if (cmn600_units == 0)
		return (ENOENT);

	for (i = 0; i < cmn600_units; i++) {
		if (cmn600_pmc_getunit(i, &cmn600_pmcs[i].arg,
		    &cmn600_pmcs[i].domain) != 0)
			cmn600_pmcs[i].arg = NULL;
	}
	return (0);
}

int
pmc_cmn600_nclasses(void)
{

	if (cmn600_pmc_nunits() > 0)
		return (1);
	return (0);
}

int
pmc_cmn600_initialize(struct pmc_mdep *md)
{
	struct pmc_classdep *pcd;
	int i, npmc, unit;

	cmn600_init_pmc_units();
	KASSERT(md != NULL, ("[cmn600,%d] md is NULL", __LINE__));
	KASSERT(cmn600_units < CMN600_UNIT_MAX,
	    ("[cmn600,%d] cmn600_units too big", __LINE__));

	PMCDBG0(MDP,INI,1, "cmn600-initialize");

	npmc = CMN600_COUNTERS_N * cmn600_units;
	pcd = &md->pmd_classdep[PMC_MDEP_CLASS_INDEX_CMN600];

	pcd->pcd_caps		= PMC_CAP_SYSTEM | PMC_CAP_READ |
	    PMC_CAP_WRITE | PMC_CAP_QUALIFIER | PMC_CAP_INTERRUPT |
	    PMC_CAP_DOMWIDE;
	pcd->pcd_class	= PMC_CLASS_CMN600_PMU;
	pcd->pcd_num	= npmc;
	pcd->pcd_ri	= md->pmd_npmc;
	pcd->pcd_width	= 48;

	pcd->pcd_allocate_pmc	= cmn600_allocate_pmc;
	pcd->pcd_config_pmc	= cmn600_config_pmc;
	pcd->pcd_describe	= cmn600_describe;
	pcd->pcd_get_config	= cmn600_get_config;
	pcd->pcd_get_msr	= NULL;
	pcd->pcd_pcpu_fini	= cmn600_pcpu_fini;
	pcd->pcd_pcpu_init	= cmn600_pcpu_init;
	pcd->pcd_read_pmc	= cmn600_read_pmc;
	pcd->pcd_release_pmc	= cmn600_release_pmc;
	pcd->pcd_start_pmc	= cmn600_start_pmc;
	pcd->pcd_stop_pmc	= cmn600_stop_pmc;
	pcd->pcd_write_pmc	= cmn600_write_pmc;

	md->pmd_npmc	       += npmc;
	cmn600_pmcdesc = malloc(sizeof(struct cmn600_descr *) * npmc *
	    CMN600_PMU_DEFAULT_UNITS_N, M_PMC, M_WAITOK|M_ZERO);
	for (i = 0; i < npmc; i++) {
		cmn600_pmcdesc[i] = malloc(sizeof(struct cmn600_descr), M_PMC,
		    M_WAITOK|M_ZERO);

		unit = i / CMN600_COUNTERS_N;
		KASSERT(unit >= 0, ("unit >= 0"));
		KASSERT(cmn600_pmcs[unit].arg != NULL, ("arg != NULL"));

		cmn600_pmcdesc[i]->pd_rw_arg = cmn600_pmcs[unit].arg;
		cmn600_pmcdesc[i]->pd_descr.pd_class =
		    PMC_CLASS_CMN600_PMU;
		cmn600_pmcdesc[i]->pd_descr.pd_caps = pcd->pcd_caps;
		cmn600_pmcdesc[i]->pd_phw = (struct pmc_hw *)malloc(
		    sizeof(struct pmc_hw), M_PMC, M_WAITOK|M_ZERO);
		snprintf(cmn600_pmcdesc[i]->pd_descr.pd_name, 63,
		    "CMN600_%d", i);
		cmn600_pmu_intr_cb(cmn600_pmcs[unit].arg, cmn600_pmu_intr);
	}

	return (0);
}

void
pmc_cmn600_finalize(struct pmc_mdep *md)
{
	struct pmc_classdep *pcd;
	int i, npmc;

	KASSERT(md->pmd_classdep[PMC_MDEP_CLASS_INDEX_CMN600].pcd_class ==
	    PMC_CLASS_CMN600_PMU, ("[cmn600,%d] pmc class mismatch",
	    __LINE__));

	pcd = &md->pmd_classdep[PMC_MDEP_CLASS_INDEX_CMN600];

	npmc = pcd->pcd_num;
	for (i = 0; i < npmc; i++) {
		free(cmn600_pmcdesc[i]->pd_phw, M_PMC);
		free(cmn600_pmcdesc[i], M_PMC);
	}
	free(cmn600_pmcdesc, M_PMC);
	cmn600_pmcdesc = NULL;
}

MODULE_DEPEND(pmc, cmn600, 1, 1, 1);
