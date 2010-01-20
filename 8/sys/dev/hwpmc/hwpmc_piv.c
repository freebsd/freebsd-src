/*-
 * Copyright (c) 2003-2007 Joseph Koshy
 * Copyright (c) 2007 The FreeBSD Foundation
 * All rights reserved.
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

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/bus.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/pmc.h>
#include <sys/pmckern.h>
#include <sys/smp.h>
#include <sys/systm.h>

#include <machine/intr_machdep.h>
#include <machine/apicvar.h>
#include <machine/cpu.h>
#include <machine/cpufunc.h>
#include <machine/cputypes.h>
#include <machine/md_var.h>
#include <machine/specialreg.h>

/*
 * PENTIUM 4 SUPPORT
 *
 * The P4 has 18 PMCs, divided into 4 groups with 4,4,4 and 6 PMCs
 * respectively.  Each PMC comprises of two model specific registers:
 * a counter configuration control register (CCCR) and a counter
 * register that holds the actual event counts.
 *
 * Configuring an event requires the use of one of 45 event selection
 * control registers (ESCR).  Events are associated with specific
 * ESCRs.  Each PMC group has a set of ESCRs it can use.
 *
 * - The BPU counter group (4 PMCs) can use the 16 ESCRs:
 *   BPU_ESCR{0,1}, IS_ESCR{0,1}, MOB_ESCR{0,1}, ITLB_ESCR{0,1},
 *   PMH_ESCR{0,1}, IX_ESCR{0,1}, FSB_ESCR{0,}, BSU_ESCR{0,1}.
 *
 * - The MS counter group (4 PMCs) can use the 6 ESCRs: MS_ESCR{0,1},
 *   TC_ESCR{0,1}, TBPU_ESCR{0,1}.
 *
 * - The FLAME counter group (4 PMCs) can use the 10 ESCRs:
 *   FLAME_ESCR{0,1}, FIRM_ESCR{0,1}, SAAT_ESCR{0,1}, U2L_ESCR{0,1},
 *   DAC_ESCR{0,1}.
 *
 * - The IQ counter group (6 PMCs) can use the 13 ESCRs: IQ_ESCR{0,1},
 *   ALF_ESCR{0,1}, RAT_ESCR{0,1}, SSU_ESCR0, CRU_ESCR{0,1,2,3,4,5}.
 *
 * Even-numbered ESCRs can be used with counters 0, 1 and 4 (if
 * present) of a counter group.  Odd-numbers ESCRs can be used with
 * counters 2, 3 and 5 (if present) of a counter group.  The
 * 'p4_escrs[]' table describes these restrictions in a form that
 * function 'p4_allocate()' uses for making allocation decisions.
 *
 * SYSTEM-MODE AND THREAD-MODE ALLOCATION
 *
 * In addition to remembering the state of PMC rows
 * ('FREE','STANDALONE', or 'THREAD'), we similar need to track the
 * state of ESCR rows.  If an ESCR is allocated to a system-mode PMC
 * on a CPU we cannot allocate this to a thread-mode PMC.  On a
 * multi-cpu (multiple physical CPUs) system, ESCR allocation on each
 * CPU is tracked by the pc_escrs[] array.
 *
 * Each system-mode PMC that is using an ESCR records its row-index in
 * the appropriate entry and system-mode allocation attempts check
 * that an ESCR is available using this array.  Process-mode PMCs do
 * not use the pc_escrs[] array, since ESCR row itself would have been
 * marked as in 'THREAD' mode.
 *
 * HYPERTHREADING SUPPORT
 *
 * When HTT is enabled, the FreeBSD kernel treats the two 'logical'
 * cpus as independent CPUs and can schedule kernel threads on them
 * independently.  However, the two logical CPUs share the same set of
 * PMC resources.  We need to ensure that:
 * - PMCs that use the PMC_F_DESCENDANTS semantics are handled correctly,
 *   and,
 * - Threads of multi-threaded processes that get scheduled on the same
 *   physical CPU are handled correctly.
 *
 * HTT Detection
 *
 * Not all HTT capable systems will have HTT enabled.  We detect the
 * presence of HTT by detecting if 'p4_init()' was called for a secondary
 * CPU in a HTT pair.
 *
 * Note that hwpmc(4) cannot currently deal with a change in HTT status once
 * loaded.
 *
 * Handling HTT READ / WRITE / START / STOP
 *
 * PMC resources are shared across the CPUs in an HTT pair.  We
 * designate the lower numbered CPU in a HTT pair as the 'primary'
 * CPU.  In each primary CPU's state we keep track of a 'runcount'
 * which reflects the number of PMC-using processes that have been
 * scheduled on its secondary CPU.  Process-mode PMC operations will
 * actually 'start' or 'stop' hardware only if these are the first or
 * last processes respectively to use the hardware.  PMC values
 * written by a 'write' operation are saved and are transferred to
 * hardware at PMC 'start' time if the runcount is 0.  If the runcount
 * is greater than 0 at the time of a 'start' operation, we keep track
 * of the actual hardware value at the time of the 'start' operation
 * and use this to adjust the final readings at PMC 'stop' or 'read'
 * time.
 *
 * Execution sequences:
 *
 * Case 1:   CPUx   +...-		(no overlap)
 *	     CPUy         +...-
 *           RC   0 1   0 1   0
 *
 * Case 2:   CPUx   +........-		(partial overlap)
 * 	     CPUy       +........-
 *           RC   0 1   2    1   0
 *
 * Case 3:   CPUx   +..............-	(fully overlapped)
 *	     CPUy       +.....-
 *	     RC   0 1   2     1    0
 *
 *     Key:
 *     'CPU[xy]' : one of the two logical processors on a HTT CPU.
 *     'RC'      : run count (#threads per physical core).
 *     '+'       : point in time when a thread is put on a CPU.
 *     '-'       : point in time where a thread is taken off a CPU.
 *
 * Handling HTT CONFIG
 *
 * Different processes attached to the same PMC may get scheduled on
 * the two logical processors in the package.  We keep track of config
 * and de-config operations using the CFGFLAGS fields of the per-physical
 * cpu state.
 */

#define	P4_PMCS()				\
	P4_PMC(BPU_COUNTER0)			\
	P4_PMC(BPU_COUNTER1)			\
	P4_PMC(BPU_COUNTER2)			\
	P4_PMC(BPU_COUNTER3)			\
	P4_PMC(MS_COUNTER0)			\
	P4_PMC(MS_COUNTER1)			\
	P4_PMC(MS_COUNTER2)			\
	P4_PMC(MS_COUNTER3)			\
	P4_PMC(FLAME_COUNTER0)			\
	P4_PMC(FLAME_COUNTER1)			\
	P4_PMC(FLAME_COUNTER2)			\
	P4_PMC(FLAME_COUNTER3)			\
	P4_PMC(IQ_COUNTER0)			\
	P4_PMC(IQ_COUNTER1)			\
	P4_PMC(IQ_COUNTER2)			\
	P4_PMC(IQ_COUNTER3)			\
	P4_PMC(IQ_COUNTER4)			\
	P4_PMC(IQ_COUNTER5)			\
	P4_PMC(NONE)

enum pmc_p4pmc {
#undef	P4_PMC
#define	P4_PMC(N)	P4_PMC_##N ,
	P4_PMCS()
};

/*
 * P4 ESCR descriptors
 */

#define	P4_ESCRS()							\
    P4_ESCR(BSU_ESCR0,	0x3A0, BPU_COUNTER0, BPU_COUNTER1, NONE)	\
    P4_ESCR(BSU_ESCR1,	0x3A1, BPU_COUNTER2, BPU_COUNTER3, NONE)	\
    P4_ESCR(FSB_ESCR0,	0x3A2, BPU_COUNTER0, BPU_COUNTER1, NONE)	\
    P4_ESCR(FSB_ESCR1,	0x3A3, BPU_COUNTER2, BPU_COUNTER3, NONE)	\
    P4_ESCR(FIRM_ESCR0,	0x3A4, FLAME_COUNTER0, FLAME_COUNTER1, NONE)	\
    P4_ESCR(FIRM_ESCR1,	0x3A5, FLAME_COUNTER2, FLAME_COUNTER3, NONE)	\
    P4_ESCR(FLAME_ESCR0, 0x3A6, FLAME_COUNTER0, FLAME_COUNTER1, NONE)	\
    P4_ESCR(FLAME_ESCR1, 0x3A7, FLAME_COUNTER2, FLAME_COUNTER3, NONE)	\
    P4_ESCR(DAC_ESCR0,	0x3A8, FLAME_COUNTER0, FLAME_COUNTER1, NONE)	\
    P4_ESCR(DAC_ESCR1,	0x3A9, FLAME_COUNTER2, FLAME_COUNTER3, NONE)	\
    P4_ESCR(MOB_ESCR0,	0x3AA, BPU_COUNTER0, BPU_COUNTER1, NONE)	\
    P4_ESCR(MOB_ESCR1,	0x3AB, BPU_COUNTER2, BPU_COUNTER3, NONE)	\
    P4_ESCR(PMH_ESCR0,	0x3AC, BPU_COUNTER0, BPU_COUNTER1, NONE)	\
    P4_ESCR(PMH_ESCR1,	0x3AD, BPU_COUNTER2, BPU_COUNTER3, NONE)	\
    P4_ESCR(SAAT_ESCR0,	0x3AE, FLAME_COUNTER0, FLAME_COUNTER1, NONE)	\
    P4_ESCR(SAAT_ESCR1,	0x3AF, FLAME_COUNTER2, FLAME_COUNTER3, NONE)	\
    P4_ESCR(U2L_ESCR0,	0x3B0, FLAME_COUNTER0, FLAME_COUNTER1, NONE)	\
    P4_ESCR(U2L_ESCR1,	0x3B1, FLAME_COUNTER2, FLAME_COUNTER3, NONE)	\
    P4_ESCR(BPU_ESCR0,	0x3B2, BPU_COUNTER0, BPU_COUNTER1, NONE)	\
    P4_ESCR(BPU_ESCR1,	0x3B3, BPU_COUNTER2, BPU_COUNTER3, NONE)	\
    P4_ESCR(IS_ESCR0,	0x3B4, BPU_COUNTER0, BPU_COUNTER1, NONE)	\
    P4_ESCR(IS_ESCR1,	0x3B5, BPU_COUNTER2, BPU_COUNTER3, NONE)	\
    P4_ESCR(ITLB_ESCR0,	0x3B6, BPU_COUNTER0, BPU_COUNTER1, NONE)	\
    P4_ESCR(ITLB_ESCR1,	0x3B7, BPU_COUNTER2, BPU_COUNTER3, NONE)	\
    P4_ESCR(CRU_ESCR0,	0x3B8, IQ_COUNTER0, IQ_COUNTER1, IQ_COUNTER4)	\
    P4_ESCR(CRU_ESCR1,	0x3B9, IQ_COUNTER2, IQ_COUNTER3, IQ_COUNTER5)	\
    P4_ESCR(IQ_ESCR0,	0x3BA, IQ_COUNTER0, IQ_COUNTER1, IQ_COUNTER4)	\
    P4_ESCR(IQ_ESCR1,	0x3BB, IQ_COUNTER1, IQ_COUNTER3, IQ_COUNTER5)	\
    P4_ESCR(RAT_ESCR0,	0x3BC, IQ_COUNTER0, IQ_COUNTER1, IQ_COUNTER4)	\
    P4_ESCR(RAT_ESCR1,	0x3BD, IQ_COUNTER2, IQ_COUNTER3, IQ_COUNTER5)	\
    P4_ESCR(SSU_ESCR0,	0x3BE, IQ_COUNTER0, IQ_COUNTER2, IQ_COUNTER4)	\
    P4_ESCR(MS_ESCR0,	0x3C0, MS_COUNTER0, MS_COUNTER1, NONE)		\
    P4_ESCR(MS_ESCR1,	0x3C1, MS_COUNTER2, MS_COUNTER3, NONE)		\
    P4_ESCR(TBPU_ESCR0,	0x3C2, MS_COUNTER0, MS_COUNTER1, NONE)		\
    P4_ESCR(TBPU_ESCR1,	0x3C3, MS_COUNTER2, MS_COUNTER3, NONE)		\
    P4_ESCR(TC_ESCR0,	0x3C4, MS_COUNTER0, MS_COUNTER1, NONE)		\
    P4_ESCR(TC_ESCR1,	0x3C5, MS_COUNTER2, MS_COUNTER3, NONE)		\
    P4_ESCR(IX_ESCR0,	0x3C8, BPU_COUNTER0, BPU_COUNTER1, NONE)	\
    P4_ESCR(IX_ESCR1,	0x3C9, BPU_COUNTER2, BPU_COUNTER3, NONE)	\
    P4_ESCR(ALF_ESCR0,	0x3CA, IQ_COUNTER0, IQ_COUNTER1, IQ_COUNTER4)	\
    P4_ESCR(ALF_ESCR1,	0x3CB, IQ_COUNTER2, IQ_COUNTER3, IQ_COUNTER5)	\
    P4_ESCR(CRU_ESCR2,	0x3CC, IQ_COUNTER0, IQ_COUNTER1, IQ_COUNTER4)	\
    P4_ESCR(CRU_ESCR3,	0x3CD, IQ_COUNTER2, IQ_COUNTER3, IQ_COUNTER5)	\
    P4_ESCR(CRU_ESCR4,	0x3E0, IQ_COUNTER0, IQ_COUNTER1, IQ_COUNTER4)	\
    P4_ESCR(CRU_ESCR5,	0x3E1, IQ_COUNTER2, IQ_COUNTER3, IQ_COUNTER5)	\
    P4_ESCR(NONE,		~0,    NONE, NONE, NONE)

enum pmc_p4escr {
#define	P4_ESCR(N, MSR, P1, P2, P3)	P4_ESCR_##N ,
	P4_ESCRS()
#undef	P4_ESCR
};

struct pmc_p4escr_descr {
	const char	pm_escrname[PMC_NAME_MAX];
	u_short		pm_escr_msr;
	const enum pmc_p4pmc pm_pmcs[P4_MAX_PMC_PER_ESCR];
};

static struct pmc_p4escr_descr p4_escrs[] =
{
#define	P4_ESCR(N, MSR, P1, P2, P3)		\
	{					\
		.pm_escrname = #N,		\
		.pm_escr_msr = (MSR),		\
		.pm_pmcs =			\
		{				\
			P4_PMC_##P1,		\
			P4_PMC_##P2,		\
			P4_PMC_##P3		\
		}				\
	} ,

	P4_ESCRS()

#undef	P4_ESCR
};

/*
 * P4 Event descriptor
 */

struct p4_event_descr {
	const enum pmc_event pm_event;
	const uint32_t	pm_escr_eventselect;
	const uint32_t	pm_cccr_select;
	const char	pm_is_ti_event;
	enum pmc_p4escr	pm_escrs[P4_MAX_ESCR_PER_EVENT];
};

static struct p4_event_descr p4_events[] = {

#define	P4_EVDESCR(NAME, ESCREVENTSEL, CCCRSEL, TI_EVENT, ESCR0, ESCR1)	\
	{								\
		.pm_event            = PMC_EV_P4_##NAME,		\
		.pm_escr_eventselect = (ESCREVENTSEL),			\
		.pm_cccr_select      = (CCCRSEL),			\
		.pm_is_ti_event	     = (TI_EVENT),			\
		.pm_escrs            =					\
		{							\
			P4_ESCR_##ESCR0,				\
			P4_ESCR_##ESCR1					\
		}							\
	}

P4_EVDESCR(TC_DELIVER_MODE,	0x01, 0x01, TRUE,  TC_ESCR0,	TC_ESCR1),
P4_EVDESCR(BPU_FETCH_REQUEST,	0x03, 0x00, FALSE, BPU_ESCR0,	BPU_ESCR1),
P4_EVDESCR(ITLB_REFERENCE,	0x18, 0x03, FALSE, ITLB_ESCR0,	ITLB_ESCR1),
P4_EVDESCR(MEMORY_CANCEL,	0x02, 0x05, FALSE, DAC_ESCR0,	DAC_ESCR1),
P4_EVDESCR(MEMORY_COMPLETE,	0x08, 0x02, FALSE, SAAT_ESCR0,	SAAT_ESCR1),
P4_EVDESCR(LOAD_PORT_REPLAY,	0x04, 0x02, FALSE, SAAT_ESCR0,	SAAT_ESCR1),
P4_EVDESCR(STORE_PORT_REPLAY,	0x05, 0x02, FALSE, SAAT_ESCR0,	SAAT_ESCR1),
P4_EVDESCR(MOB_LOAD_REPLAY,	0x03, 0x02, FALSE, MOB_ESCR0,	MOB_ESCR1),
P4_EVDESCR(PAGE_WALK_TYPE,	0x01, 0x04, TRUE,  PMH_ESCR0,	PMH_ESCR1),
P4_EVDESCR(BSQ_CACHE_REFERENCE,	0x0C, 0x07, FALSE, BSU_ESCR0,	BSU_ESCR1),
P4_EVDESCR(IOQ_ALLOCATION,	0x03, 0x06, FALSE, FSB_ESCR0,	FSB_ESCR1),
P4_EVDESCR(IOQ_ACTIVE_ENTRIES,	0x1A, 0x06, FALSE, FSB_ESCR1,	NONE),
P4_EVDESCR(FSB_DATA_ACTIVITY,	0x17, 0x06, TRUE,  FSB_ESCR0,	FSB_ESCR1),
P4_EVDESCR(BSQ_ALLOCATION,	0x05, 0x07, FALSE, BSU_ESCR0,	NONE),
P4_EVDESCR(BSQ_ACTIVE_ENTRIES,	0x06, 0x07, FALSE, BSU_ESCR1,	NONE),
	/* BSQ_ACTIVE_ENTRIES inherits CPU specificity from BSQ_ALLOCATION */
P4_EVDESCR(SSE_INPUT_ASSIST,	0x34, 0x01, TRUE,  FIRM_ESCR0,	FIRM_ESCR1),
P4_EVDESCR(PACKED_SP_UOP,	0x08, 0x01, TRUE,  FIRM_ESCR0,	FIRM_ESCR1),
P4_EVDESCR(PACKED_DP_UOP,	0x0C, 0x01, TRUE,  FIRM_ESCR0,	FIRM_ESCR1),
P4_EVDESCR(SCALAR_SP_UOP,	0x0A, 0x01, TRUE,  FIRM_ESCR0,	FIRM_ESCR1),
P4_EVDESCR(SCALAR_DP_UOP,	0x0E, 0x01, TRUE,  FIRM_ESCR0,	FIRM_ESCR1),
P4_EVDESCR(64BIT_MMX_UOP,	0x02, 0x01, TRUE,  FIRM_ESCR0,	FIRM_ESCR1),
P4_EVDESCR(128BIT_MMX_UOP,	0x1A, 0x01, TRUE,  FIRM_ESCR0,	FIRM_ESCR1),
P4_EVDESCR(X87_FP_UOP,		0x04, 0x01, TRUE,  FIRM_ESCR0,	FIRM_ESCR1),
P4_EVDESCR(X87_SIMD_MOVES_UOP,	0x2E, 0x01, TRUE,  FIRM_ESCR0,	FIRM_ESCR1),
P4_EVDESCR(GLOBAL_POWER_EVENTS,	0x13, 0x06, FALSE, FSB_ESCR0,	FSB_ESCR1),
P4_EVDESCR(TC_MS_XFER,		0x05, 0x00, FALSE, MS_ESCR0,	MS_ESCR1),
P4_EVDESCR(UOP_QUEUE_WRITES,	0x09, 0x00, FALSE, MS_ESCR0,	MS_ESCR1),
P4_EVDESCR(RETIRED_MISPRED_BRANCH_TYPE,
    				0x05, 0x02, FALSE, TBPU_ESCR0,	TBPU_ESCR1),
P4_EVDESCR(RETIRED_BRANCH_TYPE,	0x04, 0x02, FALSE, TBPU_ESCR0,	TBPU_ESCR1),
P4_EVDESCR(RESOURCE_STALL,	0x01, 0x01, FALSE, ALF_ESCR0,	ALF_ESCR1),
P4_EVDESCR(WC_BUFFER,		0x05, 0x05, TRUE,  DAC_ESCR0,	DAC_ESCR1),
P4_EVDESCR(B2B_CYCLES,		0x16, 0x03, TRUE,  FSB_ESCR0,	FSB_ESCR1),
P4_EVDESCR(BNR,			0x08, 0x03, TRUE,  FSB_ESCR0,	FSB_ESCR1),
P4_EVDESCR(SNOOP,		0x06, 0x03, TRUE,  FSB_ESCR0,	FSB_ESCR1),
P4_EVDESCR(RESPONSE,		0x04, 0x03, TRUE,  FSB_ESCR0,	FSB_ESCR1),
P4_EVDESCR(FRONT_END_EVENT,	0x08, 0x05, FALSE, CRU_ESCR2,	CRU_ESCR3),
P4_EVDESCR(EXECUTION_EVENT,	0x0C, 0x05, FALSE, CRU_ESCR2,	CRU_ESCR3),
P4_EVDESCR(REPLAY_EVENT, 	0x09, 0x05, FALSE, CRU_ESCR2,	CRU_ESCR3),
P4_EVDESCR(INSTR_RETIRED,	0x02, 0x04, FALSE, CRU_ESCR0,	CRU_ESCR1),
P4_EVDESCR(UOPS_RETIRED,	0x01, 0x04, FALSE, CRU_ESCR0,	CRU_ESCR1),
P4_EVDESCR(UOP_TYPE,		0x02, 0x02, FALSE, RAT_ESCR0,	RAT_ESCR1),
P4_EVDESCR(BRANCH_RETIRED,	0x06, 0x05, FALSE, CRU_ESCR2,	CRU_ESCR3),
P4_EVDESCR(MISPRED_BRANCH_RETIRED, 0x03, 0x04, FALSE, CRU_ESCR0, CRU_ESCR1),
P4_EVDESCR(X87_ASSIST,		0x03, 0x05, FALSE, CRU_ESCR2,	CRU_ESCR3),
P4_EVDESCR(MACHINE_CLEAR,	0x02, 0x05, FALSE, CRU_ESCR2,	CRU_ESCR3)

#undef	P4_EVDESCR
};

#define	P4_EVENT_IS_TI(E) ((E)->pm_is_ti_event == TRUE)

#define	P4_NEVENTS	(PMC_EV_P4_LAST - PMC_EV_P4_FIRST + 1)

/*
 * P4 PMC descriptors
 */

struct p4pmc_descr {
	struct pmc_descr pm_descr; 	/* common information */
	enum pmc_p4pmc	pm_pmcnum;	/* PMC number */
	uint32_t	pm_pmc_msr; 	/* PERFCTR MSR address */
	uint32_t	pm_cccr_msr;  	/* CCCR MSR address */
};

static struct p4pmc_descr p4_pmcdesc[P4_NPMCS] = {
#define	P4_PMC_CAPS (PMC_CAP_INTERRUPT | PMC_CAP_USER | PMC_CAP_SYSTEM |  \
	PMC_CAP_EDGE | PMC_CAP_THRESHOLD | PMC_CAP_READ | PMC_CAP_WRITE | \
	PMC_CAP_INVERT | PMC_CAP_QUALIFIER | PMC_CAP_PRECISE |            \
	PMC_CAP_TAGGING | PMC_CAP_CASCADE)

#define	P4_PMCDESCR(N, PMC, CCCR)			\
	{						\
		.pm_descr =				\
		{					\
			.pd_name = #N,			\
			.pd_class = PMC_CLASS_P4,	\
			.pd_caps = P4_PMC_CAPS,		\
			.pd_width = 40			\
		},					\
		.pm_pmcnum      = P4_PMC_##N,		\
		.pm_cccr_msr 	= (CCCR),		\
		.pm_pmc_msr	= (PMC)			\
	}

	P4_PMCDESCR(BPU_COUNTER0,	0x300,	0x360),
	P4_PMCDESCR(BPU_COUNTER1,	0x301,	0x361),
	P4_PMCDESCR(BPU_COUNTER2,	0x302,	0x362),
	P4_PMCDESCR(BPU_COUNTER3,	0x303,	0x363),
	P4_PMCDESCR(MS_COUNTER0,	0x304,	0x364),
	P4_PMCDESCR(MS_COUNTER1,	0x305,	0x365),
	P4_PMCDESCR(MS_COUNTER2,	0x306,	0x366),
	P4_PMCDESCR(MS_COUNTER3,	0x307,	0x367),
	P4_PMCDESCR(FLAME_COUNTER0,	0x308,	0x368),
	P4_PMCDESCR(FLAME_COUNTER1,	0x309,	0x369),
	P4_PMCDESCR(FLAME_COUNTER2,	0x30A,	0x36A),
	P4_PMCDESCR(FLAME_COUNTER3,	0x30B,	0x36B),
	P4_PMCDESCR(IQ_COUNTER0,	0x30C,	0x36C),
	P4_PMCDESCR(IQ_COUNTER1,	0x30D,	0x36D),
	P4_PMCDESCR(IQ_COUNTER2,	0x30E,	0x36E),
	P4_PMCDESCR(IQ_COUNTER3,	0x30F,	0x36F),
	P4_PMCDESCR(IQ_COUNTER4,	0x310,	0x370),
	P4_PMCDESCR(IQ_COUNTER5,	0x311,	0x371),

#undef	P4_PMCDESCR
};

/* HTT support */
#define	P4_NHTT					2 /* logical processors/chip */

static int p4_system_has_htt;

/*
 * Per-CPU data structure for P4 class CPUs
 *
 * [19 struct pmc_hw structures]
 * [45 ESCRs status bytes]
 * [per-cpu spin mutex]
 * [19 flag fields for holding config flags and a runcount]
 * [19*2 hw value fields]	(Thread mode PMC support)
 *    or
 * [19*2 EIP values]		(Sampling mode PMCs)
 * [19*2 pmc value fields]	(Thread mode PMC support))
 */

struct p4_cpu {
	struct pmc_hw	pc_p4pmcs[P4_NPMCS];
	char		pc_escrs[P4_NESCR];
	struct mtx	pc_mtx;		/* spin lock */
	uint32_t	pc_intrflag;	/* NMI handler flags */
	unsigned int	pc_intrlock;	/* NMI handler spin lock */
	unsigned char	pc_flags[P4_NPMCS]; /* 4 bits each: {cfg,run}count */
	union {
		pmc_value_t pc_hw[P4_NPMCS * P4_NHTT];
		uintptr_t   pc_ip[P4_NPMCS * P4_NHTT];
	}		pc_si;
	pmc_value_t	pc_pmc_values[P4_NPMCS * P4_NHTT];
};

static struct p4_cpu **p4_pcpu;

#define	P4_PCPU_PMC_VALUE(PC,RI,CPU) 	(PC)->pc_pmc_values[(RI)*((CPU) & 1)]
#define	P4_PCPU_HW_VALUE(PC,RI,CPU)	(PC)->pc_si.pc_hw[(RI)*((CPU) & 1)]
#define	P4_PCPU_SAVED_IP(PC,RI,CPU)	(PC)->pc_si.pc_ip[(RI)*((CPU) & 1)]

#define	P4_PCPU_GET_FLAGS(PC,RI,MASK)	((PC)->pc_flags[(RI)] & (MASK))
#define	P4_PCPU_SET_FLAGS(PC,RI,MASK,VAL)	do {	\
	char _tmp;					\
	_tmp = (PC)->pc_flags[(RI)];			\
	_tmp &= ~(MASK);				\
	_tmp |= (VAL) & (MASK);				\
	(PC)->pc_flags[(RI)] = _tmp;			\
} while (0)

#define	P4_PCPU_GET_RUNCOUNT(PC,RI)	P4_PCPU_GET_FLAGS(PC,RI,0x0F)
#define	P4_PCPU_SET_RUNCOUNT(PC,RI,V)	P4_PCPU_SET_FLAGS(PC,RI,0x0F,V)

#define	P4_PCPU_GET_CFGFLAGS(PC,RI)	(P4_PCPU_GET_FLAGS(PC,RI,0xF0) >> 4)
#define	P4_PCPU_SET_CFGFLAGS(PC,RI,C)	P4_PCPU_SET_FLAGS(PC,RI,0xF0,((C) <<4))

#define	P4_CPU_TO_FLAG(C)		(P4_CPU_IS_HTT_SECONDARY(cpu) ? 0x2 : 0x1)

#define	P4_PCPU_GET_INTRFLAG(PC,I)	((PC)->pc_intrflag & (1 << (I)))
#define	P4_PCPU_SET_INTRFLAG(PC,I,V)	do {		\
		uint32_t __mask;			\
		__mask = 1 << (I);			\
		if ((V))				\
			(PC)->pc_intrflag |= __mask;	\
		else					\
			(PC)->pc_intrflag &= ~__mask;	\
	} while (0)

/*
 * A minimal spin lock implementation for use inside the NMI handler.
 *
 * We don't want to use a regular spin lock here, because curthread
 * may not be consistent at the time the handler is invoked.
 */
#define	P4_PCPU_ACQ_INTR_SPINLOCK(PC) do {				\
		while (!atomic_cmpset_acq_int(&pc->pc_intrlock, 0, 1))	\
			ia32_pause();					\
	} while (0)
#define	P4_PCPU_REL_INTR_SPINLOCK(PC) 					\
	atomic_store_rel_int(&pc->pc_intrlock, 0);

/* ESCR row disposition */
static int p4_escrdisp[P4_NESCR];

#define	P4_ESCR_ROW_DISP_IS_THREAD(E)		(p4_escrdisp[(E)] > 0)
#define	P4_ESCR_ROW_DISP_IS_STANDALONE(E)	(p4_escrdisp[(E)] < 0)
#define	P4_ESCR_ROW_DISP_IS_FREE(E)		(p4_escrdisp[(E)] == 0)

#define	P4_ESCR_MARK_ROW_STANDALONE(E) do {				\
	KASSERT(p4_escrdisp[(E)] <= 0, ("[p4,%d] row disposition error",\
		    __LINE__));						\
	atomic_add_int(&p4_escrdisp[(E)], -1);				\
	KASSERT(p4_escrdisp[(E)] >= (-pmc_cpu_max_active()), 		\
		("[p4,%d] row disposition error", __LINE__));		\
} while (0)

#define	P4_ESCR_UNMARK_ROW_STANDALONE(E) do {				\
	atomic_add_int(&p4_escrdisp[(E)], 1);				\
	KASSERT(p4_escrdisp[(E)] <= 0, ("[p4,%d] row disposition error",\
		    __LINE__));						\
} while (0)

#define	P4_ESCR_MARK_ROW_THREAD(E) do {					 \
	KASSERT(p4_escrdisp[(E)] >= 0, ("[p4,%d] row disposition error", \
		    __LINE__));						 \
	atomic_add_int(&p4_escrdisp[(E)], 1);				 \
} while (0)

#define	P4_ESCR_UNMARK_ROW_THREAD(E) do {				 \
	atomic_add_int(&p4_escrdisp[(E)], -1);				 \
	KASSERT(p4_escrdisp[(E)] >= 0, ("[p4,%d] row disposition error", \
		    __LINE__));						 \
} while (0)

#define	P4_PMC_IS_STOPPED(cccr)	((rdmsr(cccr) & P4_CCCR_ENABLE) == 0)

#define	P4_CPU_IS_HTT_SECONDARY(cpu)					\
	(p4_system_has_htt ? ((cpu) & 1) : 0)
#define	P4_TO_HTT_PRIMARY(cpu) 						\
	(p4_system_has_htt ? ((cpu) & ~1) : (cpu))

#define	P4_CCCR_Tx_MASK	(~(P4_CCCR_OVF_PMI_T0|P4_CCCR_OVF_PMI_T1|	\
			     P4_CCCR_ENABLE|P4_CCCR_OVF))
#define	P4_ESCR_Tx_MASK	(~(P4_ESCR_T0_OS|P4_ESCR_T0_USR|P4_ESCR_T1_OS|	\
			     P4_ESCR_T1_USR))

/*
 * support routines
 */

static struct p4_event_descr *
p4_find_event(enum pmc_event ev)
{
	int n;

	for (n = 0; n < P4_NEVENTS; n++)
		if (p4_events[n].pm_event == ev)
			break;
	if (n == P4_NEVENTS)
		return (NULL);
	return (&p4_events[n]);
}

/*
 * Initialize per-cpu state
 */

static int
p4_pcpu_init(struct pmc_mdep *md, int cpu)
{
	char *pescr;
	int n, first_ri, phycpu;
	struct pmc_hw *phw;
	struct p4_cpu *p4c;
	struct pmc_cpu *pc, *plc;

	KASSERT(cpu >= 0 && cpu < pmc_cpu_max(),
	    ("[p4,%d] insane cpu number %d", __LINE__, cpu));

	PMCDBG(MDP,INI,0, "p4-init cpu=%d is-primary=%d", cpu,
	    pmc_cpu_is_primary(cpu) != 0);

	first_ri = md->pmd_classdep[PMC_MDEP_CLASS_INDEX_P4].pcd_ri;

	/*
	 * The two CPUs in an HT pair share their per-cpu state.
	 *
	 * For HT capable CPUs, we assume that the two logical
	 * processors in the HT pair get two consecutive CPU ids
	 * starting with an even id #.
	 *
	 * The primary CPU (the even numbered CPU of the pair) would
	 * have been initialized prior to the initialization for the
	 * secondary.
	 */

	if (!pmc_cpu_is_primary(cpu) && (cpu & 1)) {

		p4_system_has_htt = 1;

		phycpu = P4_TO_HTT_PRIMARY(cpu);
		pc = pmc_pcpu[phycpu];
		plc = pmc_pcpu[cpu];

		KASSERT(plc != pc, ("[p4,%d] per-cpu config error", __LINE__));

		PMCDBG(MDP,INI,1, "p4-init cpu=%d phycpu=%d pc=%p", cpu,
		    phycpu, pc);
		KASSERT(pc, ("[p4,%d] Null Per-Cpu state cpu=%d phycpu=%d",
		    __LINE__, cpu, phycpu));

		/* PMCs are shared with the physical CPU. */
		for (n = 0; n < P4_NPMCS; n++)
			plc->pc_hwpmcs[n + first_ri] =
			    pc->pc_hwpmcs[n + first_ri];

		return (0);
	}

	p4c = malloc(sizeof(struct p4_cpu), M_PMC, M_WAITOK|M_ZERO);

	if (p4c == NULL)
		return (ENOMEM);

	pc = pmc_pcpu[cpu];

	KASSERT(pc != NULL, ("[p4,%d] cpu %d null per-cpu", __LINE__, cpu));

	p4_pcpu[cpu] = p4c;
	phw = p4c->pc_p4pmcs;

	for (n = 0; n < P4_NPMCS; n++, phw++) {
		phw->phw_state   = PMC_PHW_FLAG_IS_ENABLED |
		    PMC_PHW_CPU_TO_STATE(cpu) | PMC_PHW_INDEX_TO_STATE(n);
		phw->phw_pmc     = NULL;
		pc->pc_hwpmcs[n + first_ri] = phw;
	}

	pescr = p4c->pc_escrs;
	for (n = 0; n < P4_NESCR; n++)
		*pescr++ = P4_INVALID_PMC_INDEX;

	mtx_init(&p4c->pc_mtx, "p4-pcpu", "pmc-leaf", MTX_SPIN);

	return (0);
}

/*
 * Destroy per-cpu state.
 */

static int
p4_pcpu_fini(struct pmc_mdep *md, int cpu)
{
	int first_ri, i;
	struct p4_cpu *p4c;
	struct pmc_cpu *pc;

	PMCDBG(MDP,INI,0, "p4-cleanup cpu=%d", cpu);

	pc = pmc_pcpu[cpu];
	first_ri = md->pmd_classdep[PMC_MDEP_CLASS_INDEX_P4].pcd_ri;

	for (i = 0; i < P4_NPMCS; i++)
		pc->pc_hwpmcs[i + first_ri] = NULL;

	if (!pmc_cpu_is_primary(cpu) && (cpu & 1))
		return (0);

	p4c = p4_pcpu[cpu];

	KASSERT(p4c != NULL, ("[p4,%d] NULL pcpu", __LINE__));

	/* Turn off all PMCs on this CPU */
	for (i = 0; i < P4_NPMCS - 1; i++)
		wrmsr(P4_CCCR_MSR_FIRST + i,
		    rdmsr(P4_CCCR_MSR_FIRST + i) & ~P4_CCCR_ENABLE);

	mtx_destroy(&p4c->pc_mtx);

	free(p4c, M_PMC);

	p4_pcpu[cpu] = NULL;

	return (0);
}

/*
 * Read a PMC
 */

static int
p4_read_pmc(int cpu, int ri, pmc_value_t *v)
{
	struct pmc *pm;
	pmc_value_t tmp;
	struct p4_cpu *pc;
	enum pmc_mode mode;
	struct p4pmc_descr *pd;

	KASSERT(cpu >= 0 && cpu < pmc_cpu_max(),
	    ("[p4,%d] illegal CPU value %d", __LINE__, cpu));
	KASSERT(ri >= 0 && ri < P4_NPMCS,
	    ("[p4,%d] illegal row-index %d", __LINE__, ri));

	pc = p4_pcpu[P4_TO_HTT_PRIMARY(cpu)];
	pm = pc->pc_p4pmcs[ri].phw_pmc;
	pd = &p4_pmcdesc[ri];

	KASSERT(pm != NULL,
	    ("[p4,%d] No owner for HWPMC [cpu%d,pmc%d]", __LINE__, cpu, ri));

	KASSERT(pd->pm_descr.pd_class == PMC_TO_CLASS(pm),
	    ("[p4,%d] class mismatch pd %d != id class %d", __LINE__,
	    pd->pm_descr.pd_class, PMC_TO_CLASS(pm)));

	mode = PMC_TO_MODE(pm);

	PMCDBG(MDP,REA,1, "p4-read cpu=%d ri=%d mode=%d", cpu, ri, mode);

	KASSERT(pd->pm_descr.pd_class == PMC_CLASS_P4,
	    ("[p4,%d] unknown PMC class %d", __LINE__, pd->pm_descr.pd_class));

	tmp = rdmsr(p4_pmcdesc[ri].pm_pmc_msr);

	if (PMC_IS_VIRTUAL_MODE(mode)) {
		if (tmp < P4_PCPU_HW_VALUE(pc,ri,cpu)) /* 40 bit overflow */
			tmp += (P4_PERFCTR_MASK + 1) -
			    P4_PCPU_HW_VALUE(pc,ri,cpu);
		else
			tmp -= P4_PCPU_HW_VALUE(pc,ri,cpu);
		tmp += P4_PCPU_PMC_VALUE(pc,ri,cpu);
	}

	if (PMC_IS_SAMPLING_MODE(mode)) /* undo transformation */
		*v = P4_PERFCTR_VALUE_TO_RELOAD_COUNT(tmp);
	else
		*v = tmp;

	PMCDBG(MDP,REA,2, "p4-read -> %jx", *v);

	return (0);
}

/*
 * Write a PMC
 */

static int
p4_write_pmc(int cpu, int ri, pmc_value_t v)
{
	enum pmc_mode mode;
	struct pmc *pm;
	struct p4_cpu *pc;
	const struct pmc_hw *phw;
	const struct p4pmc_descr *pd;

	KASSERT(cpu >= 0 && cpu < pmc_cpu_max(),
	    ("[amd,%d] illegal CPU value %d", __LINE__, cpu));
	KASSERT(ri >= 0 && ri < P4_NPMCS,
	    ("[amd,%d] illegal row-index %d", __LINE__, ri));

	pc  = p4_pcpu[P4_TO_HTT_PRIMARY(cpu)];
	phw = &pc->pc_p4pmcs[ri];
	pm  = phw->phw_pmc;
	pd  = &p4_pmcdesc[ri];

	KASSERT(pm != NULL,
	    ("[p4,%d] No owner for HWPMC [cpu%d,pmc%d]", __LINE__,
		cpu, ri));

	mode = PMC_TO_MODE(pm);

	PMCDBG(MDP,WRI,1, "p4-write cpu=%d ri=%d mode=%d v=%jx", cpu, ri,
	    mode, v);

	/*
	 * write the PMC value to the register/saved value: for
	 * sampling mode PMCs, the value to be programmed into the PMC
	 * counter is -(C+1) where 'C' is the requested sample rate.
	 */
	if (PMC_IS_SAMPLING_MODE(mode))
		v = P4_RELOAD_COUNT_TO_PERFCTR_VALUE(v);

	if (PMC_IS_SYSTEM_MODE(mode))
		wrmsr(pd->pm_pmc_msr, v);
	else
		P4_PCPU_PMC_VALUE(pc,ri,cpu) = v;

	return (0);
}

/*
 * Configure a PMC 'pm' on the given CPU and row-index.
 *
 * 'pm' may be NULL to indicate de-configuration.
 *
 * On HTT systems, a PMC may get configured twice, once for each
 * "logical" CPU.  We track this using the CFGFLAGS field of the
 * per-cpu state; this field is a bit mask with one bit each for
 * logical CPUs 0 & 1.
 */

static int
p4_config_pmc(int cpu, int ri, struct pmc *pm)
{
	struct pmc_hw *phw;
	struct p4_cpu *pc;
	int cfgflags, cpuflag;

	KASSERT(cpu >= 0 && cpu < pmc_cpu_max(),
	    ("[p4,%d] illegal CPU %d", __LINE__, cpu));

	KASSERT(ri >= 0 && ri < P4_NPMCS,
	    ("[p4,%d] illegal row-index %d", __LINE__, ri));

	PMCDBG(MDP,CFG,1, "cpu=%d ri=%d pm=%p", cpu, ri, pm);

	pc  = p4_pcpu[P4_TO_HTT_PRIMARY(cpu)];
	phw = &pc->pc_p4pmcs[ri];

	KASSERT(pm == NULL || phw->phw_pmc == NULL ||
	    (p4_system_has_htt && phw->phw_pmc == pm),
	    ("[p4,%d] hwpmc not unconfigured before re-config", __LINE__));

	mtx_lock_spin(&pc->pc_mtx);
	cfgflags = P4_PCPU_GET_CFGFLAGS(pc,ri);

	KASSERT(cfgflags >= 0 || cfgflags <= 3,
	    ("[p4,%d] illegal cfgflags cfg=%d on cpu=%d ri=%d", __LINE__,
		cfgflags, cpu, ri));

	KASSERT(cfgflags == 0 || phw->phw_pmc,
	    ("[p4,%d] cpu=%d ri=%d pmc configured with zero cfg count",
		__LINE__, cpu, ri));

	cpuflag = P4_CPU_TO_FLAG(cpu);

	if (pm) {		/* config */
		if (cfgflags == 0)
			phw->phw_pmc = pm;

		KASSERT(phw->phw_pmc == pm,
		    ("[p4,%d] cpu=%d ri=%d config %p != hw %p",
			__LINE__, cpu, ri, pm, phw->phw_pmc));

		cfgflags |= cpuflag;
	} else {		/* unconfig */
		cfgflags &= ~cpuflag;

		if (cfgflags == 0)
			phw->phw_pmc = NULL;
	}

	KASSERT(cfgflags >= 0 || cfgflags <= 3,
	    ("[p4,%d] illegal runcount cfg=%d on cpu=%d ri=%d", __LINE__,
		cfgflags, cpu, ri));

	P4_PCPU_SET_CFGFLAGS(pc,ri,cfgflags);

	mtx_unlock_spin(&pc->pc_mtx);

	return (0);
}

/*
 * Retrieve a configured PMC pointer from hardware state.
 */

static int
p4_get_config(int cpu, int ri, struct pmc **ppm)
{
	int cfgflags;
	struct p4_cpu *pc;

	KASSERT(cpu >= 0 && cpu < pmc_cpu_max(),
	    ("[p4,%d] illegal CPU %d", __LINE__, cpu));
	KASSERT(ri >= 0 && ri < P4_NPMCS,
	    ("[p4,%d] illegal row-index %d", __LINE__, ri));

	pc = p4_pcpu[P4_TO_HTT_PRIMARY(cpu)];

	mtx_lock_spin(&pc->pc_mtx);
	cfgflags = P4_PCPU_GET_CFGFLAGS(pc,ri);
	mtx_unlock_spin(&pc->pc_mtx);

	if (cfgflags & P4_CPU_TO_FLAG(cpu))
		*ppm = pc->pc_p4pmcs[ri].phw_pmc; /* PMC config'ed on this CPU */
	else
		*ppm = NULL;

	return 0;
}

/*
 * Allocate a PMC.
 *
 * The allocation strategy differs between HTT and non-HTT systems.
 *
 * The non-HTT case:
 *   - Given the desired event and the PMC row-index, lookup the
 *   list of valid ESCRs for the event.
 *   - For each valid ESCR:
 *     - Check if the ESCR is free and the ESCR row is in a compatible
 *       mode (i.e., system or process))
 *     - Check if the ESCR is usable with a P4 PMC at the desired row-index.
 *   If everything matches, we determine the appropriate bit values for the
 *   ESCR and CCCR registers.
 *
 * The HTT case:
 *
 * - Process mode PMCs require special care.  The FreeBSD scheduler could
 *   schedule any two processes on the same physical CPU.  We need to ensure
 *   that a given PMC row-index is never allocated to two different
 *   PMCs owned by different user-processes.
 *   This is ensured by always allocating a PMC from a 'FREE' PMC row
 *   if the system has HTT active.
 * - A similar check needs to be done for ESCRs; we do not want two PMCs
 *   using the same ESCR to be scheduled at the same time.  Thus ESCR
 *   allocation is also restricted to FREE rows if the system has HTT
 *   enabled.
 * - Thirdly, some events are 'thread-independent' terminology, i.e.,
 *   the PMC hardware cannot distinguish between events caused by
 *   different logical CPUs.  This makes it impossible to assign events
 *   to a given thread of execution.  If the system has HTT enabled,
 *   these events are not allowed for process-mode PMCs.
 */

static int
p4_allocate_pmc(int cpu, int ri, struct pmc *pm,
    const struct pmc_op_pmcallocate *a)
{
	int found, n, m;
	uint32_t caps, cccrvalue, escrvalue, tflags;
	enum pmc_p4escr escr;
	struct p4_cpu *pc;
	struct p4_event_descr *pevent;
	const struct p4pmc_descr *pd;

	KASSERT(cpu >= 0 && cpu < pmc_cpu_max(),
	    ("[p4,%d] illegal CPU %d", __LINE__, cpu));
	KASSERT(ri >= 0 && ri < P4_NPMCS,
	    ("[p4,%d] illegal row-index value %d", __LINE__, ri));

	pd = &p4_pmcdesc[ri];

	PMCDBG(MDP,ALL,1, "p4-allocate ri=%d class=%d pmccaps=0x%x "
	    "reqcaps=0x%x", ri, pd->pm_descr.pd_class, pd->pm_descr.pd_caps,
	    pm->pm_caps);

	/* check class */
	if (pd->pm_descr.pd_class != a->pm_class)
		return (EINVAL);

	/* check requested capabilities */
	caps = a->pm_caps;
	if ((pd->pm_descr.pd_caps & caps) != caps)
		return (EPERM);

	/*
	 * If the system has HTT enabled, and the desired allocation
	 * mode is process-private, and the PMC row disposition is not
	 * FREE (0), decline the allocation.
	 */

	if (p4_system_has_htt &&
	    PMC_IS_VIRTUAL_MODE(PMC_TO_MODE(pm)) &&
	    pmc_getrowdisp(ri) != 0)
		return (EBUSY);

	KASSERT(pd->pm_descr.pd_class == PMC_CLASS_P4,
	    ("[p4,%d] unknown PMC class %d", __LINE__,
		pd->pm_descr.pd_class));

	if (pm->pm_event < PMC_EV_P4_FIRST ||
	    pm->pm_event > PMC_EV_P4_LAST)
		return (EINVAL);

	if ((pevent = p4_find_event(pm->pm_event)) == NULL)
		return (ESRCH);

	PMCDBG(MDP,ALL,2, "pevent={ev=%d,escrsel=0x%x,cccrsel=0x%x,isti=%d}",
	    pevent->pm_event, pevent->pm_escr_eventselect,
	    pevent->pm_cccr_select, pevent->pm_is_ti_event);

	/*
	 * Some PMC events are 'thread independent'and therefore
	 * cannot be used for process-private modes if HTT is being
	 * used.
	 */

	if (P4_EVENT_IS_TI(pevent) &&
	    PMC_IS_VIRTUAL_MODE(PMC_TO_MODE(pm)) &&
	    p4_system_has_htt)
		return (EINVAL);

	pc = p4_pcpu[P4_TO_HTT_PRIMARY(cpu)];

	found   = 0;

	/* look for a suitable ESCR for this event */
	for (n = 0; n < P4_MAX_ESCR_PER_EVENT && !found; n++) {
		if ((escr = pevent->pm_escrs[n]) == P4_ESCR_NONE)
			break;	/* out of ESCRs */
		/*
		 * Check ESCR row disposition.
		 *
		 * If the request is for a system-mode PMC, then the
		 * ESCR row should not be in process-virtual mode, and
		 * should also be free on the current CPU.
		 */

		if (PMC_IS_SYSTEM_MODE(PMC_TO_MODE(pm))) {
		    if (P4_ESCR_ROW_DISP_IS_THREAD(escr) ||
			pc->pc_escrs[escr] != P4_INVALID_PMC_INDEX)
			    continue;
		}

		/*
		 * If the request is for a process-virtual PMC, and if
		 * HTT is not enabled, we can use an ESCR row that is
		 * either FREE or already in process mode.
		 *
		 * If HTT is enabled, then we need to ensure that a
		 * given ESCR is never allocated to two PMCS that
		 * could run simultaneously on the two logical CPUs of
		 * a CPU package.  We ensure this be only allocating
		 * ESCRs from rows marked as 'FREE'.
		 */

		if (PMC_IS_VIRTUAL_MODE(PMC_TO_MODE(pm))) {
			if (p4_system_has_htt) {
				if (!P4_ESCR_ROW_DISP_IS_FREE(escr))
					continue;
			} else
				if (P4_ESCR_ROW_DISP_IS_STANDALONE(escr))
					continue;
		}

		/*
		 * We found a suitable ESCR for this event.  Now check if
		 * this escr can work with the PMC at row-index 'ri'.
		 */

		for (m = 0; m < P4_MAX_PMC_PER_ESCR; m++)
			if (p4_escrs[escr].pm_pmcs[m] == pd->pm_pmcnum) {
				found = 1;
				break;
			}
	}

	if (found == 0)
		return (ESRCH);

	KASSERT((int) escr >= 0 && escr < P4_NESCR,
	    ("[p4,%d] illegal ESCR value %d", __LINE__, escr));

	/* mark ESCR row mode */
	if (PMC_IS_SYSTEM_MODE(PMC_TO_MODE(pm))) {
		pc->pc_escrs[escr] = ri; /* mark ESCR as in use on this cpu */
		P4_ESCR_MARK_ROW_STANDALONE(escr);
	} else {
		KASSERT(pc->pc_escrs[escr] == P4_INVALID_PMC_INDEX,
		    ("[p4,%d] escr[%d] already in use", __LINE__, escr));
		P4_ESCR_MARK_ROW_THREAD(escr);
	}

	pm->pm_md.pm_p4.pm_p4_escrmsr   = p4_escrs[escr].pm_escr_msr;
	pm->pm_md.pm_p4.pm_p4_escr      = escr;

	cccrvalue = P4_CCCR_TO_ESCR_SELECT(pevent->pm_cccr_select);
	escrvalue = P4_ESCR_TO_EVENT_SELECT(pevent->pm_escr_eventselect);

	/* CCCR fields */
	if (caps & PMC_CAP_THRESHOLD)
		cccrvalue |= (a->pm_md.pm_p4.pm_p4_cccrconfig &
		    P4_CCCR_THRESHOLD_MASK) | P4_CCCR_COMPARE;

	if (caps & PMC_CAP_EDGE)
		cccrvalue |= P4_CCCR_EDGE;

	if (caps & PMC_CAP_INVERT)
		cccrvalue |= P4_CCCR_COMPLEMENT;

	if (p4_system_has_htt)
		cccrvalue |= a->pm_md.pm_p4.pm_p4_cccrconfig &
		    P4_CCCR_ACTIVE_THREAD_MASK;
	else			/* no HTT; thread field should be '11b' */
		cccrvalue |= P4_CCCR_TO_ACTIVE_THREAD(0x3);

	if (caps & PMC_CAP_CASCADE)
		cccrvalue |= P4_CCCR_CASCADE;

	/* On HTT systems the PMI T0 field may get moved to T1 at pmc start */
	if (caps & PMC_CAP_INTERRUPT)
		cccrvalue |= P4_CCCR_OVF_PMI_T0;

	/* ESCR fields */
	if (caps & PMC_CAP_QUALIFIER)
		escrvalue |= a->pm_md.pm_p4.pm_p4_escrconfig &
		    P4_ESCR_EVENT_MASK_MASK;
	if (caps & PMC_CAP_TAGGING)
		escrvalue |= (a->pm_md.pm_p4.pm_p4_escrconfig &
		    P4_ESCR_TAG_VALUE_MASK) | P4_ESCR_TAG_ENABLE;
	if (caps & PMC_CAP_QUALIFIER)
		escrvalue |= (a->pm_md.pm_p4.pm_p4_escrconfig &
		    P4_ESCR_EVENT_MASK_MASK);

	/* HTT: T0_{OS,USR} bits may get moved to T1 at pmc start */
	tflags = 0;
	if (caps & PMC_CAP_SYSTEM)
		tflags |= P4_ESCR_T0_OS;
	if (caps & PMC_CAP_USER)
		tflags |= P4_ESCR_T0_USR;
	if (tflags == 0)
		tflags = (P4_ESCR_T0_OS|P4_ESCR_T0_USR);
	escrvalue |= tflags;

	pm->pm_md.pm_p4.pm_p4_cccrvalue = cccrvalue;
	pm->pm_md.pm_p4.pm_p4_escrvalue = escrvalue;

	PMCDBG(MDP,ALL,2, "p4-allocate cccrsel=0x%x cccrval=0x%x "
	    "escr=%d escrmsr=0x%x escrval=0x%x", pevent->pm_cccr_select,
	    cccrvalue, escr, pm->pm_md.pm_p4.pm_p4_escrmsr, escrvalue);

	return (0);
}

/*
 * release a PMC.
 */

static int
p4_release_pmc(int cpu, int ri, struct pmc *pm)
{
	enum pmc_p4escr escr;
	struct p4_cpu *pc;

	KASSERT(ri >= 0 && ri < P4_NPMCS,
	    ("[p4,%d] illegal row-index %d", __LINE__, ri));

	escr = pm->pm_md.pm_p4.pm_p4_escr;

	PMCDBG(MDP,REL,1, "p4-release cpu=%d ri=%d escr=%d", cpu, ri, escr);

	if (PMC_IS_SYSTEM_MODE(PMC_TO_MODE(pm))) {
		pc  = p4_pcpu[P4_TO_HTT_PRIMARY(cpu)];

		KASSERT(pc->pc_p4pmcs[ri].phw_pmc == NULL,
		    ("[p4,%d] releasing configured PMC ri=%d", __LINE__, ri));

		P4_ESCR_UNMARK_ROW_STANDALONE(escr);
		KASSERT(pc->pc_escrs[escr] == ri,
		    ("[p4,%d] escr[%d] not allocated to ri %d", __LINE__,
			escr, ri));
	        pc->pc_escrs[escr] = P4_INVALID_PMC_INDEX; /* mark as free */
	} else
		P4_ESCR_UNMARK_ROW_THREAD(escr);

	return (0);
}

/*
 * Start a PMC
 */

static int
p4_start_pmc(int cpu, int ri)
{
	int rc;
	struct pmc *pm;
	struct p4_cpu *pc;
	struct p4pmc_descr *pd;
	uint32_t cccrvalue, cccrtbits, escrvalue, escrmsr, escrtbits;

	KASSERT(cpu >= 0 && cpu < pmc_cpu_max(),
	    ("[p4,%d] illegal CPU value %d", __LINE__, cpu));
	KASSERT(ri >= 0 && ri < P4_NPMCS,
	    ("[p4,%d] illegal row-index %d", __LINE__, ri));

	pc = p4_pcpu[P4_TO_HTT_PRIMARY(cpu)];
	pm = pc->pc_p4pmcs[ri].phw_pmc;
	pd = &p4_pmcdesc[ri];

	KASSERT(pm != NULL,
	    ("[p4,%d] starting cpu%d,pmc%d with null pmc", __LINE__, cpu, ri));

	PMCDBG(MDP,STA,1, "p4-start cpu=%d ri=%d", cpu, ri);

	KASSERT(pd->pm_descr.pd_class == PMC_CLASS_P4,
	    ("[p4,%d] wrong PMC class %d", __LINE__,
		pd->pm_descr.pd_class));

	/* retrieve the desired CCCR/ESCR values from the PMC */
	cccrvalue = pm->pm_md.pm_p4.pm_p4_cccrvalue;
	escrvalue = pm->pm_md.pm_p4.pm_p4_escrvalue;
	escrmsr   = pm->pm_md.pm_p4.pm_p4_escrmsr;

	/* extract and zero the logical processor selection bits */
	cccrtbits = cccrvalue & P4_CCCR_OVF_PMI_T0;
	escrtbits = escrvalue & (P4_ESCR_T0_OS|P4_ESCR_T0_USR);
	cccrvalue &= ~P4_CCCR_OVF_PMI_T0;
	escrvalue &= ~(P4_ESCR_T0_OS|P4_ESCR_T0_USR);

	if (P4_CPU_IS_HTT_SECONDARY(cpu)) { /* shift T0 bits to T1 position */
		cccrtbits <<= 1;
		escrtbits >>= 2;
	}

	/* start system mode PMCs directly */
	if (PMC_IS_SYSTEM_MODE(PMC_TO_MODE(pm))) {
		wrmsr(escrmsr, escrvalue | escrtbits);
		wrmsr(pd->pm_cccr_msr, cccrvalue | cccrtbits | P4_CCCR_ENABLE);
		return 0;
	}

	/*
	 * Thread mode PMCs
	 *
	 * On HTT machines, the same PMC could be scheduled on the
	 * same physical CPU twice (once for each logical CPU), for
	 * example, if two threads of a multi-threaded process get
	 * scheduled on the same CPU.
	 *
	 */

	mtx_lock_spin(&pc->pc_mtx);

	rc = P4_PCPU_GET_RUNCOUNT(pc,ri);
	KASSERT(rc == 0 || rc == 1,
	    ("[p4,%d] illegal runcount cpu=%d ri=%d rc=%d", __LINE__, cpu, ri,
		rc));

	if (rc == 0) {		/* 1st CPU and the non-HTT case */

		KASSERT(P4_PMC_IS_STOPPED(pd->pm_cccr_msr),
		    ("[p4,%d] cpu=%d ri=%d cccr=0x%x not stopped", __LINE__,
			cpu, ri, pd->pm_cccr_msr));

		/* write out the low 40 bits of the saved value to hardware */
		wrmsr(pd->pm_pmc_msr,
		    P4_PCPU_PMC_VALUE(pc,ri,cpu) & P4_PERFCTR_MASK);

	} else if (rc == 1) {		/* 2nd CPU */

		/*
		 * Stop the PMC and retrieve the CCCR and ESCR values
		 * from their MSRs, and turn on the additional T[0/1]
		 * bits for the 2nd CPU.
		 */

		cccrvalue = rdmsr(pd->pm_cccr_msr);
		wrmsr(pd->pm_cccr_msr, cccrvalue & ~P4_CCCR_ENABLE);

		/* check that the configuration bits read back match the PMC */
		KASSERT((cccrvalue & P4_CCCR_Tx_MASK) ==
		    (pm->pm_md.pm_p4.pm_p4_cccrvalue & P4_CCCR_Tx_MASK),
		    ("[p4,%d] Extra CCCR bits cpu=%d rc=%d ri=%d "
			"cccr=0x%x PMC=0x%x", __LINE__, cpu, rc, ri,
			cccrvalue & P4_CCCR_Tx_MASK,
			pm->pm_md.pm_p4.pm_p4_cccrvalue & P4_CCCR_Tx_MASK));
		KASSERT(cccrvalue & P4_CCCR_ENABLE,
		    ("[p4,%d] 2nd cpu rc=%d cpu=%d ri=%d not running",
			__LINE__, rc, cpu, ri));
		KASSERT((cccrvalue & cccrtbits) == 0,
		    ("[p4,%d] CCCR T0/T1 mismatch rc=%d cpu=%d ri=%d"
		     "cccrvalue=0x%x tbits=0x%x", __LINE__, rc, cpu, ri,
			cccrvalue, cccrtbits));

		escrvalue = rdmsr(escrmsr);

		KASSERT((escrvalue & P4_ESCR_Tx_MASK) ==
		    (pm->pm_md.pm_p4.pm_p4_escrvalue & P4_ESCR_Tx_MASK),
		    ("[p4,%d] Extra ESCR bits cpu=%d rc=%d ri=%d "
			"escr=0x%x pm=0x%x", __LINE__, cpu, rc, ri,
			escrvalue & P4_ESCR_Tx_MASK,
			pm->pm_md.pm_p4.pm_p4_escrvalue & P4_ESCR_Tx_MASK));
		KASSERT((escrvalue & escrtbits) == 0,
		    ("[p4,%d] ESCR T0/T1 mismatch rc=%d cpu=%d ri=%d "
		     "escrmsr=0x%x escrvalue=0x%x tbits=0x%x", __LINE__,
			rc, cpu, ri, escrmsr, escrvalue, escrtbits));
	}

	/* Enable the correct bits for this CPU. */
	escrvalue |= escrtbits;
	cccrvalue |= cccrtbits | P4_CCCR_ENABLE;

	/* Save HW value at the time of starting hardware */
	P4_PCPU_HW_VALUE(pc,ri,cpu) = rdmsr(pd->pm_pmc_msr);

	/* Program the ESCR and CCCR and start the PMC */
	wrmsr(escrmsr, escrvalue);
	wrmsr(pd->pm_cccr_msr, cccrvalue);

	++rc;
	P4_PCPU_SET_RUNCOUNT(pc,ri,rc);

	mtx_unlock_spin(&pc->pc_mtx);

	PMCDBG(MDP,STA,2,"p4-start cpu=%d rc=%d ri=%d escr=%d "
	    "escrmsr=0x%x escrvalue=0x%x cccr_config=0x%x v=%jx", cpu, rc,
	    ri, pm->pm_md.pm_p4.pm_p4_escr, escrmsr, escrvalue,
	    cccrvalue, P4_PCPU_HW_VALUE(pc,ri,cpu));

	return (0);
}

/*
 * Stop a PMC.
 */

static int
p4_stop_pmc(int cpu, int ri)
{
	int rc;
	uint32_t cccrvalue, cccrtbits, escrvalue, escrmsr, escrtbits;
	struct pmc *pm;
	struct p4_cpu *pc;
	struct p4pmc_descr *pd;
	pmc_value_t tmp;

	KASSERT(cpu >= 0 && cpu < pmc_cpu_max(),
	    ("[p4,%d] illegal CPU value %d", __LINE__, cpu));
	KASSERT(ri >= 0 && ri < P4_NPMCS,
	    ("[p4,%d] illegal row index %d", __LINE__, ri));

	pd = &p4_pmcdesc[ri];
	pc = p4_pcpu[P4_TO_HTT_PRIMARY(cpu)];
	pm = pc->pc_p4pmcs[ri].phw_pmc;

	KASSERT(pm != NULL,
	    ("[p4,%d] null pmc for cpu%d, ri%d", __LINE__, cpu, ri));

	PMCDBG(MDP,STO,1, "p4-stop cpu=%d ri=%d", cpu, ri);

	if (PMC_IS_SYSTEM_MODE(PMC_TO_MODE(pm))) {
		wrmsr(pd->pm_cccr_msr,
		    pm->pm_md.pm_p4.pm_p4_cccrvalue & ~P4_CCCR_ENABLE);
		return (0);
	}

	/*
	 * Thread mode PMCs.
	 *
	 * On HTT machines, this PMC may be in use by two threads
	 * running on two logical CPUS.  Thus we look at the
	 * 'runcount' field and only turn off the appropriate TO/T1
	 * bits (and keep the PMC running) if two logical CPUs were
	 * using the PMC.
	 *
	 */

	/* bits to mask */
	cccrtbits = P4_CCCR_OVF_PMI_T0;
	escrtbits = P4_ESCR_T0_OS | P4_ESCR_T0_USR;
	if (P4_CPU_IS_HTT_SECONDARY(cpu)) {
		cccrtbits <<= 1;
		escrtbits >>= 2;
	}

	mtx_lock_spin(&pc->pc_mtx);

	rc = P4_PCPU_GET_RUNCOUNT(pc,ri);

	KASSERT(rc == 2 || rc == 1,
	    ("[p4,%d] illegal runcount cpu=%d ri=%d rc=%d", __LINE__, cpu, ri,
		rc));

	--rc;

	P4_PCPU_SET_RUNCOUNT(pc,ri,rc);

	/* Stop this PMC */
	cccrvalue = rdmsr(pd->pm_cccr_msr);
	wrmsr(pd->pm_cccr_msr, cccrvalue & ~P4_CCCR_ENABLE);

	escrmsr   = pm->pm_md.pm_p4.pm_p4_escrmsr;
	escrvalue = rdmsr(escrmsr);

	/* The current CPU should be running on this PMC */
	KASSERT(escrvalue & escrtbits,
	    ("[p4,%d] ESCR T0/T1 mismatch cpu=%d rc=%d ri=%d escrmsr=0x%x "
		"escrvalue=0x%x tbits=0x%x", __LINE__, cpu, rc, ri, escrmsr,
		escrvalue, escrtbits));
	KASSERT(PMC_IS_COUNTING_MODE(PMC_TO_MODE(pm)) ||
	    (cccrvalue & cccrtbits),
	    ("[p4,%d] CCCR T0/T1 mismatch cpu=%d ri=%d cccrvalue=0x%x "
		"tbits=0x%x", __LINE__, cpu, ri, cccrvalue, cccrtbits));

	/* get the current hardware reading */
	tmp = rdmsr(pd->pm_pmc_msr);

	if (rc == 1) {		/* need to keep the PMC running */
		escrvalue &= ~escrtbits;
		cccrvalue &= ~cccrtbits;
		wrmsr(escrmsr, escrvalue);
		wrmsr(pd->pm_cccr_msr, cccrvalue);
	}

	mtx_unlock_spin(&pc->pc_mtx);

	PMCDBG(MDP,STO,2, "p4-stop cpu=%d rc=%d ri=%d escrmsr=0x%x "
	    "escrval=0x%x cccrval=0x%x v=%jx", cpu, rc, ri, escrmsr,
	    escrvalue, cccrvalue, tmp);

	if (tmp < P4_PCPU_HW_VALUE(pc,ri,cpu)) /* 40 bit counter overflow */
		tmp += (P4_PERFCTR_MASK + 1) - P4_PCPU_HW_VALUE(pc,ri,cpu);
	else
		tmp -= P4_PCPU_HW_VALUE(pc,ri,cpu);

	P4_PCPU_PMC_VALUE(pc,ri,cpu) += tmp;

	return 0;
}

/*
 * Handle an interrupt.
 *
 * The hardware sets the CCCR_OVF whenever a counter overflow occurs,
 * so the handler examines all the 18 CCCR registers, processing the
 * counters that have overflowed.
 *
 * On HTT machines, the CCCR register is shared and will interrupt
 * both logical processors if so configured.  Thus multiple logical
 * CPUs could enter the NMI service routine at the same time.  These
 * will get serialized using a per-cpu spinlock dedicated for use in
 * the NMI handler.
 */

static int
p4_intr(int cpu, struct trapframe *tf)
{
	uint32_t cccrval, ovf_mask, ovf_partner;
	int did_interrupt, error, ri;
	struct p4_cpu *pc;
	struct pmc *pm;
	pmc_value_t v;

	PMCDBG(MDP,INT, 1, "cpu=%d tf=0x%p um=%d", cpu, (void *) tf,
	    TRAPF_USERMODE(tf));

	pc = p4_pcpu[P4_TO_HTT_PRIMARY(cpu)];

	ovf_mask = P4_CPU_IS_HTT_SECONDARY(cpu) ?
	    P4_CCCR_OVF_PMI_T1 : P4_CCCR_OVF_PMI_T0;
	ovf_mask |= P4_CCCR_OVF;
	if (p4_system_has_htt)
		ovf_partner = P4_CPU_IS_HTT_SECONDARY(cpu) ?
		    P4_CCCR_OVF_PMI_T0 : P4_CCCR_OVF_PMI_T1;
	else
		ovf_partner = 0;
	did_interrupt = 0;

	if (p4_system_has_htt)
		P4_PCPU_ACQ_INTR_SPINLOCK(pc);

	/*
	 * Loop through all CCCRs, looking for ones that have
	 * interrupted this CPU.
	 */
	for (ri = 0; ri < P4_NPMCS; ri++) {

		/*
		 * Check if our partner logical CPU has already marked
		 * this PMC has having interrupted it.  If so, reset
		 * the flag and process the interrupt, but leave the
		 * hardware alone.
		 */
		if (p4_system_has_htt && P4_PCPU_GET_INTRFLAG(pc,ri)) {
			P4_PCPU_SET_INTRFLAG(pc,ri,0);
			did_interrupt = 1;

			/*
			 * Ignore de-configured or stopped PMCs.
			 * Ignore PMCs not in sampling mode.
			 */
			pm = pc->pc_p4pmcs[ri].phw_pmc;
			if (pm == NULL ||
			    pm->pm_state != PMC_STATE_RUNNING ||
			    !PMC_IS_SAMPLING_MODE(PMC_TO_MODE(pm))) {
				continue;
			}
			(void) pmc_process_interrupt(cpu, pm, tf,
			    TRAPF_USERMODE(tf));
			continue;
		}

		/*
		 * Fresh interrupt.  Look for the CCCR_OVF bit
		 * and the OVF_Tx bit for this logical
		 * processor being set.
		 */
		cccrval = rdmsr(P4_CCCR_MSR_FIRST + ri);

		if ((cccrval & ovf_mask) != ovf_mask)
			continue;

		/*
		 * If the other logical CPU would also have been
		 * interrupted due to the PMC being shared, record
		 * this fact in the per-cpu saved interrupt flag
		 * bitmask.
		 */
		if (p4_system_has_htt && (cccrval & ovf_partner))
			P4_PCPU_SET_INTRFLAG(pc, ri, 1);

		v = rdmsr(P4_PERFCTR_MSR_FIRST + ri);

		PMCDBG(MDP,INT, 2, "ri=%d v=%jx", ri, v);

		/* Stop the counter, and reset the overflow  bit */
		cccrval &= ~(P4_CCCR_OVF | P4_CCCR_ENABLE);
		wrmsr(P4_CCCR_MSR_FIRST + ri, cccrval);

		did_interrupt = 1;

		/*
		 * Ignore de-configured or stopped PMCs.  Ignore PMCs
		 * not in sampling mode.
		 */
		pm = pc->pc_p4pmcs[ri].phw_pmc;

		if (pm == NULL ||
		    pm->pm_state != PMC_STATE_RUNNING ||
		    !PMC_IS_SAMPLING_MODE(PMC_TO_MODE(pm))) {
			continue;
		}

		/*
		 * Process the interrupt.  Re-enable the PMC if
		 * processing was successful.
		 */
		error = pmc_process_interrupt(cpu, pm, tf,
		    TRAPF_USERMODE(tf));

		/*
		 * Only the first processor executing the NMI handler
		 * in a HTT pair will restart a PMC, and that too
		 * only if there were no errors.
		 */
		v = P4_RELOAD_COUNT_TO_PERFCTR_VALUE(
			pm->pm_sc.pm_reloadcount);
		wrmsr(P4_PERFCTR_MSR_FIRST + ri, v);
		if (error == 0)
			wrmsr(P4_CCCR_MSR_FIRST + ri,
			    cccrval | P4_CCCR_ENABLE);
	}

	/* allow the other CPU to proceed */
	if (p4_system_has_htt)
		P4_PCPU_REL_INTR_SPINLOCK(pc);

	/*
	 * On Intel P4 CPUs, the PMC 'pcint' entry in the LAPIC gets
	 * masked when a PMC interrupts the CPU.  We need to unmask
	 * the interrupt source explicitly.
	 */

	if (did_interrupt)
		lapic_reenable_pmc();

	atomic_add_int(did_interrupt ? &pmc_stats.pm_intr_processed :
	    &pmc_stats.pm_intr_ignored, 1);

	return (did_interrupt);
}

/*
 * Describe a CPU's PMC state.
 */

static int
p4_describe(int cpu, int ri, struct pmc_info *pi,
    struct pmc **ppmc)
{
	int error;
	size_t copied;
	const struct p4pmc_descr *pd;

	KASSERT(cpu >= 0 && cpu < pmc_cpu_max(),
	    ("[p4,%d] illegal CPU %d", __LINE__, cpu));
	KASSERT(ri >= 0 && ri < P4_NPMCS,
	    ("[p4,%d] row-index %d out of range", __LINE__, ri));

	PMCDBG(MDP,OPS,1,"p4-describe cpu=%d ri=%d", cpu, ri);

	if (P4_CPU_IS_HTT_SECONDARY(cpu))
		return (EINVAL);

	pd  = &p4_pmcdesc[ri];

	if ((error = copystr(pd->pm_descr.pd_name, pi->pm_name,
	    PMC_NAME_MAX, &copied)) != 0)
		return (error);

	pi->pm_class = pd->pm_descr.pd_class;

	if (p4_pcpu[cpu]->pc_p4pmcs[ri].phw_state & PMC_PHW_FLAG_IS_ENABLED) {
		pi->pm_enabled = TRUE;
		*ppmc          = p4_pcpu[cpu]->pc_p4pmcs[ri].phw_pmc;
	} else {
		pi->pm_enabled = FALSE;
		*ppmc          = NULL;
	}

	return (0);
}

/*
 * Get MSR# for use with RDPMC.
 */

static int
p4_get_msr(int ri, uint32_t *msr)
{
	KASSERT(ri >= 0 && ri < P4_NPMCS,
	    ("[p4,%d] ri %d out of range", __LINE__, ri));

	*msr = p4_pmcdesc[ri].pm_pmc_msr - P4_PERFCTR_MSR_FIRST;

	PMCDBG(MDP,OPS, 1, "ri=%d getmsr=0x%x", ri, *msr);

	return 0;
}


int
pmc_p4_initialize(struct pmc_mdep *md, int ncpus)
{
	struct pmc_classdep *pcd;
	struct p4_event_descr *pe;

	KASSERT(md != NULL, ("[p4,%d] md is NULL", __LINE__));
	KASSERT(cpu_vendor_id == CPU_VENDOR_INTEL,
	    ("[p4,%d] Initializing non-intel processor", __LINE__));

	PMCDBG(MDP,INI,1, "%s", "p4-initialize");

	/* Allocate space for pointers to per-cpu descriptors. */
	p4_pcpu = malloc(sizeof(struct p4_cpu **) * ncpus, M_PMC,
	    M_ZERO|M_WAITOK);

	/* Fill in the class dependent descriptor. */
	pcd = &md->pmd_classdep[PMC_MDEP_CLASS_INDEX_P4];

	switch (md->pmd_cputype) {
	case PMC_CPU_INTEL_PIV:

		pcd->pcd_caps		= P4_PMC_CAPS;
		pcd->pcd_class		= PMC_CLASS_P4;
		pcd->pcd_num		= P4_NPMCS;
		pcd->pcd_ri		= md->pmd_npmc;
		pcd->pcd_width		= 40;

		pcd->pcd_allocate_pmc	= p4_allocate_pmc;
		pcd->pcd_config_pmc	= p4_config_pmc;
		pcd->pcd_describe	= p4_describe;
		pcd->pcd_get_config	= p4_get_config;
		pcd->pcd_get_msr	= p4_get_msr;
		pcd->pcd_pcpu_fini 	= p4_pcpu_fini;
		pcd->pcd_pcpu_init    	= p4_pcpu_init;
		pcd->pcd_read_pmc	= p4_read_pmc;
		pcd->pcd_release_pmc	= p4_release_pmc;
		pcd->pcd_start_pmc	= p4_start_pmc;
		pcd->pcd_stop_pmc	= p4_stop_pmc;
		pcd->pcd_write_pmc	= p4_write_pmc;

		md->pmd_pcpu_fini	= NULL;
		md->pmd_pcpu_init	= NULL;
		md->pmd_intr	    	= p4_intr;
		md->pmd_npmc	       += P4_NPMCS;

		/* model specific configuration */
		if ((cpu_id & 0xFFF) < 0xF27) {

			/*
			 * On P4 and Xeon with CPUID < (Family 15,
			 * Model 2, Stepping 7), only one ESCR is
			 * available for the IOQ_ALLOCATION event.
			 */

			pe = p4_find_event(PMC_EV_P4_IOQ_ALLOCATION);
			pe->pm_escrs[1] = P4_ESCR_NONE;
		}

		break;

	default:
		KASSERT(0,("[p4,%d] Unknown CPU type", __LINE__));
		return ENOSYS;
	}

	return (0);
}

void
pmc_p4_finalize(struct pmc_mdep *md)
{
#if	defined(INVARIANTS)
	int i, ncpus;
#endif

	KASSERT(p4_pcpu != NULL,
	    ("[p4,%d] NULL p4_pcpu", __LINE__));

#if	defined(INVARIANTS)
	ncpus = pmc_cpu_max();
	for (i = 0; i < ncpus; i++)
		KASSERT(p4_pcpu[i] == NULL, ("[p4,%d] non-null pcpu %d",
		    __LINE__, i));
#endif

	free(p4_pcpu, M_PMC);
	p4_pcpu = NULL;
}
