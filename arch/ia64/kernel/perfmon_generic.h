/*
 * This file contains the architected PMU register description tables
 * and pmc checker used by perfmon.c.
 *
 * Copyright (C) 2002  Hewlett Packard Co
 *               Stephane Eranian <eranian@hpl.hp.com>
 */
#define RDEP(x)	(1UL<<(x))

#if defined(CONFIG_ITANIUM) || defined (CONFIG_MCKINLEY)
#error "This file should not be used when CONFIG_ITANIUM or CONFIG_MCKINLEY is defined"
#endif

static pfm_reg_desc_t pmc_gen_desc[PMU_MAX_PMCS]={
/* pmc0  */ { PFM_REG_CONTROL , 0, 0x1UL, -1UL, NULL, NULL, {0UL,0UL, 0UL, 0UL}, {0UL,0UL, 0UL, 0UL}},
/* pmc1  */ { PFM_REG_CONTROL , 0, 0x0UL, -1UL, NULL, NULL, {0UL,0UL, 0UL, 0UL}, {0UL,0UL, 0UL, 0UL}},
/* pmc2  */ { PFM_REG_CONTROL , 0, 0x0UL, -1UL, NULL, NULL, {0UL,0UL, 0UL, 0UL}, {0UL,0UL, 0UL, 0UL}},
/* pmc3  */ { PFM_REG_CONTROL , 0, 0x0UL, -1UL, NULL, NULL, {0UL,0UL, 0UL, 0UL}, {0UL,0UL, 0UL, 0UL}},
/* pmc4  */ { PFM_REG_COUNTING, 0, 0x0UL, -1UL, NULL, NULL, {RDEP(4),0UL, 0UL, 0UL}, {0UL,0UL, 0UL, 0UL}},
/* pmc5  */ { PFM_REG_COUNTING, 0, 0x0UL, -1UL, NULL, NULL, {RDEP(5),0UL, 0UL, 0UL}, {0UL,0UL, 0UL, 0UL}},
/* pmc6  */ { PFM_REG_COUNTING, 0, 0x0UL, -1UL, NULL, NULL, {RDEP(6),0UL, 0UL, 0UL}, {0UL,0UL, 0UL, 0UL}},
/* pmc7  */ { PFM_REG_COUNTING, 0, 0x0UL, -1UL, NULL, NULL, {RDEP(7),0UL, 0UL, 0UL}, {0UL,0UL, 0UL, 0UL}},
	    { PFM_REG_END     , 0, 0x0UL, -1UL, NULL, NULL, {0,}, {0,}}, /* end marker */
};

static pfm_reg_desc_t pmd_gen_desc[PMU_MAX_PMDS]={
/* pmd0  */ { PFM_REG_NOTIMPL , 0, 0x0UL, -1UL, NULL, NULL, {0,}, {0,}},
/* pmd1  */ { PFM_REG_NOTIMPL , 0, 0x0UL, -1UL, NULL, NULL, {0,}, {0,}},
/* pmd2  */ { PFM_REG_NOTIMPL , 0, 0x0UL, -1UL, NULL, NULL, {0,}, {0,}},
/* pmd3  */ { PFM_REG_NOTIMPL , 0, 0x0UL, -1UL, NULL, NULL, {0,}, {0,}},
/* pmd4  */ { PFM_REG_COUNTING, 0, 0x0UL, -1UL, NULL, NULL, {0UL,0UL, 0UL, 0UL}, {RDEP(4),0UL, 0UL, 0UL}},
/* pmd5  */ { PFM_REG_COUNTING, 0, 0x0UL, -1UL, NULL, NULL, {0UL,0UL, 0UL, 0UL}, {RDEP(5),0UL, 0UL, 0UL}},
/* pmd6  */ { PFM_REG_COUNTING, 0, 0x0UL, -1UL, NULL, NULL, {0UL,0UL, 0UL, 0UL}, {RDEP(6),0UL, 0UL, 0UL}},
/* pmd7  */ { PFM_REG_COUNTING, 0, 0x0UL, -1UL, NULL, NULL, {0UL,0UL, 0UL, 0UL}, {RDEP(7),0UL, 0UL, 0UL}},
	    { PFM_REG_END     , 0, 0x0UL, -1UL, NULL, NULL, {0,}, {0,}}, /* end marker */
};

/*
 * impl_pmcs, impl_pmds are computed at runtime to minimize errors!
 */
static pmu_config_t pmu_conf={
	disabled:	1,
	ovfl_val:	(1UL << 32) - 1,
	num_ibrs:	8,
	num_dbrs:	8,
	pmd_desc:	pfm_gen_pmd_desc,
	pmc_desc:	pfm_gen_pmc_desc
};
