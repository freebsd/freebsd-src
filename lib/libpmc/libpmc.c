/*-
 * Copyright (c) 2003-2008 Joseph Koshy
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
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/types.h>
#include <sys/module.h>
#include <sys/pmc.h>
#include <sys/syscall.h>

#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <pmc.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>

#include "libpmcinternal.h"

/* Function prototypes */
#if defined(__i386__)
static int k7_allocate_pmc(enum pmc_event _pe, char *_ctrspec,
    struct pmc_op_pmcallocate *_pmc_config);
#endif
#if defined(__amd64__) || defined(__i386__)
static int iaf_allocate_pmc(enum pmc_event _pe, char *_ctrspec,
    struct pmc_op_pmcallocate *_pmc_config);
static int iap_allocate_pmc(enum pmc_event _pe, char *_ctrspec,
    struct pmc_op_pmcallocate *_pmc_config);
static int ucf_allocate_pmc(enum pmc_event _pe, char *_ctrspec,
    struct pmc_op_pmcallocate *_pmc_config);
static int ucp_allocate_pmc(enum pmc_event _pe, char *_ctrspec,
    struct pmc_op_pmcallocate *_pmc_config);
static int k8_allocate_pmc(enum pmc_event _pe, char *_ctrspec,
    struct pmc_op_pmcallocate *_pmc_config);
static int p4_allocate_pmc(enum pmc_event _pe, char *_ctrspec,
    struct pmc_op_pmcallocate *_pmc_config);
#endif
#if defined(__i386__)
static int p5_allocate_pmc(enum pmc_event _pe, char *_ctrspec,
    struct pmc_op_pmcallocate *_pmc_config);
static int p6_allocate_pmc(enum pmc_event _pe, char *_ctrspec,
    struct pmc_op_pmcallocate *_pmc_config);
#endif
#if defined(__amd64__) || defined(__i386__)
static int tsc_allocate_pmc(enum pmc_event _pe, char *_ctrspec,
    struct pmc_op_pmcallocate *_pmc_config);
#endif
#if defined(__XSCALE__)
static int xscale_allocate_pmc(enum pmc_event _pe, char *_ctrspec,
    struct pmc_op_pmcallocate *_pmc_config);
#endif

#if defined(__mips__)
static int mips24k_allocate_pmc(enum pmc_event _pe, char* ctrspec,
			     struct pmc_op_pmcallocate *_pmc_config);
#endif /* __mips__ */


#define PMC_CALL(cmd, params)				\
	syscall(pmc_syscall, PMC_OP_##cmd, (params))

/*
 * Event aliases provide a way for the user to ask for generic events
 * like "cache-misses", or "instructions-retired".  These aliases are
 * mapped to the appropriate canonical event descriptions using a
 * lookup table.
 */
struct pmc_event_alias {
	const char	*pm_alias;
	const char	*pm_spec;
};

static const struct pmc_event_alias *pmc_mdep_event_aliases;

/*
 * The pmc_event_descr structure maps symbolic names known to the user
 * to integer codes used by the PMC KLD.
 */
struct pmc_event_descr {
	const char	*pm_ev_name;
	enum pmc_event	pm_ev_code;
};

/*
 * The pmc_class_descr structure maps class name prefixes for
 * event names to event tables and other PMC class data.
 */
struct pmc_class_descr {
	const char	*pm_evc_name;
	size_t		pm_evc_name_size;
	enum pmc_class	pm_evc_class;
	const struct pmc_event_descr *pm_evc_event_table;
	size_t		pm_evc_event_table_size;
	int		(*pm_evc_allocate_pmc)(enum pmc_event _pe,
			    char *_ctrspec, struct pmc_op_pmcallocate *_pa);
};

#define	PMC_TABLE_SIZE(N)	(sizeof(N)/sizeof(N[0]))
#define	PMC_EVENT_TABLE_SIZE(N)	PMC_TABLE_SIZE(N##_event_table)

#undef	__PMC_EV
#define	__PMC_EV(C,N) { #N, PMC_EV_ ## C ## _ ## N },

/*
 * PMC_CLASSDEP_TABLE(NAME, CLASS)
 *
 * Define a table mapping event names and aliases to HWPMC event IDs.
 */
#define	PMC_CLASSDEP_TABLE(N, C)				\
	static const struct pmc_event_descr N##_event_table[] =	\
	{							\
		__PMC_EV_##C()					\
	}

PMC_CLASSDEP_TABLE(iaf, IAF);
PMC_CLASSDEP_TABLE(k7, K7);
PMC_CLASSDEP_TABLE(k8, K8);
PMC_CLASSDEP_TABLE(p4, P4);
PMC_CLASSDEP_TABLE(p5, P5);
PMC_CLASSDEP_TABLE(p6, P6);
PMC_CLASSDEP_TABLE(xscale, XSCALE);
PMC_CLASSDEP_TABLE(mips24k, MIPS24K);
PMC_CLASSDEP_TABLE(ucf, UCF);

#undef	__PMC_EV_ALIAS
#define	__PMC_EV_ALIAS(N,CODE) 	{ N, PMC_EV_##CODE },

static const struct pmc_event_descr atom_event_table[] =
{
	__PMC_EV_ALIAS_ATOM()
};

static const struct pmc_event_descr core_event_table[] =
{
	__PMC_EV_ALIAS_CORE()
};


static const struct pmc_event_descr core2_event_table[] =
{
	__PMC_EV_ALIAS_CORE2()
};

static const struct pmc_event_descr corei7_event_table[] =
{
	__PMC_EV_ALIAS_COREI7()
};

static const struct pmc_event_descr westmere_event_table[] =
{
	__PMC_EV_ALIAS_WESTMERE()
};

static const struct pmc_event_descr corei7uc_event_table[] =
{
	__PMC_EV_ALIAS_COREI7UC()
};

static const struct pmc_event_descr westmereuc_event_table[] =
{
	__PMC_EV_ALIAS_WESTMEREUC()
};

/*
 * PMC_MDEP_TABLE(NAME, PRIMARYCLASS, ADDITIONAL_CLASSES...)
 *
 * Map a CPU to the PMC classes it supports.
 */
#define	PMC_MDEP_TABLE(N,C,...)				\
	static const enum pmc_class N##_pmc_classes[] = {	\
		PMC_CLASS_##C, __VA_ARGS__			\
	}

PMC_MDEP_TABLE(atom, IAP, PMC_CLASS_IAF, PMC_CLASS_TSC);
PMC_MDEP_TABLE(core, IAP, PMC_CLASS_TSC);
PMC_MDEP_TABLE(core2, IAP, PMC_CLASS_IAF, PMC_CLASS_TSC);
PMC_MDEP_TABLE(corei7, IAP, PMC_CLASS_IAF, PMC_CLASS_TSC, PMC_CLASS_UCF, PMC_CLASS_UCP);
PMC_MDEP_TABLE(westmere, IAP, PMC_CLASS_IAF, PMC_CLASS_TSC, PMC_CLASS_UCF, PMC_CLASS_UCP);
PMC_MDEP_TABLE(k7, K7, PMC_CLASS_TSC);
PMC_MDEP_TABLE(k8, K8, PMC_CLASS_TSC);
PMC_MDEP_TABLE(p4, P4, PMC_CLASS_TSC);
PMC_MDEP_TABLE(p5, P5, PMC_CLASS_TSC);
PMC_MDEP_TABLE(p6, P6, PMC_CLASS_TSC);
PMC_MDEP_TABLE(xscale, XSCALE, PMC_CLASS_XSCALE);
PMC_MDEP_TABLE(mips24k, MIPS24K, PMC_CLASS_MIPS24K);

static const struct pmc_event_descr tsc_event_table[] =
{
	__PMC_EV_TSC()
};

#undef	PMC_CLASS_TABLE_DESC
#define	PMC_CLASS_TABLE_DESC(NAME, CLASS, EVENTS, ALLOCATOR)	\
static const struct pmc_class_descr NAME##_class_table_descr =	\
	{							\
		.pm_evc_name  = #CLASS "-",			\
		.pm_evc_name_size = sizeof(#CLASS "-") - 1,	\
		.pm_evc_class = PMC_CLASS_##CLASS ,		\
		.pm_evc_event_table = EVENTS##_event_table ,	\
		.pm_evc_event_table_size = 			\
			PMC_EVENT_TABLE_SIZE(EVENTS),		\
		.pm_evc_allocate_pmc = ALLOCATOR##_allocate_pmc	\
	}

#if	defined(__i386__) || defined(__amd64__)
PMC_CLASS_TABLE_DESC(iaf, IAF, iaf, iaf);
PMC_CLASS_TABLE_DESC(atom, IAP, atom, iap);
PMC_CLASS_TABLE_DESC(core, IAP, core, iap);
PMC_CLASS_TABLE_DESC(core2, IAP, core2, iap);
PMC_CLASS_TABLE_DESC(corei7, IAP, corei7, iap);
PMC_CLASS_TABLE_DESC(westmere, IAP, westmere, iap);
PMC_CLASS_TABLE_DESC(ucf, UCF, ucf, ucf);
PMC_CLASS_TABLE_DESC(corei7uc, UCP, corei7uc, ucp);
PMC_CLASS_TABLE_DESC(westmereuc, UCP, westmereuc, ucp);
#endif
#if	defined(__i386__)
PMC_CLASS_TABLE_DESC(k7, K7, k7, k7);
#endif
#if	defined(__i386__) || defined(__amd64__)
PMC_CLASS_TABLE_DESC(k8, K8, k8, k8);
PMC_CLASS_TABLE_DESC(p4, P4, p4, p4);
#endif
#if	defined(__i386__)
PMC_CLASS_TABLE_DESC(p5, P5, p5, p5);
PMC_CLASS_TABLE_DESC(p6, P6, p6, p6);
#endif
#if	defined(__i386__) || defined(__amd64__)
PMC_CLASS_TABLE_DESC(tsc, TSC, tsc, tsc);
#endif
#if	defined(__XSCALE__)
PMC_CLASS_TABLE_DESC(xscale, XSCALE, xscale, xscale);
#endif

#if defined(__mips__)
PMC_CLASS_TABLE_DESC(mips24k, MIPS24K, mips24k, mips24k);
#endif /* __mips__ */

#undef	PMC_CLASS_TABLE_DESC

static const struct pmc_class_descr **pmc_class_table;
#define	PMC_CLASS_TABLE_SIZE	cpu_info.pm_nclass

static const enum pmc_class *pmc_mdep_class_list;
static size_t pmc_mdep_class_list_size;

/*
 * Mapping tables, mapping enumeration values to human readable
 * strings.
 */

static const char * pmc_capability_names[] = {
#undef	__PMC_CAP
#define	__PMC_CAP(N,V,D)	#N ,
	__PMC_CAPS()
};

static const char * pmc_class_names[] = {
#undef	__PMC_CLASS
#define __PMC_CLASS(C)	#C ,
	__PMC_CLASSES()
};

struct pmc_cputype_map {
	enum pmc_class	pm_cputype;
	const char	*pm_name;
};

static const struct pmc_cputype_map pmc_cputype_names[] = {
#undef	__PMC_CPU
#define	__PMC_CPU(S, V, D) { .pm_cputype = PMC_CPU_##S, .pm_name = #S } ,
	__PMC_CPUS()
};

static const char * pmc_disposition_names[] = {
#undef	__PMC_DISP
#define	__PMC_DISP(D)	#D ,
	__PMC_DISPOSITIONS()
};

static const char * pmc_mode_names[] = {
#undef  __PMC_MODE
#define __PMC_MODE(M,N)	#M ,
	__PMC_MODES()
};

static const char * pmc_state_names[] = {
#undef  __PMC_STATE
#define __PMC_STATE(S) #S ,
	__PMC_STATES()
};

static int pmc_syscall = -1;		/* filled in by pmc_init() */

static struct pmc_cpuinfo cpu_info;	/* filled in by pmc_init() */

/* Event masks for events */
struct pmc_masks {
	const char	*pm_name;
	const uint32_t	pm_value;
};
#define	PMCMASK(N,V)	{ .pm_name = #N, .pm_value = (V) }
#define	NULLMASK	{ .pm_name = NULL }

#if defined(__amd64__) || defined(__i386__)
static int
pmc_parse_mask(const struct pmc_masks *pmask, char *p, uint32_t *evmask)
{
	const struct pmc_masks *pm;
	char *q, *r;
	int c;

	if (pmask == NULL)	/* no mask keywords */
		return (-1);
	q = strchr(p, '=');	/* skip '=' */
	if (*++q == '\0')	/* no more data */
		return (-1);
	c = 0;			/* count of mask keywords seen */
	while ((r = strsep(&q, "+")) != NULL) {
		for (pm = pmask; pm->pm_name && strcasecmp(r, pm->pm_name);
		    pm++)
			;
		if (pm->pm_name == NULL) /* not found */
			return (-1);
		*evmask |= pm->pm_value;
		c++;
	}
	return (c);
}
#endif

#define	KWMATCH(p,kw)		(strcasecmp((p), (kw)) == 0)
#define	KWPREFIXMATCH(p,kw)	(strncasecmp((p), (kw), sizeof((kw)) - 1) == 0)
#define	EV_ALIAS(N,S)		{ .pm_alias = N, .pm_spec = S }

#if defined(__i386__)

/*
 * AMD K7 (Athlon) CPUs.
 */

static struct pmc_event_alias k7_aliases[] = {
	EV_ALIAS("branches",		"k7-retired-branches"),
	EV_ALIAS("branch-mispredicts",	"k7-retired-branches-mispredicted"),
	EV_ALIAS("cycles",		"tsc"),
	EV_ALIAS("dc-misses",		"k7-dc-misses"),
	EV_ALIAS("ic-misses",		"k7-ic-misses"),
	EV_ALIAS("instructions",	"k7-retired-instructions"),
	EV_ALIAS("interrupts",		"k7-hardware-interrupts"),
	EV_ALIAS(NULL, NULL)
};

#define	K7_KW_COUNT	"count"
#define	K7_KW_EDGE	"edge"
#define	K7_KW_INV	"inv"
#define	K7_KW_OS	"os"
#define	K7_KW_UNITMASK	"unitmask"
#define	K7_KW_USR	"usr"

static int
k7_allocate_pmc(enum pmc_event pe, char *ctrspec,
    struct pmc_op_pmcallocate *pmc_config)
{
	char		*e, *p, *q;
	int		c, has_unitmask;
	uint32_t	count, unitmask;

	pmc_config->pm_md.pm_amd.pm_amd_config = 0;
	pmc_config->pm_caps |= (PMC_CAP_READ | PMC_CAP_WRITE);

	if (pe == PMC_EV_K7_DC_REFILLS_FROM_L2 ||
	    pe == PMC_EV_K7_DC_REFILLS_FROM_SYSTEM ||
	    pe == PMC_EV_K7_DC_WRITEBACKS) {
		has_unitmask = 1;
		unitmask = AMD_PMC_UNITMASK_MOESI;
	} else
		unitmask = has_unitmask = 0;

	while ((p = strsep(&ctrspec, ",")) != NULL) {
		if (KWPREFIXMATCH(p, K7_KW_COUNT "=")) {
			q = strchr(p, '=');
			if (*++q == '\0') /* skip '=' */
				return (-1);

			count = strtol(q, &e, 0);
			if (e == q || *e != '\0')
				return (-1);

			pmc_config->pm_caps |= PMC_CAP_THRESHOLD;
			pmc_config->pm_md.pm_amd.pm_amd_config |=
			    AMD_PMC_TO_COUNTER(count);

		} else if (KWMATCH(p, K7_KW_EDGE)) {
			pmc_config->pm_caps |= PMC_CAP_EDGE;
		} else if (KWMATCH(p, K7_KW_INV)) {
			pmc_config->pm_caps |= PMC_CAP_INVERT;
		} else if (KWMATCH(p, K7_KW_OS)) {
			pmc_config->pm_caps |= PMC_CAP_SYSTEM;
		} else if (KWPREFIXMATCH(p, K7_KW_UNITMASK "=")) {
			if (has_unitmask == 0)
				return (-1);
			unitmask = 0;
			q = strchr(p, '=');
			if (*++q == '\0') /* skip '=' */
				return (-1);

			while ((c = tolower(*q++)) != 0)
				if (c == 'm')
					unitmask |= AMD_PMC_UNITMASK_M;
				else if (c == 'o')
					unitmask |= AMD_PMC_UNITMASK_O;
				else if (c == 'e')
					unitmask |= AMD_PMC_UNITMASK_E;
				else if (c == 's')
					unitmask |= AMD_PMC_UNITMASK_S;
				else if (c == 'i')
					unitmask |= AMD_PMC_UNITMASK_I;
				else if (c == '+')
					continue;
				else
					return (-1);

			if (unitmask == 0)
				return (-1);

		} else if (KWMATCH(p, K7_KW_USR)) {
			pmc_config->pm_caps |= PMC_CAP_USER;
		} else
			return (-1);
	}

	if (has_unitmask) {
		pmc_config->pm_caps |= PMC_CAP_QUALIFIER;
		pmc_config->pm_md.pm_amd.pm_amd_config |=
		    AMD_PMC_TO_UNITMASK(unitmask);
	}

	return (0);

}

#endif

#if defined(__amd64__) || defined(__i386__)

/*
 * Intel Core (Family 6, Model E) PMCs.
 */

static struct pmc_event_alias core_aliases[] = {
	EV_ALIAS("branches",		"iap-br-instr-ret"),
	EV_ALIAS("branch-mispredicts",	"iap-br-mispred-ret"),
	EV_ALIAS("cycles",		"tsc-tsc"),
	EV_ALIAS("ic-misses",		"iap-icache-misses"),
	EV_ALIAS("instructions",	"iap-instr-ret"),
	EV_ALIAS("interrupts",		"iap-core-hw-int-rx"),
	EV_ALIAS("unhalted-cycles",	"iap-unhalted-core-cycles"),
	EV_ALIAS(NULL, NULL)
};

/*
 * Intel Core2 (Family 6, Model F), Core2Extreme (Family 6, Model 17H)
 * and Atom (Family 6, model 1CH) PMCs.
 *
 * We map aliases to events on the fixed-function counters if these
 * are present.  Note that not all CPUs in this family contain fixed-function
 * counters.
 */

static struct pmc_event_alias core2_aliases[] = {
	EV_ALIAS("branches",		"iap-br-inst-retired.any"),
	EV_ALIAS("branch-mispredicts",	"iap-br-inst-retired.mispred"),
	EV_ALIAS("cycles",		"tsc-tsc"),
	EV_ALIAS("ic-misses",		"iap-l1i-misses"),
	EV_ALIAS("instructions",	"iaf-instr-retired.any"),
	EV_ALIAS("interrupts",		"iap-hw-int-rcv"),
	EV_ALIAS("unhalted-cycles",	"iaf-cpu-clk-unhalted.core"),
	EV_ALIAS(NULL, NULL)
};

static struct pmc_event_alias core2_aliases_without_iaf[] = {
	EV_ALIAS("branches",		"iap-br-inst-retired.any"),
	EV_ALIAS("branch-mispredicts",	"iap-br-inst-retired.mispred"),
	EV_ALIAS("cycles",		"tsc-tsc"),
	EV_ALIAS("ic-misses",		"iap-l1i-misses"),
	EV_ALIAS("instructions",	"iap-inst-retired.any_p"),
	EV_ALIAS("interrupts",		"iap-hw-int-rcv"),
	EV_ALIAS("unhalted-cycles",	"iap-cpu-clk-unhalted.core_p"),
	EV_ALIAS(NULL, NULL)
};

#define	atom_aliases			core2_aliases
#define	atom_aliases_without_iaf	core2_aliases_without_iaf
#define corei7_aliases			core2_aliases
#define corei7_aliases_without_iaf	core2_aliases_without_iaf
#define westmere_aliases		core2_aliases
#define westmere_aliases_without_iaf	core2_aliases_without_iaf

#define	IAF_KW_OS		"os"
#define	IAF_KW_USR		"usr"
#define	IAF_KW_ANYTHREAD	"anythread"

/*
 * Parse an event specifier for Intel fixed function counters.
 */
static int
iaf_allocate_pmc(enum pmc_event pe, char *ctrspec,
    struct pmc_op_pmcallocate *pmc_config)
{
	char *p;

	(void) pe;

	pmc_config->pm_caps |= (PMC_CAP_READ | PMC_CAP_WRITE);
	pmc_config->pm_md.pm_iaf.pm_iaf_flags = 0;

	while ((p = strsep(&ctrspec, ",")) != NULL) {
		if (KWMATCH(p, IAF_KW_OS))
			pmc_config->pm_caps |= PMC_CAP_SYSTEM;
		else if (KWMATCH(p, IAF_KW_USR))
			pmc_config->pm_caps |= PMC_CAP_USER;
		else if (KWMATCH(p, IAF_KW_ANYTHREAD))
			pmc_config->pm_md.pm_iaf.pm_iaf_flags |= IAF_ANY;
		else
			return (-1);
	}

	return (0);
}

/*
 * Core/Core2 support.
 */

#define	IAP_KW_AGENT		"agent"
#define	IAP_KW_ANYTHREAD	"anythread"
#define	IAP_KW_CACHESTATE	"cachestate"
#define	IAP_KW_CMASK		"cmask"
#define	IAP_KW_CORE		"core"
#define	IAP_KW_EDGE		"edge"
#define	IAP_KW_INV		"inv"
#define	IAP_KW_OS		"os"
#define	IAP_KW_PREFETCH		"prefetch"
#define	IAP_KW_SNOOPRESPONSE	"snoopresponse"
#define	IAP_KW_SNOOPTYPE	"snooptype"
#define	IAP_KW_TRANSITION	"trans"
#define	IAP_KW_USR		"usr"
#define	IAP_KW_RSP		"rsp"

static struct pmc_masks iap_core_mask[] = {
	PMCMASK(all,	(0x3 << 14)),
	PMCMASK(this,	(0x1 << 14)),
	NULLMASK
};

static struct pmc_masks iap_agent_mask[] = {
	PMCMASK(this,	0),
	PMCMASK(any,	(0x1 << 13)),
	NULLMASK
};

static struct pmc_masks iap_prefetch_mask[] = {
	PMCMASK(both,		(0x3 << 12)),
	PMCMASK(only,		(0x1 << 12)),
	PMCMASK(exclude,	0),
	NULLMASK
};

static struct pmc_masks iap_cachestate_mask[] = {
	PMCMASK(i,		(1 <<  8)),
	PMCMASK(s,		(1 <<  9)),
	PMCMASK(e,		(1 << 10)),
	PMCMASK(m,		(1 << 11)),
	NULLMASK
};

static struct pmc_masks iap_snoopresponse_mask[] = {
	PMCMASK(clean,		(1 << 8)),
	PMCMASK(hit,		(1 << 9)),
	PMCMASK(hitm,		(1 << 11)),
	NULLMASK
};

static struct pmc_masks iap_snooptype_mask[] = {
	PMCMASK(cmp2s,		(1 << 8)),
	PMCMASK(cmp2i,		(1 << 9)),
	NULLMASK
};

static struct pmc_masks iap_transition_mask[] = {
	PMCMASK(any,		0x00),
	PMCMASK(frequency,	0x10),
	NULLMASK
};

static struct pmc_masks iap_rsp_mask[] = {
	PMCMASK(DMND_DATA_RD,		(1 <<  0)),
	PMCMASK(DMND_RFO,		(1 <<  1)),
	PMCMASK(DMND_IFETCH,		(1 <<  2)),
	PMCMASK(WB,			(1 <<  3)),
	PMCMASK(PF_DATA_RD,		(1 <<  4)),
	PMCMASK(PF_RFO,			(1 <<  5)),
	PMCMASK(PF_IFETCH,		(1 <<  6)),
	PMCMASK(OTHER,			(1 <<  7)),
	PMCMASK(UNCORE_HIT,		(1 <<  8)),
	PMCMASK(OTHER_CORE_HIT_SNP,	(1 <<  9)),
	PMCMASK(OTHER_CORE_HITM,	(1 << 10)),
	PMCMASK(REMOTE_CACHE_FWD,	(1 << 12)),
	PMCMASK(REMOTE_DRAM,		(1 << 13)),
	PMCMASK(LOCAL_DRAM,		(1 << 14)),
	PMCMASK(NON_DRAM,		(1 << 15)),
	NULLMASK
};

static int
iap_allocate_pmc(enum pmc_event pe, char *ctrspec,
    struct pmc_op_pmcallocate *pmc_config)
{
	char *e, *p, *q;
	uint32_t cachestate, evmask, rsp;
	int count, n;

	pmc_config->pm_caps |= (PMC_CAP_READ | PMC_CAP_WRITE |
	    PMC_CAP_QUALIFIER);
	pmc_config->pm_md.pm_iap.pm_iap_config = 0;

	cachestate = evmask = rsp = 0;

	/* Parse additional modifiers if present */
	while ((p = strsep(&ctrspec, ",")) != NULL) {

		n = 0;
		if (KWPREFIXMATCH(p, IAP_KW_CMASK "=")) {
			q = strchr(p, '=');
			if (*++q == '\0') /* skip '=' */
				return (-1);
			count = strtol(q, &e, 0);
			if (e == q || *e != '\0')
				return (-1);
			pmc_config->pm_caps |= PMC_CAP_THRESHOLD;
			pmc_config->pm_md.pm_iap.pm_iap_config |=
			    IAP_CMASK(count);
		} else if (KWMATCH(p, IAP_KW_EDGE)) {
			pmc_config->pm_caps |= PMC_CAP_EDGE;
		} else if (KWMATCH(p, IAP_KW_INV)) {
			pmc_config->pm_caps |= PMC_CAP_INVERT;
		} else if (KWMATCH(p, IAP_KW_OS)) {
			pmc_config->pm_caps |= PMC_CAP_SYSTEM;
		} else if (KWMATCH(p, IAP_KW_USR)) {
			pmc_config->pm_caps |= PMC_CAP_USER;
		} else if (KWMATCH(p, IAP_KW_ANYTHREAD)) {
			pmc_config->pm_md.pm_iap.pm_iap_config |= IAP_ANY;
		} else if (KWPREFIXMATCH(p, IAP_KW_CORE "=")) {
			n = pmc_parse_mask(iap_core_mask, p, &evmask);
			if (n != 1)
				return (-1);
		} else if (KWPREFIXMATCH(p, IAP_KW_AGENT "=")) {
			n = pmc_parse_mask(iap_agent_mask, p, &evmask);
			if (n != 1)
				return (-1);
		} else if (KWPREFIXMATCH(p, IAP_KW_PREFETCH "=")) {
			n = pmc_parse_mask(iap_prefetch_mask, p, &evmask);
			if (n != 1)
				return (-1);
		} else if (KWPREFIXMATCH(p, IAP_KW_CACHESTATE "=")) {
			n = pmc_parse_mask(iap_cachestate_mask, p, &cachestate);
		} else if (cpu_info.pm_cputype == PMC_CPU_INTEL_CORE &&
		    KWPREFIXMATCH(p, IAP_KW_TRANSITION "=")) {
			n = pmc_parse_mask(iap_transition_mask, p, &evmask);
			if (n != 1)
				return (-1);
		} else if (cpu_info.pm_cputype == PMC_CPU_INTEL_ATOM ||
		    cpu_info.pm_cputype == PMC_CPU_INTEL_CORE2 ||
		    cpu_info.pm_cputype == PMC_CPU_INTEL_CORE2EXTREME) {
			if (KWPREFIXMATCH(p, IAP_KW_SNOOPRESPONSE "=")) {
				n = pmc_parse_mask(iap_snoopresponse_mask, p,
				    &evmask);
			} else if (KWPREFIXMATCH(p, IAP_KW_SNOOPTYPE "=")) {
				n = pmc_parse_mask(iap_snooptype_mask, p,
				    &evmask);
			} else
				return (-1);
		} else if (cpu_info.pm_cputype == PMC_CPU_INTEL_COREI7 ||
		    cpu_info.pm_cputype == PMC_CPU_INTEL_WESTMERE) {
			if (KWPREFIXMATCH(p, IAP_KW_RSP "=")) {
				n = pmc_parse_mask(iap_rsp_mask, p, &rsp);
			} else
				return (-1);
		} else
			return (-1);

		if (n < 0)	/* Parsing failed. */
			return (-1);
	}

	pmc_config->pm_md.pm_iap.pm_iap_config |= evmask;

	/*
	 * If the event requires a 'cachestate' qualifier but was not
	 * specified by the user, use a sensible default.
	 */
	switch (pe) {
	case PMC_EV_IAP_EVENT_28H: /* Core, Core2, Atom */
	case PMC_EV_IAP_EVENT_29H: /* Core, Core2, Atom */
	case PMC_EV_IAP_EVENT_2AH: /* Core, Core2, Atom */
	case PMC_EV_IAP_EVENT_2BH: /* Atom, Core2 */
	case PMC_EV_IAP_EVENT_2EH: /* Core, Core2, Atom */
	case PMC_EV_IAP_EVENT_30H: /* Core, Core2, Atom */
	case PMC_EV_IAP_EVENT_32H: /* Core */
	case PMC_EV_IAP_EVENT_40H: /* Core */
	case PMC_EV_IAP_EVENT_41H: /* Core */
	case PMC_EV_IAP_EVENT_42H: /* Core, Core2, Atom */
		if (cachestate == 0)
			cachestate = (0xF << 8);
		break;
	case PMC_EV_IAP_EVENT_77H: /* Atom */
		/* IAP_EVENT_77H only accepts a cachestate qualifier on the
		 * Atom processor
		 */
		if(cpu_info.pm_cputype == PMC_CPU_INTEL_ATOM && cachestate == 0)
			cachestate = (0xF << 8);
	    break;
	default:
		break;
	}

	pmc_config->pm_md.pm_iap.pm_iap_config |= cachestate;
	pmc_config->pm_md.pm_iap.pm_iap_rsp = rsp;

	return (0);
}

/*
 * Intel Uncore.
 */

static int
ucf_allocate_pmc(enum pmc_event pe, char *ctrspec,
    struct pmc_op_pmcallocate *pmc_config)
{
	(void) pe;
	(void) ctrspec;

	pmc_config->pm_caps |= (PMC_CAP_READ | PMC_CAP_WRITE);
	pmc_config->pm_md.pm_ucf.pm_ucf_flags = 0;

	return (0);
}

#define	UCP_KW_CMASK		"cmask"
#define	UCP_KW_EDGE		"edge"
#define	UCP_KW_INV		"inv"

static int
ucp_allocate_pmc(enum pmc_event pe, char *ctrspec,
    struct pmc_op_pmcallocate *pmc_config)
{
	char *e, *p, *q;
	int count, n;

	(void) pe;

	pmc_config->pm_caps |= (PMC_CAP_READ | PMC_CAP_WRITE |
	    PMC_CAP_QUALIFIER);
	pmc_config->pm_md.pm_ucp.pm_ucp_config = 0;

	/* Parse additional modifiers if present */
	while ((p = strsep(&ctrspec, ",")) != NULL) {

		n = 0;
		if (KWPREFIXMATCH(p, UCP_KW_CMASK "=")) {
			q = strchr(p, '=');
			if (*++q == '\0') /* skip '=' */
				return (-1);
			count = strtol(q, &e, 0);
			if (e == q || *e != '\0')
				return (-1);
			pmc_config->pm_caps |= PMC_CAP_THRESHOLD;
			pmc_config->pm_md.pm_ucp.pm_ucp_config |=
			    UCP_CMASK(count);
		} else if (KWMATCH(p, UCP_KW_EDGE)) {
			pmc_config->pm_caps |= PMC_CAP_EDGE;
		} else if (KWMATCH(p, UCP_KW_INV)) {
			pmc_config->pm_caps |= PMC_CAP_INVERT;
		} else
			return (-1);

		if (n < 0)	/* Parsing failed. */
			return (-1);
	}

	return (0);
}

/*
 * AMD K8 PMCs.
 *
 * These are very similar to AMD K7 PMCs, but support more kinds of
 * events.
 */

static struct pmc_event_alias k8_aliases[] = {
	EV_ALIAS("branches",		"k8-fr-retired-taken-branches"),
	EV_ALIAS("branch-mispredicts",
	    "k8-fr-retired-taken-branches-mispredicted"),
	EV_ALIAS("cycles",		"tsc"),
	EV_ALIAS("dc-misses",		"k8-dc-miss"),
	EV_ALIAS("ic-misses",		"k8-ic-miss"),
	EV_ALIAS("instructions",	"k8-fr-retired-x86-instructions"),
	EV_ALIAS("interrupts",		"k8-fr-taken-hardware-interrupts"),
	EV_ALIAS("unhalted-cycles",	"k8-bu-cpu-clk-unhalted"),
	EV_ALIAS(NULL, NULL)
};

#define	__K8MASK(N,V) PMCMASK(N,(1 << (V)))

/*
 * Parsing tables
 */

/* fp dispatched fpu ops */
static const struct pmc_masks k8_mask_fdfo[] = {
	__K8MASK(add-pipe-excluding-junk-ops,	0),
	__K8MASK(multiply-pipe-excluding-junk-ops,	1),
	__K8MASK(store-pipe-excluding-junk-ops,	2),
	__K8MASK(add-pipe-junk-ops,		3),
	__K8MASK(multiply-pipe-junk-ops,	4),
	__K8MASK(store-pipe-junk-ops,		5),
	NULLMASK
};

/* ls segment register loads */
static const struct pmc_masks k8_mask_lsrl[] = {
	__K8MASK(es,	0),
	__K8MASK(cs,	1),
	__K8MASK(ss,	2),
	__K8MASK(ds,	3),
	__K8MASK(fs,	4),
	__K8MASK(gs,	5),
	__K8MASK(hs,	6),
	NULLMASK
};

/* ls locked operation */
static const struct pmc_masks k8_mask_llo[] = {
	__K8MASK(locked-instructions,	0),
	__K8MASK(cycles-in-request,	1),
	__K8MASK(cycles-to-complete,	2),
	NULLMASK
};

/* dc refill from {l2,system} and dc copyback */
static const struct pmc_masks k8_mask_dc[] = {
	__K8MASK(invalid,	0),
	__K8MASK(shared,	1),
	__K8MASK(exclusive,	2),
	__K8MASK(owner,		3),
	__K8MASK(modified,	4),
	NULLMASK
};

/* dc one bit ecc error */
static const struct pmc_masks k8_mask_dobee[] = {
	__K8MASK(scrubber,	0),
	__K8MASK(piggyback,	1),
	NULLMASK
};

/* dc dispatched prefetch instructions */
static const struct pmc_masks k8_mask_ddpi[] = {
	__K8MASK(load,	0),
	__K8MASK(store,	1),
	__K8MASK(nta,	2),
	NULLMASK
};

/* dc dcache accesses by locks */
static const struct pmc_masks k8_mask_dabl[] = {
	__K8MASK(accesses,	0),
	__K8MASK(misses,	1),
	NULLMASK
};

/* bu internal l2 request */
static const struct pmc_masks k8_mask_bilr[] = {
	__K8MASK(ic-fill,	0),
	__K8MASK(dc-fill,	1),
	__K8MASK(tlb-reload,	2),
	__K8MASK(tag-snoop,	3),
	__K8MASK(cancelled,	4),
	NULLMASK
};

/* bu fill request l2 miss */
static const struct pmc_masks k8_mask_bfrlm[] = {
	__K8MASK(ic-fill,	0),
	__K8MASK(dc-fill,	1),
	__K8MASK(tlb-reload,	2),
	NULLMASK
};

/* bu fill into l2 */
static const struct pmc_masks k8_mask_bfil[] = {
	__K8MASK(dirty-l2-victim,	0),
	__K8MASK(victim-from-l2,	1),
	NULLMASK
};

/* fr retired fpu instructions */
static const struct pmc_masks k8_mask_frfi[] = {
	__K8MASK(x87,			0),
	__K8MASK(mmx-3dnow,		1),
	__K8MASK(packed-sse-sse2,	2),
	__K8MASK(scalar-sse-sse2,	3),
	NULLMASK
};

/* fr retired fastpath double op instructions */
static const struct pmc_masks k8_mask_frfdoi[] = {
	__K8MASK(low-op-pos-0,		0),
	__K8MASK(low-op-pos-1,		1),
	__K8MASK(low-op-pos-2,		2),
	NULLMASK
};

/* fr fpu exceptions */
static const struct pmc_masks k8_mask_ffe[] = {
	__K8MASK(x87-reclass-microfaults,	0),
	__K8MASK(sse-retype-microfaults,	1),
	__K8MASK(sse-reclass-microfaults,	2),
	__K8MASK(sse-and-x87-microtraps,	3),
	NULLMASK
};

/* nb memory controller page access event */
static const struct pmc_masks k8_mask_nmcpae[] = {
	__K8MASK(page-hit,	0),
	__K8MASK(page-miss,	1),
	__K8MASK(page-conflict,	2),
	NULLMASK
};

/* nb memory controller turnaround */
static const struct pmc_masks k8_mask_nmct[] = {
	__K8MASK(dimm-turnaround,		0),
	__K8MASK(read-to-write-turnaround,	1),
	__K8MASK(write-to-read-turnaround,	2),
	NULLMASK
};

/* nb memory controller bypass saturation */
static const struct pmc_masks k8_mask_nmcbs[] = {
	__K8MASK(memory-controller-hi-pri-bypass,	0),
	__K8MASK(memory-controller-lo-pri-bypass,	1),
	__K8MASK(dram-controller-interface-bypass,	2),
	__K8MASK(dram-controller-queue-bypass,		3),
	NULLMASK
};

/* nb sized commands */
static const struct pmc_masks k8_mask_nsc[] = {
	__K8MASK(nonpostwrszbyte,	0),
	__K8MASK(nonpostwrszdword,	1),
	__K8MASK(postwrszbyte,		2),
	__K8MASK(postwrszdword,		3),
	__K8MASK(rdszbyte,		4),
	__K8MASK(rdszdword,		5),
	__K8MASK(rdmodwr,		6),
	NULLMASK
};

/* nb probe result */
static const struct pmc_masks k8_mask_npr[] = {
	__K8MASK(probe-miss,		0),
	__K8MASK(probe-hit,		1),
	__K8MASK(probe-hit-dirty-no-memory-cancel, 2),
	__K8MASK(probe-hit-dirty-with-memory-cancel, 3),
	NULLMASK
};

/* nb hypertransport bus bandwidth */
static const struct pmc_masks k8_mask_nhbb[] = { /* HT bus bandwidth */
	__K8MASK(command,	0),
	__K8MASK(data,	1),
	__K8MASK(buffer-release, 2),
	__K8MASK(nop,	3),
	NULLMASK
};

#undef	__K8MASK

#define	K8_KW_COUNT	"count"
#define	K8_KW_EDGE	"edge"
#define	K8_KW_INV	"inv"
#define	K8_KW_MASK	"mask"
#define	K8_KW_OS	"os"
#define	K8_KW_USR	"usr"

static int
k8_allocate_pmc(enum pmc_event pe, char *ctrspec,
    struct pmc_op_pmcallocate *pmc_config)
{
	char		*e, *p, *q;
	int		n;
	uint32_t	count, evmask;
	const struct pmc_masks	*pm, *pmask;

	pmc_config->pm_caps |= (PMC_CAP_READ | PMC_CAP_WRITE);
	pmc_config->pm_md.pm_amd.pm_amd_config = 0;

	pmask = NULL;
	evmask = 0;

#define	__K8SETMASK(M) pmask = k8_mask_##M

	/* setup parsing tables */
	switch (pe) {
	case PMC_EV_K8_FP_DISPATCHED_FPU_OPS:
		__K8SETMASK(fdfo);
		break;
	case PMC_EV_K8_LS_SEGMENT_REGISTER_LOAD:
		__K8SETMASK(lsrl);
		break;
	case PMC_EV_K8_LS_LOCKED_OPERATION:
		__K8SETMASK(llo);
		break;
	case PMC_EV_K8_DC_REFILL_FROM_L2:
	case PMC_EV_K8_DC_REFILL_FROM_SYSTEM:
	case PMC_EV_K8_DC_COPYBACK:
		__K8SETMASK(dc);
		break;
	case PMC_EV_K8_DC_ONE_BIT_ECC_ERROR:
		__K8SETMASK(dobee);
		break;
	case PMC_EV_K8_DC_DISPATCHED_PREFETCH_INSTRUCTIONS:
		__K8SETMASK(ddpi);
		break;
	case PMC_EV_K8_DC_DCACHE_ACCESSES_BY_LOCKS:
		__K8SETMASK(dabl);
		break;
	case PMC_EV_K8_BU_INTERNAL_L2_REQUEST:
		__K8SETMASK(bilr);
		break;
	case PMC_EV_K8_BU_FILL_REQUEST_L2_MISS:
		__K8SETMASK(bfrlm);
		break;
	case PMC_EV_K8_BU_FILL_INTO_L2:
		__K8SETMASK(bfil);
		break;
	case PMC_EV_K8_FR_RETIRED_FPU_INSTRUCTIONS:
		__K8SETMASK(frfi);
		break;
	case PMC_EV_K8_FR_RETIRED_FASTPATH_DOUBLE_OP_INSTRUCTIONS:
		__K8SETMASK(frfdoi);
		break;
	case PMC_EV_K8_FR_FPU_EXCEPTIONS:
		__K8SETMASK(ffe);
		break;
	case PMC_EV_K8_NB_MEMORY_CONTROLLER_PAGE_ACCESS_EVENT:
		__K8SETMASK(nmcpae);
		break;
	case PMC_EV_K8_NB_MEMORY_CONTROLLER_TURNAROUND:
		__K8SETMASK(nmct);
		break;
	case PMC_EV_K8_NB_MEMORY_CONTROLLER_BYPASS_SATURATION:
		__K8SETMASK(nmcbs);
		break;
	case PMC_EV_K8_NB_SIZED_COMMANDS:
		__K8SETMASK(nsc);
		break;
	case PMC_EV_K8_NB_PROBE_RESULT:
		__K8SETMASK(npr);
		break;
	case PMC_EV_K8_NB_HT_BUS0_BANDWIDTH:
	case PMC_EV_K8_NB_HT_BUS1_BANDWIDTH:
	case PMC_EV_K8_NB_HT_BUS2_BANDWIDTH:
		__K8SETMASK(nhbb);
		break;

	default:
		break;		/* no options defined */
	}

	while ((p = strsep(&ctrspec, ",")) != NULL) {
		if (KWPREFIXMATCH(p, K8_KW_COUNT "=")) {
			q = strchr(p, '=');
			if (*++q == '\0') /* skip '=' */
				return (-1);

			count = strtol(q, &e, 0);
			if (e == q || *e != '\0')
				return (-1);

			pmc_config->pm_caps |= PMC_CAP_THRESHOLD;
			pmc_config->pm_md.pm_amd.pm_amd_config |=
			    AMD_PMC_TO_COUNTER(count);

		} else if (KWMATCH(p, K8_KW_EDGE)) {
			pmc_config->pm_caps |= PMC_CAP_EDGE;
		} else if (KWMATCH(p, K8_KW_INV)) {
			pmc_config->pm_caps |= PMC_CAP_INVERT;
		} else if (KWPREFIXMATCH(p, K8_KW_MASK "=")) {
			if ((n = pmc_parse_mask(pmask, p, &evmask)) < 0)
				return (-1);
			pmc_config->pm_caps |= PMC_CAP_QUALIFIER;
		} else if (KWMATCH(p, K8_KW_OS)) {
			pmc_config->pm_caps |= PMC_CAP_SYSTEM;
		} else if (KWMATCH(p, K8_KW_USR)) {
			pmc_config->pm_caps |= PMC_CAP_USER;
		} else
			return (-1);
	}

	/* other post processing */
	switch (pe) {
	case PMC_EV_K8_FP_DISPATCHED_FPU_OPS:
	case PMC_EV_K8_FP_CYCLES_WITH_NO_FPU_OPS_RETIRED:
	case PMC_EV_K8_FP_DISPATCHED_FPU_FAST_FLAG_OPS:
	case PMC_EV_K8_FR_RETIRED_FASTPATH_DOUBLE_OP_INSTRUCTIONS:
	case PMC_EV_K8_FR_RETIRED_FPU_INSTRUCTIONS:
	case PMC_EV_K8_FR_FPU_EXCEPTIONS:
		/* XXX only available in rev B and later */
		break;
	case PMC_EV_K8_DC_DCACHE_ACCESSES_BY_LOCKS:
		/* XXX only available in rev C and later */
		break;
	case PMC_EV_K8_LS_LOCKED_OPERATION:
		/* XXX CPU Rev A,B evmask is to be zero */
		if (evmask & (evmask - 1)) /* > 1 bit set */
			return (-1);
		if (evmask == 0) {
			evmask = 0x01; /* Rev C and later: #instrs */
			pmc_config->pm_caps |= PMC_CAP_QUALIFIER;
		}
		break;
	default:
		if (evmask == 0 && pmask != NULL) {
			for (pm = pmask; pm->pm_name; pm++)
				evmask |= pm->pm_value;
			pmc_config->pm_caps |= PMC_CAP_QUALIFIER;
		}
	}

	if (pmc_config->pm_caps & PMC_CAP_QUALIFIER)
		pmc_config->pm_md.pm_amd.pm_amd_config =
		    AMD_PMC_TO_UNITMASK(evmask);

	return (0);
}

#endif

#if defined(__amd64__) || defined(__i386__)

/*
 * Intel P4 PMCs
 */

static struct pmc_event_alias p4_aliases[] = {
	EV_ALIAS("branches",		"p4-branch-retired,mask=mmtp+mmtm"),
	EV_ALIAS("branch-mispredicts",	"p4-mispred-branch-retired"),
	EV_ALIAS("cycles",		"tsc"),
	EV_ALIAS("instructions",
	    "p4-instr-retired,mask=nbogusntag+nbogustag"),
	EV_ALIAS("unhalted-cycles",	"p4-global-power-events"),
	EV_ALIAS(NULL, NULL)
};

#define	P4_KW_ACTIVE	"active"
#define	P4_KW_ACTIVE_ANY "any"
#define	P4_KW_ACTIVE_BOTH "both"
#define	P4_KW_ACTIVE_NONE "none"
#define	P4_KW_ACTIVE_SINGLE "single"
#define	P4_KW_BUSREQTYPE "busreqtype"
#define	P4_KW_CASCADE	"cascade"
#define	P4_KW_EDGE	"edge"
#define	P4_KW_INV	"complement"
#define	P4_KW_OS	"os"
#define	P4_KW_MASK	"mask"
#define	P4_KW_PRECISE	"precise"
#define	P4_KW_TAG	"tag"
#define	P4_KW_THRESHOLD	"threshold"
#define	P4_KW_USR	"usr"

#define	__P4MASK(N,V) PMCMASK(N, (1 << (V)))

static const struct pmc_masks p4_mask_tcdm[] = { /* tc deliver mode */
	__P4MASK(dd, 0),
	__P4MASK(db, 1),
	__P4MASK(di, 2),
	__P4MASK(bd, 3),
	__P4MASK(bb, 4),
	__P4MASK(bi, 5),
	__P4MASK(id, 6),
	__P4MASK(ib, 7),
	NULLMASK
};

static const struct pmc_masks p4_mask_bfr[] = { /* bpu fetch request */
	__P4MASK(tcmiss, 0),
	NULLMASK,
};

static const struct pmc_masks p4_mask_ir[] = { /* itlb reference */
	__P4MASK(hit, 0),
	__P4MASK(miss, 1),
	__P4MASK(hit-uc, 2),
	NULLMASK
};

static const struct pmc_masks p4_mask_memcan[] = { /* memory cancel */
	__P4MASK(st-rb-full, 2),
	__P4MASK(64k-conf, 3),
	NULLMASK
};

static const struct pmc_masks p4_mask_memcomp[] = { /* memory complete */
	__P4MASK(lsc, 0),
	__P4MASK(ssc, 1),
	NULLMASK
};

static const struct pmc_masks p4_mask_lpr[] = { /* load port replay */
	__P4MASK(split-ld, 1),
	NULLMASK
};

static const struct pmc_masks p4_mask_spr[] = { /* store port replay */
	__P4MASK(split-st, 1),
	NULLMASK
};

static const struct pmc_masks p4_mask_mlr[] = { /* mob load replay */
	__P4MASK(no-sta, 1),
	__P4MASK(no-std, 3),
	__P4MASK(partial-data, 4),
	__P4MASK(unalgn-addr, 5),
	NULLMASK
};

static const struct pmc_masks p4_mask_pwt[] = { /* page walk type */
	__P4MASK(dtmiss, 0),
	__P4MASK(itmiss, 1),
	NULLMASK
};

static const struct pmc_masks p4_mask_bcr[] = { /* bsq cache reference */
	__P4MASK(rd-2ndl-hits, 0),
	__P4MASK(rd-2ndl-hite, 1),
	__P4MASK(rd-2ndl-hitm, 2),
	__P4MASK(rd-3rdl-hits, 3),
	__P4MASK(rd-3rdl-hite, 4),
	__P4MASK(rd-3rdl-hitm, 5),
	__P4MASK(rd-2ndl-miss, 8),
	__P4MASK(rd-3rdl-miss, 9),
	__P4MASK(wr-2ndl-miss, 10),
	NULLMASK
};

static const struct pmc_masks p4_mask_ia[] = { /* ioq allocation */
	__P4MASK(all-read, 5),
	__P4MASK(all-write, 6),
	__P4MASK(mem-uc, 7),
	__P4MASK(mem-wc, 8),
	__P4MASK(mem-wt, 9),
	__P4MASK(mem-wp, 10),
	__P4MASK(mem-wb, 11),
	__P4MASK(own, 13),
	__P4MASK(other, 14),
	__P4MASK(prefetch, 15),
	NULLMASK
};

static const struct pmc_masks p4_mask_iae[] = { /* ioq active entries */
	__P4MASK(all-read, 5),
	__P4MASK(all-write, 6),
	__P4MASK(mem-uc, 7),
	__P4MASK(mem-wc, 8),
	__P4MASK(mem-wt, 9),
	__P4MASK(mem-wp, 10),
	__P4MASK(mem-wb, 11),
	__P4MASK(own, 13),
	__P4MASK(other, 14),
	__P4MASK(prefetch, 15),
	NULLMASK
};

static const struct pmc_masks p4_mask_fda[] = { /* fsb data activity */
	__P4MASK(drdy-drv, 0),
	__P4MASK(drdy-own, 1),
	__P4MASK(drdy-other, 2),
	__P4MASK(dbsy-drv, 3),
	__P4MASK(dbsy-own, 4),
	__P4MASK(dbsy-other, 5),
	NULLMASK
};

static const struct pmc_masks p4_mask_ba[] = { /* bsq allocation */
	__P4MASK(req-type0, 0),
	__P4MASK(req-type1, 1),
	__P4MASK(req-len0, 2),
	__P4MASK(req-len1, 3),
	__P4MASK(req-io-type, 5),
	__P4MASK(req-lock-type, 6),
	__P4MASK(req-cache-type, 7),
	__P4MASK(req-split-type, 8),
	__P4MASK(req-dem-type, 9),
	__P4MASK(req-ord-type, 10),
	__P4MASK(mem-type0, 11),
	__P4MASK(mem-type1, 12),
	__P4MASK(mem-type2, 13),
	NULLMASK
};

static const struct pmc_masks p4_mask_sia[] = { /* sse input assist */
	__P4MASK(all, 15),
	NULLMASK
};

static const struct pmc_masks p4_mask_psu[] = { /* packed sp uop */
	__P4MASK(all, 15),
	NULLMASK
};

static const struct pmc_masks p4_mask_pdu[] = { /* packed dp uop */
	__P4MASK(all, 15),
	NULLMASK
};

static const struct pmc_masks p4_mask_ssu[] = { /* scalar sp uop */
	__P4MASK(all, 15),
	NULLMASK
};

static const struct pmc_masks p4_mask_sdu[] = { /* scalar dp uop */
	__P4MASK(all, 15),
	NULLMASK
};

static const struct pmc_masks p4_mask_64bmu[] = { /* 64 bit mmx uop */
	__P4MASK(all, 15),
	NULLMASK
};

static const struct pmc_masks p4_mask_128bmu[] = { /* 128 bit mmx uop */
	__P4MASK(all, 15),
	NULLMASK
};

static const struct pmc_masks p4_mask_xfu[] = { /* X87 fp uop */
	__P4MASK(all, 15),
	NULLMASK
};

static const struct pmc_masks p4_mask_xsmu[] = { /* x87 simd moves uop */
	__P4MASK(allp0, 3),
	__P4MASK(allp2, 4),
	NULLMASK
};

static const struct pmc_masks p4_mask_gpe[] = { /* global power events */
	__P4MASK(running, 0),
	NULLMASK
};

static const struct pmc_masks p4_mask_tmx[] = { /* TC ms xfer */
	__P4MASK(cisc, 0),
	NULLMASK
};

static const struct pmc_masks p4_mask_uqw[] = { /* uop queue writes */
	__P4MASK(from-tc-build, 0),
	__P4MASK(from-tc-deliver, 1),
	__P4MASK(from-rom, 2),
	NULLMASK
};

static const struct pmc_masks p4_mask_rmbt[] = {
	/* retired mispred branch type */
	__P4MASK(conditional, 1),
	__P4MASK(call, 2),
	__P4MASK(return, 3),
	__P4MASK(indirect, 4),
	NULLMASK
};

static const struct pmc_masks p4_mask_rbt[] = { /* retired branch type */
	__P4MASK(conditional, 1),
	__P4MASK(call, 2),
	__P4MASK(retired, 3),
	__P4MASK(indirect, 4),
	NULLMASK
};

static const struct pmc_masks p4_mask_rs[] = { /* resource stall */
	__P4MASK(sbfull, 5),
	NULLMASK
};

static const struct pmc_masks p4_mask_wb[] = { /* WC buffer */
	__P4MASK(wcb-evicts, 0),
	__P4MASK(wcb-full-evict, 1),
	NULLMASK
};

static const struct pmc_masks p4_mask_fee[] = { /* front end event */
	__P4MASK(nbogus, 0),
	__P4MASK(bogus, 1),
	NULLMASK
};

static const struct pmc_masks p4_mask_ee[] = { /* execution event */
	__P4MASK(nbogus0, 0),
	__P4MASK(nbogus1, 1),
	__P4MASK(nbogus2, 2),
	__P4MASK(nbogus3, 3),
	__P4MASK(bogus0, 4),
	__P4MASK(bogus1, 5),
	__P4MASK(bogus2, 6),
	__P4MASK(bogus3, 7),
	NULLMASK
};

static const struct pmc_masks p4_mask_re[] = { /* replay event */
	__P4MASK(nbogus, 0),
	__P4MASK(bogus, 1),
	NULLMASK
};

static const struct pmc_masks p4_mask_insret[] = { /* instr retired */
	__P4MASK(nbogusntag, 0),
	__P4MASK(nbogustag, 1),
	__P4MASK(bogusntag, 2),
	__P4MASK(bogustag, 3),
	NULLMASK
};

static const struct pmc_masks p4_mask_ur[] = { /* uops retired */
	__P4MASK(nbogus, 0),
	__P4MASK(bogus, 1),
	NULLMASK
};

static const struct pmc_masks p4_mask_ut[] = { /* uop type */
	__P4MASK(tagloads, 1),
	__P4MASK(tagstores, 2),
	NULLMASK
};

static const struct pmc_masks p4_mask_br[] = { /* branch retired */
	__P4MASK(mmnp, 0),
	__P4MASK(mmnm, 1),
	__P4MASK(mmtp, 2),
	__P4MASK(mmtm, 3),
	NULLMASK
};

static const struct pmc_masks p4_mask_mbr[] = { /* mispred branch retired */
	__P4MASK(nbogus, 0),
	NULLMASK
};

static const struct pmc_masks p4_mask_xa[] = { /* x87 assist */
	__P4MASK(fpsu, 0),
	__P4MASK(fpso, 1),
	__P4MASK(poao, 2),
	__P4MASK(poau, 3),
	__P4MASK(prea, 4),
	NULLMASK
};

static const struct pmc_masks p4_mask_machclr[] = { /* machine clear */
	__P4MASK(clear, 0),
	__P4MASK(moclear, 2),
	__P4MASK(smclear, 3),
	NULLMASK
};

/* P4 event parser */
static int
p4_allocate_pmc(enum pmc_event pe, char *ctrspec,
    struct pmc_op_pmcallocate *pmc_config)
{

	char	*e, *p, *q;
	int	count, has_tag, has_busreqtype, n;
	uint32_t evmask, cccractivemask;
	const struct pmc_masks *pm, *pmask;

	pmc_config->pm_caps |= (PMC_CAP_READ | PMC_CAP_WRITE);
	pmc_config->pm_md.pm_p4.pm_p4_cccrconfig =
	    pmc_config->pm_md.pm_p4.pm_p4_escrconfig = 0;

	pmask   = NULL;
	evmask  = 0;
	cccractivemask = 0x3;
	has_tag = has_busreqtype = 0;

#define	__P4SETMASK(M) do {				\
	pmask = p4_mask_##M;				\
} while (0)

	switch (pe) {
	case PMC_EV_P4_TC_DELIVER_MODE:
		__P4SETMASK(tcdm);
		break;
	case PMC_EV_P4_BPU_FETCH_REQUEST:
		__P4SETMASK(bfr);
		break;
	case PMC_EV_P4_ITLB_REFERENCE:
		__P4SETMASK(ir);
		break;
	case PMC_EV_P4_MEMORY_CANCEL:
		__P4SETMASK(memcan);
		break;
	case PMC_EV_P4_MEMORY_COMPLETE:
		__P4SETMASK(memcomp);
		break;
	case PMC_EV_P4_LOAD_PORT_REPLAY:
		__P4SETMASK(lpr);
		break;
	case PMC_EV_P4_STORE_PORT_REPLAY:
		__P4SETMASK(spr);
		break;
	case PMC_EV_P4_MOB_LOAD_REPLAY:
		__P4SETMASK(mlr);
		break;
	case PMC_EV_P4_PAGE_WALK_TYPE:
		__P4SETMASK(pwt);
		break;
	case PMC_EV_P4_BSQ_CACHE_REFERENCE:
		__P4SETMASK(bcr);
		break;
	case PMC_EV_P4_IOQ_ALLOCATION:
		__P4SETMASK(ia);
		has_busreqtype = 1;
		break;
	case PMC_EV_P4_IOQ_ACTIVE_ENTRIES:
		__P4SETMASK(iae);
		has_busreqtype = 1;
		break;
	case PMC_EV_P4_FSB_DATA_ACTIVITY:
		__P4SETMASK(fda);
		break;
	case PMC_EV_P4_BSQ_ALLOCATION:
		__P4SETMASK(ba);
		break;
	case PMC_EV_P4_SSE_INPUT_ASSIST:
		__P4SETMASK(sia);
		break;
	case PMC_EV_P4_PACKED_SP_UOP:
		__P4SETMASK(psu);
		break;
	case PMC_EV_P4_PACKED_DP_UOP:
		__P4SETMASK(pdu);
		break;
	case PMC_EV_P4_SCALAR_SP_UOP:
		__P4SETMASK(ssu);
		break;
	case PMC_EV_P4_SCALAR_DP_UOP:
		__P4SETMASK(sdu);
		break;
	case PMC_EV_P4_64BIT_MMX_UOP:
		__P4SETMASK(64bmu);
		break;
	case PMC_EV_P4_128BIT_MMX_UOP:
		__P4SETMASK(128bmu);
		break;
	case PMC_EV_P4_X87_FP_UOP:
		__P4SETMASK(xfu);
		break;
	case PMC_EV_P4_X87_SIMD_MOVES_UOP:
		__P4SETMASK(xsmu);
		break;
	case PMC_EV_P4_GLOBAL_POWER_EVENTS:
		__P4SETMASK(gpe);
		break;
	case PMC_EV_P4_TC_MS_XFER:
		__P4SETMASK(tmx);
		break;
	case PMC_EV_P4_UOP_QUEUE_WRITES:
		__P4SETMASK(uqw);
		break;
	case PMC_EV_P4_RETIRED_MISPRED_BRANCH_TYPE:
		__P4SETMASK(rmbt);
		break;
	case PMC_EV_P4_RETIRED_BRANCH_TYPE:
		__P4SETMASK(rbt);
		break;
	case PMC_EV_P4_RESOURCE_STALL:
		__P4SETMASK(rs);
		break;
	case PMC_EV_P4_WC_BUFFER:
		__P4SETMASK(wb);
		break;
	case PMC_EV_P4_BSQ_ACTIVE_ENTRIES:
	case PMC_EV_P4_B2B_CYCLES:
	case PMC_EV_P4_BNR:
	case PMC_EV_P4_SNOOP:
	case PMC_EV_P4_RESPONSE:
		break;
	case PMC_EV_P4_FRONT_END_EVENT:
		__P4SETMASK(fee);
		break;
	case PMC_EV_P4_EXECUTION_EVENT:
		__P4SETMASK(ee);
		break;
	case PMC_EV_P4_REPLAY_EVENT:
		__P4SETMASK(re);
		break;
	case PMC_EV_P4_INSTR_RETIRED:
		__P4SETMASK(insret);
		break;
	case PMC_EV_P4_UOPS_RETIRED:
		__P4SETMASK(ur);
		break;
	case PMC_EV_P4_UOP_TYPE:
		__P4SETMASK(ut);
		break;
	case PMC_EV_P4_BRANCH_RETIRED:
		__P4SETMASK(br);
		break;
	case PMC_EV_P4_MISPRED_BRANCH_RETIRED:
		__P4SETMASK(mbr);
		break;
	case PMC_EV_P4_X87_ASSIST:
		__P4SETMASK(xa);
		break;
	case PMC_EV_P4_MACHINE_CLEAR:
		__P4SETMASK(machclr);
		break;
	default:
		return (-1);
	}

	/* process additional flags */
	while ((p = strsep(&ctrspec, ",")) != NULL) {
		if (KWPREFIXMATCH(p, P4_KW_ACTIVE)) {
			q = strchr(p, '=');
			if (*++q == '\0') /* skip '=' */
				return (-1);

			if (strcasecmp(q, P4_KW_ACTIVE_NONE) == 0)
				cccractivemask = 0x0;
			else if (strcasecmp(q, P4_KW_ACTIVE_SINGLE) == 0)
				cccractivemask = 0x1;
			else if (strcasecmp(q, P4_KW_ACTIVE_BOTH) == 0)
				cccractivemask = 0x2;
			else if (strcasecmp(q, P4_KW_ACTIVE_ANY) == 0)
				cccractivemask = 0x3;
			else
				return (-1);

		} else if (KWPREFIXMATCH(p, P4_KW_BUSREQTYPE)) {
			if (has_busreqtype == 0)
				return (-1);

			q = strchr(p, '=');
			if (*++q == '\0') /* skip '=' */
				return (-1);

			count = strtol(q, &e, 0);
			if (e == q || *e != '\0')
				return (-1);
			evmask = (evmask & ~0x1F) | (count & 0x1F);
		} else if (KWMATCH(p, P4_KW_CASCADE))
			pmc_config->pm_caps |= PMC_CAP_CASCADE;
		else if (KWMATCH(p, P4_KW_EDGE))
			pmc_config->pm_caps |= PMC_CAP_EDGE;
		else if (KWMATCH(p, P4_KW_INV))
			pmc_config->pm_caps |= PMC_CAP_INVERT;
		else if (KWPREFIXMATCH(p, P4_KW_MASK "=")) {
			if ((n = pmc_parse_mask(pmask, p, &evmask)) < 0)
				return (-1);
			pmc_config->pm_caps |= PMC_CAP_QUALIFIER;
		} else if (KWMATCH(p, P4_KW_OS))
			pmc_config->pm_caps |= PMC_CAP_SYSTEM;
		else if (KWMATCH(p, P4_KW_PRECISE))
			pmc_config->pm_caps |= PMC_CAP_PRECISE;
		else if (KWPREFIXMATCH(p, P4_KW_TAG "=")) {
			if (has_tag == 0)
				return (-1);

			q = strchr(p, '=');
			if (*++q == '\0') /* skip '=' */
				return (-1);

			count = strtol(q, &e, 0);
			if (e == q || *e != '\0')
				return (-1);

			pmc_config->pm_caps |= PMC_CAP_TAGGING;
			pmc_config->pm_md.pm_p4.pm_p4_escrconfig |=
			    P4_ESCR_TO_TAG_VALUE(count);
		} else if (KWPREFIXMATCH(p, P4_KW_THRESHOLD "=")) {
			q = strchr(p, '=');
			if (*++q == '\0') /* skip '=' */
				return (-1);

			count = strtol(q, &e, 0);
			if (e == q || *e != '\0')
				return (-1);

			pmc_config->pm_caps |= PMC_CAP_THRESHOLD;
			pmc_config->pm_md.pm_p4.pm_p4_cccrconfig &=
			    ~P4_CCCR_THRESHOLD_MASK;
			pmc_config->pm_md.pm_p4.pm_p4_cccrconfig |=
			    P4_CCCR_TO_THRESHOLD(count);
		} else if (KWMATCH(p, P4_KW_USR))
			pmc_config->pm_caps |= PMC_CAP_USER;
		else
			return (-1);
	}

	/* other post processing */
	if (pe == PMC_EV_P4_IOQ_ALLOCATION ||
	    pe == PMC_EV_P4_FSB_DATA_ACTIVITY ||
	    pe == PMC_EV_P4_BSQ_ALLOCATION)
		pmc_config->pm_caps |= PMC_CAP_EDGE;

	/* fill in thread activity mask */
	pmc_config->pm_md.pm_p4.pm_p4_cccrconfig |=
	    P4_CCCR_TO_ACTIVE_THREAD(cccractivemask);

	if (evmask)
		pmc_config->pm_caps |= PMC_CAP_QUALIFIER;

	switch (pe) {
	case PMC_EV_P4_FSB_DATA_ACTIVITY:
		if ((evmask & 0x06) == 0x06 ||
		    (evmask & 0x18) == 0x18)
			return (-1); /* can't have own+other bits together */
		if (evmask == 0) /* default:drdy-{drv,own}+dbsy{drv,own} */
			evmask = 0x1D;
		break;
	case PMC_EV_P4_MACHINE_CLEAR:
		/* only one bit is allowed to be set */
		if ((evmask & (evmask - 1)) != 0)
			return (-1);
		if (evmask == 0) {
			evmask = 0x1;	/* 'CLEAR' */
			pmc_config->pm_caps |= PMC_CAP_QUALIFIER;
		}
		break;
	default:
		if (evmask == 0 && pmask) {
			for (pm = pmask; pm->pm_name; pm++)
				evmask |= pm->pm_value;
			pmc_config->pm_caps |= PMC_CAP_QUALIFIER;
		}
	}

	pmc_config->pm_md.pm_p4.pm_p4_escrconfig =
	    P4_ESCR_TO_EVENT_MASK(evmask);

	return (0);
}

#endif

#if defined(__i386__)

/*
 * Pentium style PMCs
 */

static struct pmc_event_alias p5_aliases[] = {
	EV_ALIAS("branches",		"p5-taken-branches"),
	EV_ALIAS("cycles",		"tsc"),
	EV_ALIAS("dc-misses",		"p5-data-read-miss-or-write-miss"),
	EV_ALIAS("ic-misses",		"p5-code-cache-miss"),
	EV_ALIAS("instructions",	"p5-instructions-executed"),
	EV_ALIAS("interrupts",		"p5-hardware-interrupts"),
	EV_ALIAS("unhalted-cycles",
	    "p5-number-of-cycles-not-in-halt-state"),
	EV_ALIAS(NULL, NULL)
};

static int
p5_allocate_pmc(enum pmc_event pe, char *ctrspec,
    struct pmc_op_pmcallocate *pmc_config)
{
	return (-1 || pe || ctrspec || pmc_config); /* shut up gcc */
}

/*
 * Pentium Pro style PMCs.  These PMCs are found in Pentium II, Pentium III,
 * and Pentium M CPUs.
 */

static struct pmc_event_alias p6_aliases[] = {
	EV_ALIAS("branches",		"p6-br-inst-retired"),
	EV_ALIAS("branch-mispredicts",	"p6-br-miss-pred-retired"),
	EV_ALIAS("cycles",		"tsc"),
	EV_ALIAS("dc-misses",		"p6-dcu-lines-in"),
	EV_ALIAS("ic-misses",		"p6-ifu-fetch-miss"),
	EV_ALIAS("instructions",	"p6-inst-retired"),
	EV_ALIAS("interrupts",		"p6-hw-int-rx"),
	EV_ALIAS("unhalted-cycles",	"p6-cpu-clk-unhalted"),
	EV_ALIAS(NULL, NULL)
};

#define	P6_KW_CMASK	"cmask"
#define	P6_KW_EDGE	"edge"
#define	P6_KW_INV	"inv"
#define	P6_KW_OS	"os"
#define	P6_KW_UMASK	"umask"
#define	P6_KW_USR	"usr"

static struct pmc_masks p6_mask_mesi[] = {
	PMCMASK(m,	0x01),
	PMCMASK(e,	0x02),
	PMCMASK(s,	0x04),
	PMCMASK(i,	0x08),
	NULLMASK
};

static struct pmc_masks p6_mask_mesihw[] = {
	PMCMASK(m,	0x01),
	PMCMASK(e,	0x02),
	PMCMASK(s,	0x04),
	PMCMASK(i,	0x08),
	PMCMASK(nonhw,	0x00),
	PMCMASK(hw,	0x10),
	PMCMASK(both,	0x30),
	NULLMASK
};

static struct pmc_masks p6_mask_hw[] = {
	PMCMASK(nonhw,	0x00),
	PMCMASK(hw,	0x10),
	PMCMASK(both,	0x30),
	NULLMASK
};

static struct pmc_masks p6_mask_any[] = {
	PMCMASK(self,	0x00),
	PMCMASK(any,	0x20),
	NULLMASK
};

static struct pmc_masks p6_mask_ekp[] = {
	PMCMASK(nta,	0x00),
	PMCMASK(t1,	0x01),
	PMCMASK(t2,	0x02),
	PMCMASK(wos,	0x03),
	NULLMASK
};

static struct pmc_masks p6_mask_pps[] = {
	PMCMASK(packed-and-scalar, 0x00),
	PMCMASK(scalar,	0x01),
	NULLMASK
};

static struct pmc_masks p6_mask_mite[] = {
	PMCMASK(packed-multiply,	 0x01),
	PMCMASK(packed-shift,		0x02),
	PMCMASK(pack,			0x04),
	PMCMASK(unpack,			0x08),
	PMCMASK(packed-logical,		0x10),
	PMCMASK(packed-arithmetic,	0x20),
	NULLMASK
};

static struct pmc_masks p6_mask_fmt[] = {
	PMCMASK(mmxtofp,	0x00),
	PMCMASK(fptommx,	0x01),
	NULLMASK
};

static struct pmc_masks p6_mask_sr[] = {
	PMCMASK(es,	0x01),
	PMCMASK(ds,	0x02),
	PMCMASK(fs,	0x04),
	PMCMASK(gs,	0x08),
	NULLMASK
};

static struct pmc_masks p6_mask_eet[] = {
	PMCMASK(all,	0x00),
	PMCMASK(freq,	0x02),
	NULLMASK
};

static struct pmc_masks p6_mask_efur[] = {
	PMCMASK(all,	0x00),
	PMCMASK(loadop,	0x01),
	PMCMASK(stdsta,	0x02),
	NULLMASK
};

static struct pmc_masks p6_mask_essir[] = {
	PMCMASK(sse-packed-single,	0x00),
	PMCMASK(sse-packed-single-scalar-single, 0x01),
	PMCMASK(sse2-packed-double,	0x02),
	PMCMASK(sse2-scalar-double,	0x03),
	NULLMASK
};

static struct pmc_masks p6_mask_esscir[] = {
	PMCMASK(sse-packed-single,	0x00),
	PMCMASK(sse-scalar-single,	0x01),
	PMCMASK(sse2-packed-double,	0x02),
	PMCMASK(sse2-scalar-double,	0x03),
	NULLMASK
};

/* P6 event parser */
static int
p6_allocate_pmc(enum pmc_event pe, char *ctrspec,
    struct pmc_op_pmcallocate *pmc_config)
{
	char *e, *p, *q;
	uint32_t evmask;
	int count, n;
	const struct pmc_masks *pm, *pmask;

	pmc_config->pm_caps |= (PMC_CAP_READ | PMC_CAP_WRITE);
	pmc_config->pm_md.pm_ppro.pm_ppro_config = 0;

	evmask = 0;

#define	P6MASKSET(M)	pmask = p6_mask_ ## M

	switch(pe) {
	case PMC_EV_P6_L2_IFETCH:	P6MASKSET(mesi); break;
	case PMC_EV_P6_L2_LD:		P6MASKSET(mesi); break;
	case PMC_EV_P6_L2_ST:		P6MASKSET(mesi); break;
	case PMC_EV_P6_L2_RQSTS:	P6MASKSET(mesi); break;
	case PMC_EV_P6_BUS_DRDY_CLOCKS:
	case PMC_EV_P6_BUS_LOCK_CLOCKS:
	case PMC_EV_P6_BUS_TRAN_BRD:
	case PMC_EV_P6_BUS_TRAN_RFO:
	case PMC_EV_P6_BUS_TRANS_WB:
	case PMC_EV_P6_BUS_TRAN_IFETCH:
	case PMC_EV_P6_BUS_TRAN_INVAL:
	case PMC_EV_P6_BUS_TRAN_PWR:
	case PMC_EV_P6_BUS_TRANS_P:
	case PMC_EV_P6_BUS_TRANS_IO:
	case PMC_EV_P6_BUS_TRAN_DEF:
	case PMC_EV_P6_BUS_TRAN_BURST:
	case PMC_EV_P6_BUS_TRAN_ANY:
	case PMC_EV_P6_BUS_TRAN_MEM:
		P6MASKSET(any);	break;
	case PMC_EV_P6_EMON_KNI_PREF_DISPATCHED:
	case PMC_EV_P6_EMON_KNI_PREF_MISS:
		P6MASKSET(ekp); break;
	case PMC_EV_P6_EMON_KNI_INST_RETIRED:
	case PMC_EV_P6_EMON_KNI_COMP_INST_RET:
		P6MASKSET(pps);	break;
	case PMC_EV_P6_MMX_INSTR_TYPE_EXEC:
		P6MASKSET(mite); break;
	case PMC_EV_P6_FP_MMX_TRANS:
		P6MASKSET(fmt);	break;
	case PMC_EV_P6_SEG_RENAME_STALLS:
	case PMC_EV_P6_SEG_REG_RENAMES:
		P6MASKSET(sr);	break;
	case PMC_EV_P6_EMON_EST_TRANS:
		P6MASKSET(eet);	break;
	case PMC_EV_P6_EMON_FUSED_UOPS_RET:
		P6MASKSET(efur); break;
	case PMC_EV_P6_EMON_SSE_SSE2_INST_RETIRED:
		P6MASKSET(essir); break;
	case PMC_EV_P6_EMON_SSE_SSE2_COMP_INST_RETIRED:
		P6MASKSET(esscir); break;
	default:
		pmask = NULL;
		break;
	}

	/* Pentium M PMCs have a few events with different semantics */
	if (cpu_info.pm_cputype == PMC_CPU_INTEL_PM) {
		if (pe == PMC_EV_P6_L2_LD ||
		    pe == PMC_EV_P6_L2_LINES_IN ||
		    pe == PMC_EV_P6_L2_LINES_OUT)
			P6MASKSET(mesihw);
		else if (pe == PMC_EV_P6_L2_M_LINES_OUTM)
			P6MASKSET(hw);
	}

	/* Parse additional modifiers if present */
	while ((p = strsep(&ctrspec, ",")) != NULL) {
		if (KWPREFIXMATCH(p, P6_KW_CMASK "=")) {
			q = strchr(p, '=');
			if (*++q == '\0') /* skip '=' */
				return (-1);
			count = strtol(q, &e, 0);
			if (e == q || *e != '\0')
				return (-1);
			pmc_config->pm_caps |= PMC_CAP_THRESHOLD;
			pmc_config->pm_md.pm_ppro.pm_ppro_config |=
			    P6_EVSEL_TO_CMASK(count);
		} else if (KWMATCH(p, P6_KW_EDGE)) {
			pmc_config->pm_caps |= PMC_CAP_EDGE;
		} else if (KWMATCH(p, P6_KW_INV)) {
			pmc_config->pm_caps |= PMC_CAP_INVERT;
		} else if (KWMATCH(p, P6_KW_OS)) {
			pmc_config->pm_caps |= PMC_CAP_SYSTEM;
		} else if (KWPREFIXMATCH(p, P6_KW_UMASK "=")) {
			evmask = 0;
			if ((n = pmc_parse_mask(pmask, p, &evmask)) < 0)
				return (-1);
			if ((pe == PMC_EV_P6_BUS_DRDY_CLOCKS ||
			     pe == PMC_EV_P6_BUS_LOCK_CLOCKS ||
			     pe == PMC_EV_P6_BUS_TRAN_BRD ||
			     pe == PMC_EV_P6_BUS_TRAN_RFO ||
			     pe == PMC_EV_P6_BUS_TRAN_IFETCH ||
			     pe == PMC_EV_P6_BUS_TRAN_INVAL ||
			     pe == PMC_EV_P6_BUS_TRAN_PWR ||
			     pe == PMC_EV_P6_BUS_TRAN_DEF ||
			     pe == PMC_EV_P6_BUS_TRAN_BURST ||
			     pe == PMC_EV_P6_BUS_TRAN_ANY ||
			     pe == PMC_EV_P6_BUS_TRAN_MEM ||
			     pe == PMC_EV_P6_BUS_TRANS_IO ||
			     pe == PMC_EV_P6_BUS_TRANS_P ||
			     pe == PMC_EV_P6_BUS_TRANS_WB ||
			     pe == PMC_EV_P6_EMON_EST_TRANS ||
			     pe == PMC_EV_P6_EMON_FUSED_UOPS_RET ||
			     pe == PMC_EV_P6_EMON_KNI_COMP_INST_RET ||
			     pe == PMC_EV_P6_EMON_KNI_INST_RETIRED ||
			     pe == PMC_EV_P6_EMON_KNI_PREF_DISPATCHED ||
			     pe == PMC_EV_P6_EMON_KNI_PREF_MISS ||
			     pe == PMC_EV_P6_EMON_SSE_SSE2_COMP_INST_RETIRED ||
			     pe == PMC_EV_P6_EMON_SSE_SSE2_INST_RETIRED ||
			     pe == PMC_EV_P6_FP_MMX_TRANS)
			    && (n > 1))	/* Only one mask keyword is allowed. */
				return (-1);
			pmc_config->pm_caps |= PMC_CAP_QUALIFIER;
		} else if (KWMATCH(p, P6_KW_USR)) {
			pmc_config->pm_caps |= PMC_CAP_USER;
		} else
			return (-1);
	}

	/* post processing */
	switch (pe) {

		/*
		 * The following events default to an evmask of 0
		 */

		/* default => 'self' */
	case PMC_EV_P6_BUS_DRDY_CLOCKS:
	case PMC_EV_P6_BUS_LOCK_CLOCKS:
	case PMC_EV_P6_BUS_TRAN_BRD:
	case PMC_EV_P6_BUS_TRAN_RFO:
	case PMC_EV_P6_BUS_TRANS_WB:
	case PMC_EV_P6_BUS_TRAN_IFETCH:
	case PMC_EV_P6_BUS_TRAN_INVAL:
	case PMC_EV_P6_BUS_TRAN_PWR:
	case PMC_EV_P6_BUS_TRANS_P:
	case PMC_EV_P6_BUS_TRANS_IO:
	case PMC_EV_P6_BUS_TRAN_DEF:
	case PMC_EV_P6_BUS_TRAN_BURST:
	case PMC_EV_P6_BUS_TRAN_ANY:
	case PMC_EV_P6_BUS_TRAN_MEM:

		/* default => 'nta' */
	case PMC_EV_P6_EMON_KNI_PREF_DISPATCHED:
	case PMC_EV_P6_EMON_KNI_PREF_MISS:

		/* default => 'packed and scalar' */
	case PMC_EV_P6_EMON_KNI_INST_RETIRED:
	case PMC_EV_P6_EMON_KNI_COMP_INST_RET:

		/* default => 'mmx to fp transitions' */
	case PMC_EV_P6_FP_MMX_TRANS:

		/* default => 'SSE Packed Single' */
	case PMC_EV_P6_EMON_SSE_SSE2_INST_RETIRED:
	case PMC_EV_P6_EMON_SSE_SSE2_COMP_INST_RETIRED:

		/* default => 'all fused micro-ops' */
	case PMC_EV_P6_EMON_FUSED_UOPS_RET:

		/* default => 'all transitions' */
	case PMC_EV_P6_EMON_EST_TRANS:
		break;

	case PMC_EV_P6_MMX_UOPS_EXEC:
		evmask = 0x0F;		/* only value allowed */
		break;

	default:
		/*
		 * For all other events, set the default event mask
		 * to a logical OR of all the allowed event mask bits.
		 */
		if (evmask == 0 && pmask) {
			for (pm = pmask; pm->pm_name; pm++)
				evmask |= pm->pm_value;
			pmc_config->pm_caps |= PMC_CAP_QUALIFIER;
		}

		break;
	}

	if (pmc_config->pm_caps & PMC_CAP_QUALIFIER)
		pmc_config->pm_md.pm_ppro.pm_ppro_config |=
		    P6_EVSEL_TO_UMASK(evmask);

	return (0);
}

#endif

#if	defined(__i386__) || defined(__amd64__)
static int
tsc_allocate_pmc(enum pmc_event pe, char *ctrspec,
    struct pmc_op_pmcallocate *pmc_config)
{
	if (pe != PMC_EV_TSC_TSC)
		return (-1);

	/* TSC events must be unqualified. */
	if (ctrspec && *ctrspec != '\0')
		return (-1);

	pmc_config->pm_md.pm_amd.pm_amd_config = 0;
	pmc_config->pm_caps |= PMC_CAP_READ;

	return (0);
}
#endif

#if	defined(__XSCALE__)

static struct pmc_event_alias xscale_aliases[] = {
	EV_ALIAS("branches",		"BRANCH_RETIRED"),
	EV_ALIAS("branch-mispredicts",	"BRANCH_MISPRED"),
	EV_ALIAS("dc-misses",		"DC_MISS"),
	EV_ALIAS("ic-misses",		"IC_MISS"),
	EV_ALIAS("instructions",	"INSTR_RETIRED"),
	EV_ALIAS(NULL, NULL)
};
static int
xscale_allocate_pmc(enum pmc_event pe, char *ctrspec __unused,
    struct pmc_op_pmcallocate *pmc_config __unused)
{
	switch (pe) {
	default:
		break;
	}

	return (0);
}
#endif

#if defined(__mips__)

static struct pmc_event_alias mips24k_aliases[] = {
	EV_ALIAS("instructions",	"INSTR_EXECUTED"),
	EV_ALIAS("branches",		"BRANCH_COMPLETED"),
	EV_ALIAS("branch-mispredicts",	"BRANCH_MISPRED"),
	EV_ALIAS(NULL, NULL)
};

#define	MIPS24K_KW_OS		"os"
#define	MIPS24K_KW_USR		"usr"
#define	MIPS24K_KW_ANYTHREAD	"anythread"

static int
mips24k_allocate_pmc(enum pmc_event pe, char *ctrspec __unused,
		  struct pmc_op_pmcallocate *pmc_config __unused)
{
	char *p;

	(void) pe;

	pmc_config->pm_caps |= (PMC_CAP_READ | PMC_CAP_WRITE);
	
	while ((p = strsep(&ctrspec, ",")) != NULL) {
		if (KWMATCH(p, MIPS24K_KW_OS))
			pmc_config->pm_caps |= PMC_CAP_SYSTEM;
		else if (KWMATCH(p, MIPS24K_KW_USR))
			pmc_config->pm_caps |= PMC_CAP_USER;
		else if (KWMATCH(p, MIPS24K_KW_ANYTHREAD))
			pmc_config->pm_caps |= (PMC_CAP_USER | PMC_CAP_SYSTEM);
		else
			return (-1);
	}

	return (0);
}
#endif /* __mips__ */


/*
 * Match an event name `name' with its canonical form.
 *
 * Matches are case insensitive and spaces, periods, underscores and
 * hyphen characters are considered to match each other.
 *
 * Returns 1 for a match, 0 otherwise.
 */

static int
pmc_match_event_name(const char *name, const char *canonicalname)
{
	int cc, nc;
	const unsigned char *c, *n;

	c = (const unsigned char *) canonicalname;
	n = (const unsigned char *) name;

	for (; (nc = *n) && (cc = *c); n++, c++) {

		if ((nc == ' ' || nc == '_' || nc == '-' || nc == '.') &&
		    (cc == ' ' || cc == '_' || cc == '-' || cc == '.'))
			continue;

		if (toupper(nc) == toupper(cc))
			continue;


		return (0);
	}

	if (*n == '\0' && *c == '\0')
		return (1);

	return (0);
}

/*
 * Match an event name against all the event named supported by a
 * PMC class.
 *
 * Returns an event descriptor pointer on match or NULL otherwise.
 */
static const struct pmc_event_descr *
pmc_match_event_class(const char *name,
    const struct pmc_class_descr *pcd)
{
	size_t n;
	const struct pmc_event_descr *ev;

	ev = pcd->pm_evc_event_table;
	for (n = 0; n < pcd->pm_evc_event_table_size; n++, ev++)
		if (pmc_match_event_name(name, ev->pm_ev_name))
			return (ev);

	return (NULL);
}

static int
pmc_mdep_is_compatible_class(enum pmc_class pc)
{
	size_t n;

	for (n = 0; n < pmc_mdep_class_list_size; n++)
		if (pmc_mdep_class_list[n] == pc)
			return (1);
	return (0);
}

/*
 * API entry points
 */

int
pmc_allocate(const char *ctrspec, enum pmc_mode mode,
    uint32_t flags, int cpu, pmc_id_t *pmcid)
{
	size_t n;
	int retval;
	char *r, *spec_copy;
	const char *ctrname;
	const struct pmc_event_descr *ev;
	const struct pmc_event_alias *alias;
	struct pmc_op_pmcallocate pmc_config;
	const struct pmc_class_descr *pcd;

	spec_copy = NULL;
	retval    = -1;

	if (mode != PMC_MODE_SS && mode != PMC_MODE_TS &&
	    mode != PMC_MODE_SC && mode != PMC_MODE_TC) {
		errno = EINVAL;
		goto out;
	}

	/* replace an event alias with the canonical event specifier */
	if (pmc_mdep_event_aliases)
		for (alias = pmc_mdep_event_aliases; alias->pm_alias; alias++)
			if (!strcasecmp(ctrspec, alias->pm_alias)) {
				spec_copy = strdup(alias->pm_spec);
				break;
			}

	if (spec_copy == NULL)
		spec_copy = strdup(ctrspec);

	r = spec_copy;
	ctrname = strsep(&r, ",");

	/*
	 * If a explicit class prefix was given by the user, restrict the
	 * search for the event to the specified PMC class.
	 */
	ev = NULL;
	for (n = 0; n < PMC_CLASS_TABLE_SIZE; n++) {
		pcd = pmc_class_table[n];
		if (pmc_mdep_is_compatible_class(pcd->pm_evc_class) &&
		    strncasecmp(ctrname, pcd->pm_evc_name,
				pcd->pm_evc_name_size) == 0) {
			if ((ev = pmc_match_event_class(ctrname +
			    pcd->pm_evc_name_size, pcd)) == NULL) {
				errno = EINVAL;
				goto out;
			}
			break;
		}
	}

	/*
	 * Otherwise, search for this event in all compatible PMC
	 * classes.
	 */
	for (n = 0; ev == NULL && n < PMC_CLASS_TABLE_SIZE; n++) {
		pcd = pmc_class_table[n];
		if (pmc_mdep_is_compatible_class(pcd->pm_evc_class))
			ev = pmc_match_event_class(ctrname, pcd);
	}

	if (ev == NULL) {
		errno = EINVAL;
		goto out;
	}

	bzero(&pmc_config, sizeof(pmc_config));
	pmc_config.pm_ev    = ev->pm_ev_code;
	pmc_config.pm_class = pcd->pm_evc_class;
	pmc_config.pm_cpu   = cpu;
	pmc_config.pm_mode  = mode;
	pmc_config.pm_flags = flags;

	if (PMC_IS_SAMPLING_MODE(mode))
		pmc_config.pm_caps |= PMC_CAP_INTERRUPT;

 	if (pcd->pm_evc_allocate_pmc(ev->pm_ev_code, r, &pmc_config) < 0) {
		errno = EINVAL;
		goto out;
	}

	if (PMC_CALL(PMCALLOCATE, &pmc_config) < 0)
		goto out;

	*pmcid = pmc_config.pm_pmcid;

	retval = 0;

 out:
	if (spec_copy)
		free(spec_copy);

	return (retval);
}

int
pmc_attach(pmc_id_t pmc, pid_t pid)
{
	struct pmc_op_pmcattach pmc_attach_args;

	pmc_attach_args.pm_pmc = pmc;
	pmc_attach_args.pm_pid = pid;

	return (PMC_CALL(PMCATTACH, &pmc_attach_args));
}

int
pmc_capabilities(pmc_id_t pmcid, uint32_t *caps)
{
	unsigned int i;
	enum pmc_class cl;

	cl = PMC_ID_TO_CLASS(pmcid);
	for (i = 0; i < cpu_info.pm_nclass; i++)
		if (cpu_info.pm_classes[i].pm_class == cl) {
			*caps = cpu_info.pm_classes[i].pm_caps;
			return (0);
		}
	errno = EINVAL;
	return (-1);
}

int
pmc_configure_logfile(int fd)
{
	struct pmc_op_configurelog cla;

	cla.pm_logfd = fd;
	if (PMC_CALL(CONFIGURELOG, &cla) < 0)
		return (-1);
	return (0);
}

int
pmc_cpuinfo(const struct pmc_cpuinfo **pci)
{
	if (pmc_syscall == -1) {
		errno = ENXIO;
		return (-1);
	}

	*pci = &cpu_info;
	return (0);
}

int
pmc_detach(pmc_id_t pmc, pid_t pid)
{
	struct pmc_op_pmcattach pmc_detach_args;

	pmc_detach_args.pm_pmc = pmc;
	pmc_detach_args.pm_pid = pid;
	return (PMC_CALL(PMCDETACH, &pmc_detach_args));
}

int
pmc_disable(int cpu, int pmc)
{
	struct pmc_op_pmcadmin ssa;

	ssa.pm_cpu = cpu;
	ssa.pm_pmc = pmc;
	ssa.pm_state = PMC_STATE_DISABLED;
	return (PMC_CALL(PMCADMIN, &ssa));
}

int
pmc_enable(int cpu, int pmc)
{
	struct pmc_op_pmcadmin ssa;

	ssa.pm_cpu = cpu;
	ssa.pm_pmc = pmc;
	ssa.pm_state = PMC_STATE_FREE;
	return (PMC_CALL(PMCADMIN, &ssa));
}

/*
 * Return a list of events known to a given PMC class.  'cl' is the
 * PMC class identifier, 'eventnames' is the returned list of 'const
 * char *' pointers pointing to the names of the events. 'nevents' is
 * the number of event name pointers returned.
 *
 * The space for 'eventnames' is allocated using malloc(3).  The caller
 * is responsible for freeing this space when done.
 */
int
pmc_event_names_of_class(enum pmc_class cl, const char ***eventnames,
    int *nevents)
{
	int count;
	const char **names;
	const struct pmc_event_descr *ev;

	switch (cl)
	{
	case PMC_CLASS_IAF:
		ev = iaf_event_table;
		count = PMC_EVENT_TABLE_SIZE(iaf);
		break;
	case PMC_CLASS_IAP:
		/*
		 * Return the most appropriate set of event name
		 * spellings for the current CPU.
		 */
		switch (cpu_info.pm_cputype) {
		default:
		case PMC_CPU_INTEL_ATOM:
			ev = atom_event_table;
			count = PMC_EVENT_TABLE_SIZE(atom);
			break;
		case PMC_CPU_INTEL_CORE:
			ev = core_event_table;
			count = PMC_EVENT_TABLE_SIZE(core);
			break;
		case PMC_CPU_INTEL_CORE2:
		case PMC_CPU_INTEL_CORE2EXTREME:
			ev = core2_event_table;
			count = PMC_EVENT_TABLE_SIZE(core2);
			break;
		case PMC_CPU_INTEL_COREI7:
			ev = corei7_event_table;
			count = PMC_EVENT_TABLE_SIZE(corei7);
			break;
		case PMC_CPU_INTEL_WESTMERE:
			ev = westmere_event_table;
			count = PMC_EVENT_TABLE_SIZE(westmere);
			break;
		}
		break;
	case PMC_CLASS_UCF:
		ev = ucf_event_table;
		count = PMC_EVENT_TABLE_SIZE(ucf);
		break;
	case PMC_CLASS_UCP:
		/*
		 * Return the most appropriate set of event name
		 * spellings for the current CPU.
		 */
		switch (cpu_info.pm_cputype) {
		default:
		case PMC_CPU_INTEL_COREI7:
			ev = corei7uc_event_table;
			count = PMC_EVENT_TABLE_SIZE(corei7uc);
			break;
		case PMC_CPU_INTEL_WESTMERE:
			ev = westmereuc_event_table;
			count = PMC_EVENT_TABLE_SIZE(westmereuc);
			break;
		}
		break;
	case PMC_CLASS_TSC:
		ev = tsc_event_table;
		count = PMC_EVENT_TABLE_SIZE(tsc);
		break;
	case PMC_CLASS_K7:
		ev = k7_event_table;
		count = PMC_EVENT_TABLE_SIZE(k7);
		break;
	case PMC_CLASS_K8:
		ev = k8_event_table;
		count = PMC_EVENT_TABLE_SIZE(k8);
		break;
	case PMC_CLASS_P4:
		ev = p4_event_table;
		count = PMC_EVENT_TABLE_SIZE(p4);
		break;
	case PMC_CLASS_P5:
		ev = p5_event_table;
		count = PMC_EVENT_TABLE_SIZE(p5);
		break;
	case PMC_CLASS_P6:
		ev = p6_event_table;
		count = PMC_EVENT_TABLE_SIZE(p6);
		break;
	case PMC_CLASS_XSCALE:
		ev = xscale_event_table;
		count = PMC_EVENT_TABLE_SIZE(xscale);
		break;
	case PMC_CLASS_MIPS24K:
		ev = mips24k_event_table;
		count = PMC_EVENT_TABLE_SIZE(mips24k);
		break;
	default:
		errno = EINVAL;
		return (-1);
	}

	if ((names = malloc(count * sizeof(const char *))) == NULL)
		return (-1);

	*eventnames = names;
	*nevents = count;

	for (;count--; ev++, names++)
		*names = ev->pm_ev_name;
	return (0);
}

int
pmc_flush_logfile(void)
{
	return (PMC_CALL(FLUSHLOG,0));
}

int
pmc_close_logfile(void)
{
	return (PMC_CALL(CLOSELOG,0));
}

int
pmc_get_driver_stats(struct pmc_driverstats *ds)
{
	struct pmc_op_getdriverstats gms;

	if (PMC_CALL(GETDRIVERSTATS, &gms) < 0)
		return (-1);

	/* copy out fields in the current userland<->library interface */
	ds->pm_intr_ignored    = gms.pm_intr_ignored;
	ds->pm_intr_processed  = gms.pm_intr_processed;
	ds->pm_intr_bufferfull = gms.pm_intr_bufferfull;
	ds->pm_syscalls        = gms.pm_syscalls;
	ds->pm_syscall_errors  = gms.pm_syscall_errors;
	ds->pm_buffer_requests = gms.pm_buffer_requests;
	ds->pm_buffer_requests_failed = gms.pm_buffer_requests_failed;
	ds->pm_log_sweeps      = gms.pm_log_sweeps;
	return (0);
}

int
pmc_get_msr(pmc_id_t pmc, uint32_t *msr)
{
	struct pmc_op_getmsr gm;

	gm.pm_pmcid = pmc;
	if (PMC_CALL(PMCGETMSR, &gm) < 0)
		return (-1);
	*msr = gm.pm_msr;
	return (0);
}

int
pmc_init(void)
{
	int error, pmc_mod_id;
	unsigned int n;
	uint32_t abi_version;
	struct module_stat pmc_modstat;
	struct pmc_op_getcpuinfo op_cpu_info;
#if defined(__amd64__) || defined(__i386__)
	int cpu_has_iaf_counters;
	unsigned int t;
#endif

	if (pmc_syscall != -1) /* already inited */
		return (0);

	/* retrieve the system call number from the KLD */
	if ((pmc_mod_id = modfind(PMC_MODULE_NAME)) < 0)
		return (-1);

	pmc_modstat.version = sizeof(struct module_stat);
	if ((error = modstat(pmc_mod_id, &pmc_modstat)) < 0)
		return (-1);

	pmc_syscall = pmc_modstat.data.intval;

	/* check the kernel module's ABI against our compiled-in version */
	abi_version = PMC_VERSION;
	if (PMC_CALL(GETMODULEVERSION, &abi_version) < 0)
		return (pmc_syscall = -1);

	/* ignore patch & minor numbers for the comparision */
	if ((abi_version & 0xFF000000) != (PMC_VERSION & 0xFF000000)) {
		errno  = EPROGMISMATCH;
		return (pmc_syscall = -1);
	}

	if (PMC_CALL(GETCPUINFO, &op_cpu_info) < 0)
		return (pmc_syscall = -1);

	cpu_info.pm_cputype = op_cpu_info.pm_cputype;
	cpu_info.pm_ncpu    = op_cpu_info.pm_ncpu;
	cpu_info.pm_npmc    = op_cpu_info.pm_npmc;
	cpu_info.pm_nclass  = op_cpu_info.pm_nclass;
	for (n = 0; n < cpu_info.pm_nclass; n++)
		cpu_info.pm_classes[n] = op_cpu_info.pm_classes[n];

	pmc_class_table = malloc(PMC_CLASS_TABLE_SIZE *
	    sizeof(struct pmc_class_descr *));

	if (pmc_class_table == NULL)
		return (-1);

	for (n = 0; n < PMC_CLASS_TABLE_SIZE; n++)
		pmc_class_table[n] = NULL;

	/*
	 * Fill in the class table.
	 */
	n = 0;
#if defined(__amd64__) || defined(__i386__)
	pmc_class_table[n++] = &tsc_class_table_descr;

	/*
 	 * Check if this CPU has fixed function counters.
	 */
	cpu_has_iaf_counters = 0;
	for (t = 0; t < cpu_info.pm_nclass; t++)
		if (cpu_info.pm_classes[t].pm_class == PMC_CLASS_IAF &&
		    cpu_info.pm_classes[t].pm_num > 0)
			cpu_has_iaf_counters = 1;
#endif

#define	PMC_MDEP_INIT(C) do {					\
		pmc_mdep_event_aliases    = C##_aliases;	\
		pmc_mdep_class_list  = C##_pmc_classes;		\
		pmc_mdep_class_list_size =			\
		    PMC_TABLE_SIZE(C##_pmc_classes);		\
	} while (0)

#define	PMC_MDEP_INIT_INTEL_V2(C) do {					\
		PMC_MDEP_INIT(C);					\
		pmc_class_table[n++] = &iaf_class_table_descr;		\
		if (!cpu_has_iaf_counters) 				\
			pmc_mdep_event_aliases =			\
				C##_aliases_without_iaf;		\
		pmc_class_table[n] = &C##_class_table_descr;		\
	} while (0)

	/* Configure the event name parser. */
	switch (cpu_info.pm_cputype) {
#if defined(__i386__)
	case PMC_CPU_AMD_K7:
		PMC_MDEP_INIT(k7);
		pmc_class_table[n] = &k7_class_table_descr;
		break;
	case PMC_CPU_INTEL_P5:
		PMC_MDEP_INIT(p5);
		pmc_class_table[n]  = &p5_class_table_descr;
		break;
	case PMC_CPU_INTEL_P6:		/* P6 ... Pentium M CPUs have */
	case PMC_CPU_INTEL_PII:		/* similar PMCs. */
	case PMC_CPU_INTEL_PIII:
	case PMC_CPU_INTEL_PM:
		PMC_MDEP_INIT(p6);
		pmc_class_table[n] = &p6_class_table_descr;
		break;
#endif
#if defined(__amd64__) || defined(__i386__)
	case PMC_CPU_AMD_K8:
		PMC_MDEP_INIT(k8);
		pmc_class_table[n] = &k8_class_table_descr;
		break;
	case PMC_CPU_INTEL_ATOM:
		PMC_MDEP_INIT_INTEL_V2(atom);
		break;
	case PMC_CPU_INTEL_CORE:
		PMC_MDEP_INIT(core);
		pmc_class_table[n] = &core_class_table_descr;
		break;
	case PMC_CPU_INTEL_CORE2:
	case PMC_CPU_INTEL_CORE2EXTREME:
		PMC_MDEP_INIT_INTEL_V2(core2);
		break;
	case PMC_CPU_INTEL_COREI7:
		pmc_class_table[n++] = &ucf_class_table_descr;
		pmc_class_table[n++] = &corei7uc_class_table_descr;
		PMC_MDEP_INIT_INTEL_V2(corei7);
		break;
	case PMC_CPU_INTEL_WESTMERE:
		pmc_class_table[n++] = &ucf_class_table_descr;
		pmc_class_table[n++] = &westmereuc_class_table_descr;
		PMC_MDEP_INIT_INTEL_V2(westmere);
		break;
	case PMC_CPU_INTEL_PIV:
		PMC_MDEP_INIT(p4);
		pmc_class_table[n] = &p4_class_table_descr;
		break;
#endif
#if defined(__XSCALE__)
	case PMC_CPU_INTEL_XSCALE:
		PMC_MDEP_INIT(xscale);
		pmc_class_table[n] = &xscale_class_table_descr;
		break;
#endif
#if defined(__mips__)
	case PMC_CPU_MIPS_24K:
		PMC_MDEP_INIT(mips24k);
		pmc_class_table[n] = &mips24k_class_table_descr;
		break;
#endif /* __mips__ */
	default:
		/*
		 * Some kind of CPU this version of the library knows nothing
		 * about.  This shouldn't happen since the abi version check
		 * should have caught this.
		 */
		errno = ENXIO;
		return (pmc_syscall = -1);
	}

	return (0);
}

const char *
pmc_name_of_capability(enum pmc_caps cap)
{
	int i;

	/*
	 * 'cap' should have a single bit set and should be in
	 * range.
	 */
	if ((cap & (cap - 1)) || cap < PMC_CAP_FIRST ||
	    cap > PMC_CAP_LAST) {
		errno = EINVAL;
		return (NULL);
	}

	i = ffs(cap);
	return (pmc_capability_names[i - 1]);
}

const char *
pmc_name_of_class(enum pmc_class pc)
{
	if ((int) pc >= PMC_CLASS_FIRST &&
	    pc <= PMC_CLASS_LAST)
		return (pmc_class_names[pc]);

	errno = EINVAL;
	return (NULL);
}

const char *
pmc_name_of_cputype(enum pmc_cputype cp)
{
	size_t n;

	for (n = 0; n < PMC_TABLE_SIZE(pmc_cputype_names); n++)
		if (cp == pmc_cputype_names[n].pm_cputype)
			return (pmc_cputype_names[n].pm_name);

	errno = EINVAL;
	return (NULL);
}

const char *
pmc_name_of_disposition(enum pmc_disp pd)
{
	if ((int) pd >= PMC_DISP_FIRST &&
	    pd <= PMC_DISP_LAST)
		return (pmc_disposition_names[pd]);

	errno = EINVAL;
	return (NULL);
}

const char *
_pmc_name_of_event(enum pmc_event pe, enum pmc_cputype cpu)
{
	const struct pmc_event_descr *ev, *evfence;

	ev = evfence = NULL;
	if (pe >= PMC_EV_IAF_FIRST && pe <= PMC_EV_IAF_LAST) {
		ev = iaf_event_table;
		evfence = iaf_event_table + PMC_EVENT_TABLE_SIZE(iaf);
	} else if (pe >= PMC_EV_IAP_FIRST && pe <= PMC_EV_IAP_LAST) {
		switch (cpu) {
		case PMC_CPU_INTEL_ATOM:
			ev = atom_event_table;
			evfence = atom_event_table + PMC_EVENT_TABLE_SIZE(atom);
			break;
		case PMC_CPU_INTEL_CORE:
			ev = core_event_table;
			evfence = core_event_table + PMC_EVENT_TABLE_SIZE(core);
			break;
		case PMC_CPU_INTEL_CORE2:
		case PMC_CPU_INTEL_CORE2EXTREME:
			ev = core2_event_table;
			evfence = core2_event_table + PMC_EVENT_TABLE_SIZE(core2);
			break;
		case PMC_CPU_INTEL_COREI7:
			ev = corei7_event_table;
			evfence = corei7_event_table + PMC_EVENT_TABLE_SIZE(corei7);
			break;
		case PMC_CPU_INTEL_WESTMERE:
			ev = westmere_event_table;
			evfence = westmere_event_table + PMC_EVENT_TABLE_SIZE(westmere);
			break;
		default:	/* Unknown CPU type. */
			break;
		}
	} else if (pe >= PMC_EV_UCF_FIRST && pe <= PMC_EV_UCF_LAST) {
		ev = ucf_event_table;
		evfence = ucf_event_table + PMC_EVENT_TABLE_SIZE(ucf);
	} else if (pe >= PMC_EV_UCP_FIRST && pe <= PMC_EV_UCP_LAST) {
		switch (cpu) {
		case PMC_CPU_INTEL_COREI7:
			ev = corei7uc_event_table;
			evfence = corei7uc_event_table + PMC_EVENT_TABLE_SIZE(corei7uc);
			break;
		case PMC_CPU_INTEL_WESTMERE:
			ev = westmereuc_event_table;
			evfence = westmereuc_event_table + PMC_EVENT_TABLE_SIZE(westmereuc);
			break;
		default:	/* Unknown CPU type. */
			break;
		}
	} else if (pe >= PMC_EV_K7_FIRST && pe <= PMC_EV_K7_LAST) {
		ev = k7_event_table;
		evfence = k7_event_table + PMC_EVENT_TABLE_SIZE(k7);
	} else if (pe >= PMC_EV_K8_FIRST && pe <= PMC_EV_K8_LAST) {
		ev = k8_event_table;
		evfence = k8_event_table + PMC_EVENT_TABLE_SIZE(k8);
	} else if (pe >= PMC_EV_P4_FIRST && pe <= PMC_EV_P4_LAST) {
		ev = p4_event_table;
		evfence = p4_event_table + PMC_EVENT_TABLE_SIZE(p4);
	} else if (pe >= PMC_EV_P5_FIRST && pe <= PMC_EV_P5_LAST) {
		ev = p5_event_table;
		evfence = p5_event_table + PMC_EVENT_TABLE_SIZE(p5);
	} else if (pe >= PMC_EV_P6_FIRST && pe <= PMC_EV_P6_LAST) {
		ev = p6_event_table;
		evfence = p6_event_table + PMC_EVENT_TABLE_SIZE(p6);
	} else if (pe >= PMC_EV_XSCALE_FIRST && pe <= PMC_EV_XSCALE_LAST) {
		ev = xscale_event_table;
		evfence = xscale_event_table + PMC_EVENT_TABLE_SIZE(xscale);
	} else if (pe >= PMC_EV_MIPS24K_FIRST && pe <= PMC_EV_MIPS24K_LAST) {
		ev = mips24k_event_table;
		evfence = mips24k_event_table + PMC_EVENT_TABLE_SIZE(mips24k
);
	} else if (pe == PMC_EV_TSC_TSC) {
		ev = tsc_event_table;
		evfence = tsc_event_table + PMC_EVENT_TABLE_SIZE(tsc);
	}

	for (; ev != evfence; ev++)
		if (pe == ev->pm_ev_code)
			return (ev->pm_ev_name);

	return (NULL);
}

const char *
pmc_name_of_event(enum pmc_event pe)
{
	const char *n;

	if ((n = _pmc_name_of_event(pe, cpu_info.pm_cputype)) != NULL)
		return (n);

	errno = EINVAL;
	return (NULL);
}

const char *
pmc_name_of_mode(enum pmc_mode pm)
{
	if ((int) pm >= PMC_MODE_FIRST &&
	    pm <= PMC_MODE_LAST)
		return (pmc_mode_names[pm]);

	errno = EINVAL;
	return (NULL);
}

const char *
pmc_name_of_state(enum pmc_state ps)
{
	if ((int) ps >= PMC_STATE_FIRST &&
	    ps <= PMC_STATE_LAST)
		return (pmc_state_names[ps]);

	errno = EINVAL;
	return (NULL);
}

int
pmc_ncpu(void)
{
	if (pmc_syscall == -1) {
		errno = ENXIO;
		return (-1);
	}

	return (cpu_info.pm_ncpu);
}

int
pmc_npmc(int cpu)
{
	if (pmc_syscall == -1) {
		errno = ENXIO;
		return (-1);
	}

	if (cpu < 0 || cpu >= (int) cpu_info.pm_ncpu) {
		errno = EINVAL;
		return (-1);
	}

	return (cpu_info.pm_npmc);
}

int
pmc_pmcinfo(int cpu, struct pmc_pmcinfo **ppmci)
{
	int nbytes, npmc;
	struct pmc_op_getpmcinfo *pmci;

	if ((npmc = pmc_npmc(cpu)) < 0)
		return (-1);

	nbytes = sizeof(struct pmc_op_getpmcinfo) +
	    npmc * sizeof(struct pmc_info);

	if ((pmci = calloc(1, nbytes)) == NULL)
		return (-1);

	pmci->pm_cpu  = cpu;

	if (PMC_CALL(GETPMCINFO, pmci) < 0) {
		free(pmci);
		return (-1);
	}

	/* kernel<->library, library<->userland interfaces are identical */
	*ppmci = (struct pmc_pmcinfo *) pmci;
	return (0);
}

int
pmc_read(pmc_id_t pmc, pmc_value_t *value)
{
	struct pmc_op_pmcrw pmc_read_op;

	pmc_read_op.pm_pmcid = pmc;
	pmc_read_op.pm_flags = PMC_F_OLDVALUE;
	pmc_read_op.pm_value = -1;

	if (PMC_CALL(PMCRW, &pmc_read_op) < 0)
		return (-1);

	*value = pmc_read_op.pm_value;
	return (0);
}

int
pmc_release(pmc_id_t pmc)
{
	struct pmc_op_simple	pmc_release_args;

	pmc_release_args.pm_pmcid = pmc;
	return (PMC_CALL(PMCRELEASE, &pmc_release_args));
}

int
pmc_rw(pmc_id_t pmc, pmc_value_t newvalue, pmc_value_t *oldvaluep)
{
	struct pmc_op_pmcrw pmc_rw_op;

	pmc_rw_op.pm_pmcid = pmc;
	pmc_rw_op.pm_flags = PMC_F_NEWVALUE | PMC_F_OLDVALUE;
	pmc_rw_op.pm_value = newvalue;

	if (PMC_CALL(PMCRW, &pmc_rw_op) < 0)
		return (-1);

	*oldvaluep = pmc_rw_op.pm_value;
	return (0);
}

int
pmc_set(pmc_id_t pmc, pmc_value_t value)
{
	struct pmc_op_pmcsetcount sc;

	sc.pm_pmcid = pmc;
	sc.pm_count = value;

	if (PMC_CALL(PMCSETCOUNT, &sc) < 0)
		return (-1);
	return (0);
}

int
pmc_start(pmc_id_t pmc)
{
	struct pmc_op_simple	pmc_start_args;

	pmc_start_args.pm_pmcid = pmc;
	return (PMC_CALL(PMCSTART, &pmc_start_args));
}

int
pmc_stop(pmc_id_t pmc)
{
	struct pmc_op_simple	pmc_stop_args;

	pmc_stop_args.pm_pmcid = pmc;
	return (PMC_CALL(PMCSTOP, &pmc_stop_args));
}

int
pmc_width(pmc_id_t pmcid, uint32_t *width)
{
	unsigned int i;
	enum pmc_class cl;

	cl = PMC_ID_TO_CLASS(pmcid);
	for (i = 0; i < cpu_info.pm_nclass; i++)
		if (cpu_info.pm_classes[i].pm_class == cl) {
			*width = cpu_info.pm_classes[i].pm_width;
			return (0);
		}
	errno = EINVAL;
	return (-1);
}

int
pmc_write(pmc_id_t pmc, pmc_value_t value)
{
	struct pmc_op_pmcrw pmc_write_op;

	pmc_write_op.pm_pmcid = pmc;
	pmc_write_op.pm_flags = PMC_F_NEWVALUE;
	pmc_write_op.pm_value = value;
	return (PMC_CALL(PMCRW, &pmc_write_op));
}

int
pmc_writelog(uint32_t userdata)
{
	struct pmc_op_writelog wl;

	wl.pm_userdata = userdata;
	return (PMC_CALL(WRITELOG, &wl));
}
