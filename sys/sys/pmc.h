/*-
 * Copyright (c) 2003, Joseph Koshy
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

#ifndef _SYS_PMC_H_
#define	_SYS_PMC_H_

#define	PMC_MODULE_NAME		"hwpmc"
#define	PMC_NAME_MAX		16 /* HW counter name size */
#define	PMC_CLASS_MAX		4  /* #classes of PMCs in a CPU */

/* Kernel<->userland API version number [MMmmpppp] */

#define	PMC_VERSION_MAJOR	0x01
#define	PMC_VERSION_MINOR	0x01
#define	PMC_VERSION_PATCH	0x0001

#define	PMC_VERSION		(PMC_VERSION_MAJOR << 24 |		\
	PMC_VERSION_MINOR << 16 | PMC_VERSION_PATCH)

/*
 * Kinds of CPUs known
 */

#define	__PMC_CPUS()					\
	__PMC_CPU(AMD_K7,     "AMD K7")			\
	__PMC_CPU(AMD_K8,     "AMD K8")			\
	__PMC_CPU(INTEL_P5,   "Intel Pentium")		\
	__PMC_CPU(INTEL_P6,   "Intel Pentium Pro")	\
	__PMC_CPU(INTEL_CL,   "Intel Celeron")		\
	__PMC_CPU(INTEL_PII,  "Intel Pentium II")	\
	__PMC_CPU(INTEL_PIII, "Intel Pentium III")	\
	__PMC_CPU(INTEL_PM,   "Intel Pentium M")	\
	__PMC_CPU(INTEL_PIV,  "Intel Pentium IV")

enum pmc_cputype {
#undef	__PMC_CPU
#define	__PMC_CPU(S,D)	PMC_CPU_##S ,
	__PMC_CPUS()
};

#define	PMC_CPU_FIRST	PMC_CPU_AMD_K7
#define	PMC_CPU_LAST	PMC_CPU_INTEL_PIV

/*
 * Classes of PMCs
 */

#define	__PMC_CLASSES()							\
	__PMC_CLASS(TSC)	/* CPU Timestamp counter */		\
	__PMC_CLASS(K7)		/* AMD K7 performance counters */	\
	__PMC_CLASS(K8)		/* AMD K8 performance counters */	\
	__PMC_CLASS(P5)		/* Intel Pentium counters */		\
	__PMC_CLASS(P6)		/* Intel Pentium Pro counters */	\
	__PMC_CLASS(P4)		/* Intel Pentium-IV counters */

enum pmc_class {
#undef  __PMC_CLASS
#define	__PMC_CLASS(N)	PMC_CLASS_##N ,
	__PMC_CLASSES()
};

#define	PMC_CLASS_FIRST	PMC_CLASS_TSC
#define	PMC_CLASS_LAST	PMC_CLASS_P4

/*
 * A PMC can be in the following states:
 *
 * Hardware states:
 *   DISABLED   -- administratively prohibited from being used.
 *   FREE       -- HW available for use
 * Software states:
 *   ALLOCATED  -- allocated
 *   STOPPED    -- allocated, but not counting events
 *   RUNNING    -- allocated, and in operation; 'pm_runcount'
 *                 holds the number of CPUs using this PMC at
 *                 a given instant
 *   DELETED    -- being destroyed
 */

#define	__PMC_HWSTATES()			\
	__PMC_STATE(DISABLED)			\
	__PMC_STATE(FREE)

#define	__PMC_SWSTATES()			\
	__PMC_STATE(ALLOCATED)			\
	__PMC_STATE(STOPPED)			\
	__PMC_STATE(RUNNING)			\
	__PMC_STATE(DELETED)

#define	__PMC_STATES()				\
	__PMC_HWSTATES()			\
	__PMC_SWSTATES()

enum pmc_state {
#undef	__PMC_STATE
#define	__PMC_STATE(S)	PMC_STATE_##S,
	__PMC_STATES()
	__PMC_STATE(MAX)
};

#define	PMC_STATE_FIRST	PMC_STATE_DISABLED
#define	PMC_STATE_LAST	PMC_STATE_DELETED

/*
 * An allocated PMC may used as a 'global' counter or as a
 * 'thread-private' one.  Each such mode of use can be in either
 * statistical sampling mode or in counting mode.  Thus a PMC in use
 *
 * SS i.e., SYSTEM STATISTICAL  -- system-wide statistical profiling
 * SC i.e., SYSTEM COUNTER      -- system-wide counting mode
 * TS i.e., THREAD STATISTICAL  -- thread virtual, statistical profiling
 * TC i.e., THREAD COUNTER      -- thread virtual, counting mode
 *
 * Statistical profiling modes rely on the PMC periodically delivering
 * a interrupt to the CPU (when the configured number of events have
 * been measured), so the PMC must have the ability to generate
 * interrupts.
 *
 * In counting modes, the PMC counts its configured events, with the
 * value of the PMC being read whenever needed by its owner process.
 *
 * The thread specific modes "virtualize" the PMCs -- the PMCs appear
 * to be thread private and count events only when the profiled thread
 * actually executes on the CPU.
 *
 * The system-wide "global" modes keep the PMCs running all the time
 * and are used to measure the behaviour of the whole system.
 */

#define	__PMC_MODES()				\
	__PMC_MODE(SS,	0)			\
	__PMC_MODE(SC,	1)			\
	__PMC_MODE(TS,	2)			\
	__PMC_MODE(TC,	3)

enum pmc_mode {
#undef	__PMC_MODE
#define	__PMC_MODE(M,N)	PMC_MODE_##M = N,
	__PMC_MODES()
};

#define	PMC_MODE_FIRST	PMC_MODE_SS
#define	PMC_MODE_LAST	PMC_MODE_TC

#define	PMC_IS_COUNTING_MODE(mode)				\
	((mode) == PMC_MODE_SC || (mode) == PMC_MODE_TC)
#define	PMC_IS_SYSTEM_MODE(mode)				\
	((mode) == PMC_MODE_SS || (mode) == PMC_MODE_SC)
#define	PMC_IS_SAMPLING_MODE(mode)				\
	((mode) == PMC_MODE_SS || (mode) == PMC_MODE_TS)
#define	PMC_IS_VIRTUAL_MODE(mode)				\
	((mode) == PMC_MODE_TS || (mode) == PMC_MODE_TC)

/*
 * PMC row disposition
 */

#define	__PMC_DISPOSITIONS(N)					\
	__PMC_DISP(STANDALONE)	/* global/disabled counters */	\
	__PMC_DISP(FREE)	/* free/available */		\
	__PMC_DISP(THREAD)	/* thread-virtual PMCs */	\
	__PMC_DISP(UNKNOWN)	/* sentinel */

enum pmc_disp {
#undef	__PMC_DISP
#define	__PMC_DISP(D)	PMC_DISP_##D ,
	__PMC_DISPOSITIONS()
};

#define	PMC_DISP_FIRST	PMC_DISP_STANDALONE
#define	PMC_DISP_LAST	PMC_DISP_THREAD

/*
 * PMC event codes
 *
 * __PMC_EV(CLASS, SYMBOLIC-NAME, VALUE, READABLE-NAME)
 */

/*
 * AMD K7 Events, from "The AMD Athlon(tm) Processor x86 Code
 * Optimization Guide" [Doc#22007K, Feb 2002]
 */

#define	__PMC_EV_K7()							\
__PMC_EV(K7, DC_ACCESSES,		k7-dc-accesses)			\
__PMC_EV(K7, DC_MISSES,			k7-dc-misses)			\
__PMC_EV(K7, DC_REFILLS_FROM_L2,	k7-dc-refills-from-l2)		\
__PMC_EV(K7, DC_REFILLS_FROM_SYSTEM,	k7-dc-refills-from-system)	\
__PMC_EV(K7, DC_WRITEBACKS,		k7-dc-writebacks)		\
__PMC_EV(K7, L1_DTLB_MISS_AND_L2_DTLB_HITS,				\
			k7-l1-dtlb-miss-and-l2-dtlb-hits)		\
__PMC_EV(K7, L1_AND_L2_DTLB_MISSES,	k7-l1-and-l2-dtlb-misses)	\
__PMC_EV(K7, MISALIGNED_REFERENCES,	k7-misaligned-references)	\
__PMC_EV(K7, IC_FETCHES,		k7-ic-fetches)			\
__PMC_EV(K7, IC_MISSES,			k7-ic-misses)			\
__PMC_EV(K7, L1_ITLB_MISSES,		k7-l1-itlb-misses)		\
__PMC_EV(K7, L1_L2_ITLB_MISSES,		k7-l1-l2-itlb-misses)		\
__PMC_EV(K7, RETIRED_INSTRUCTIONS,	k7-retired-instructions)	\
__PMC_EV(K7, RETIRED_OPS,		k7-retired-ops)			\
__PMC_EV(K7, RETIRED_BRANCHES,		k7-retired-branches)		\
__PMC_EV(K7, RETIRED_BRANCHES_MISPREDICTED,				\
			k7-retired-branches-mispredicted)		\
__PMC_EV(K7, RETIRED_TAKEN_BRANCHES,	k7-retired-taken-branches)	\
__PMC_EV(K7, RETIRED_TAKEN_BRANCHES_MISPREDICTED,			\
			k7-retired-taken-branches-mispredicted)		\
__PMC_EV(K7, RETIRED_FAR_CONTROL_TRANSFERS,				\
			k7-retired-far-control-transfers)		\
__PMC_EV(K7, RETIRED_RESYNC_BRANCHES, k7-retired-resync-branches)	\
__PMC_EV(K7, INTERRUPTS_MASKED_CYCLES, k7-interrupts-masked-cycles)	\
__PMC_EV(K7, INTERRUPTS_MASKED_WHILE_PENDING_CYCLES,			\
			k7-interrupts-masked-while-pending-cycles)	\
__PMC_EV(K7, HARDWARE_INTERRUPTS,	k7-hardware-interrupts)

#define	PMC_EV_K7_FIRST	PMC_EV_K7_DC_ACCESSES
#define	PMC_EV_K7_LAST	PMC_EV_K7_HARDWARE_INTERRUPTS

/*
 * Intel P4 Events, from "IA-32 Intel(r) Architecture Software
 * Developer's Manual, Volume 3: System Programming Guide" [245472-012]
 */

#define	__PMC_EV_P4()							\
__PMC_EV(P4, TC_DELIVER_MODE,		p4-tc-deliver-mode)		\
__PMC_EV(P4, BPU_FETCH_REQUEST,		p4-bpu-fetch-request)		\
__PMC_EV(P4, ITLB_REFERENCE,		p4-itlb-reference)		\
__PMC_EV(P4, MEMORY_CANCEL,		p4-memory-cancel)		\
__PMC_EV(P4, MEMORY_COMPLETE,		p4-memory-complete)		\
__PMC_EV(P4, LOAD_PORT_REPLAY,		p4-load-port-replay)		\
__PMC_EV(P4, STORE_PORT_REPLAY,		p4-store-port-replay)		\
__PMC_EV(P4, MOB_LOAD_REPLAY,		p4-mob-load-replay)		\
__PMC_EV(P4, PAGE_WALK_TYPE,		p4-page-walk-type)		\
__PMC_EV(P4, BSQ_CACHE_REFERENCE,	p4-bsq-cache-reference)		\
__PMC_EV(P4, IOQ_ALLOCATION,		p4-ioq-allocation)		\
__PMC_EV(P4, IOQ_ACTIVE_ENTRIES,	p4-ioq-active-entries)		\
__PMC_EV(P4, FSB_DATA_ACTIVITY,		p4-fsb-data-activity)		\
__PMC_EV(P4, BSQ_ALLOCATION,		p4-bsq-allocation)		\
__PMC_EV(P4, BSQ_ACTIVE_ENTRIES,	p4-bsq-active-entries)		\
__PMC_EV(P4, SSE_INPUT_ASSIST,		p4-sse-input-assist)		\
__PMC_EV(P4, PACKED_SP_UOP,		p4-packed-sp-uop)		\
__PMC_EV(P4, PACKED_DP_UOP,		p4-packed-dp-uop)		\
__PMC_EV(P4, SCALAR_SP_UOP,		p4-scalar-sp-uop)		\
__PMC_EV(P4, SCALAR_DP_UOP,		p4-scalar-dp-uop)		\
__PMC_EV(P4, 64BIT_MMX_UOP,		p4-64bit-mmx-uop)		\
__PMC_EV(P4, 128BIT_MMX_UOP,		p4-128bit-mmx-uop)		\
__PMC_EV(P4, X87_FP_UOP,		p4-x87-fp-uop)			\
__PMC_EV(P4, X87_SIMD_MOVES_UOP,	p4-x87-simd-moves-uop)		\
__PMC_EV(P4, GLOBAL_POWER_EVENTS,	p4-global-power-events)		\
__PMC_EV(P4, TC_MS_XFER,		p4-tc-ms-xfer)			\
__PMC_EV(P4, UOP_QUEUE_WRITES,		p4-uop-queue-writes)		\
__PMC_EV(P4, RETIRED_MISPRED_BRANCH_TYPE,				\
			    p4-retired-mispred-branch-type)		\
__PMC_EV(P4, RETIRED_BRANCH_TYPE,	p4-retired-branch-type)		\
__PMC_EV(P4, RESOURCE_STALL,		p4-resource-stall)		\
__PMC_EV(P4, WC_BUFFER,			p4-wc-buffer)			\
__PMC_EV(P4, B2B_CYCLES,		p4-b2b-cycles)			\
__PMC_EV(P4, BNR,			p4-bnr)				\
__PMC_EV(P4, SNOOP,			p4-snoop)			\
__PMC_EV(P4, RESPONSE,			p4-response)			\
__PMC_EV(P4, FRONT_END_EVENT,		p4-front-end-event)		\
__PMC_EV(P4, EXECUTION_EVENT,		p4-execution-event)		\
__PMC_EV(P4, REPLAY_EVENT,		p4-replay-event)		\
__PMC_EV(P4, INSTR_RETIRED,		p4-instr-retired)		\
__PMC_EV(P4, UOPS_RETIRED,		p4-uops-retired)		\
__PMC_EV(P4, UOP_TYPE,			p4-uop-type)			\
__PMC_EV(P4, BRANCH_RETIRED,		p4-branch-retired)		\
__PMC_EV(P4, MISPRED_BRANCH_RETIRED,	p4-mispred-branch-retired)	\
__PMC_EV(P4, X87_ASSIST,		p4-x87-assist)			\
__PMC_EV(P4, MACHINE_CLEAR,		p4-machine-clear)

#define	PMC_EV_P4_FIRST PMC_EV_P4_TC_DELIVER_MODE
#define	PMC_EV_P4_LAST	PMC_EV_P4_MACHINE_CLEAR

/* Intel Pentium Pro, P-II, P-III and Pentium-M style events */

#define	__PMC_EV_P6()							\
__PMC_EV(P6, DATA_MEM_REFS,		p6-data-mem-refs)		\
__PMC_EV(P6, DCU_LINES_IN,		p6-dcu-lines-in)		\
__PMC_EV(P6, DCU_M_LINES_IN,		p6-dcu-m-lines-in)		\
__PMC_EV(P6, DCU_M_LINES_OUT,		p6-dcu-m-lines-out)		\
__PMC_EV(P6, DCU_MISS_OUTSTANDING,	p6-dcu-miss-outstanding)	\
__PMC_EV(P6, IFU_FETCH,			p6-ifu-fetch)			\
__PMC_EV(P6, IFU_FETCH_MISS,		p6-ifu-fetch-miss)		\
__PMC_EV(P6, ITLB_MISS,			p6-itlb-miss)			\
__PMC_EV(P6, IFU_MEM_STALL,		p6-ifu-mem-stall)		\
__PMC_EV(P6, ILD_STALL,			p6-ild-stall)			\
__PMC_EV(P6, L2_IFETCH,			p6-l2-ifetch)			\
__PMC_EV(P6, L2_LD,			p6-l2-ld)			\
__PMC_EV(P6, L2_ST,			p6-l2-st)			\
__PMC_EV(P6, L2_LINES_IN,		p6-l2-lines-in)			\
__PMC_EV(P6, L2_LINES_OUT,		p6-l2-lines-out)		\
__PMC_EV(P6, L2_M_LINES_INM,		p6-l2-m-lines-inm)		\
__PMC_EV(P6, L2_M_LINES_OUTM,		p6-l2-m-lines-outm)		\
__PMC_EV(P6, L2_RQSTS,			p6-l2-rqsts)			\
__PMC_EV(P6, L2_ADS,			p6-l2-ads)			\
__PMC_EV(P6, L2_DBUS_BUSY,		p6-l2-dbus-busy)		\
__PMC_EV(P6, L2_DBUS_BUSY_RD,		p6-l2-dbus-busy-rd)		\
__PMC_EV(P6, BUS_DRDY_CLOCKS,		p6-bus-drdy-clocks)		\
__PMC_EV(P6, BUS_LOCK_CLOCKS,		p6-bus-lock-clocks)		\
__PMC_EV(P6, BUS_REQ_OUTSTANDING,	p6-bus-req-outstanding)		\
__PMC_EV(P6, BUS_TRAN_BRD,		p6-bus-tran-brd)		\
__PMC_EV(P6, BUS_TRAN_RFO,		p6-bus-tran-rfo)		\
__PMC_EV(P6, BUS_TRANS_WB,		p6-bus-trans-wb)		\
__PMC_EV(P6, BUS_TRAN_IFETCH,		p6-bus-tran-ifetch)		\
__PMC_EV(P6, BUS_TRAN_INVAL,		p6-bus-tran-inval)		\
__PMC_EV(P6, BUS_TRAN_PWR,		p6-bus-tran-pwr)		\
__PMC_EV(P6, BUS_TRANS_P,		p6-bus-trans-p)			\
__PMC_EV(P6, BUS_TRANS_IO,		p6-bus-trans-io)		\
__PMC_EV(P6, BUS_TRAN_DEF,		p6-bus-tran-def)		\
__PMC_EV(P6, BUS_TRAN_BURST,		p6-bus-tran-burst)		\
__PMC_EV(P6, BUS_TRAN_ANY,		p6-bus-tran-any)		\
__PMC_EV(P6, BUS_TRAN_MEM,		p6-bus-tran-mem)		\
__PMC_EV(P6, BUS_DATA_RCV,		p6-bus-data-rcv)		\
__PMC_EV(P6, BUS_BNR_DRV,		p6-bus-bnr-drv)			\
__PMC_EV(P6, BUS_HIT_DRV,		p6-bus-hit-drv)			\
__PMC_EV(P6, BUS_HITM_DRV,		p6-bus-hitm-drv)		\
__PMC_EV(P6, BUS_SNOOP_STALL,		p6-bus-snoop-stall)		\
__PMC_EV(P6, FLOPS,			p6-flops)			\
__PMC_EV(P6, FP_COMPS_OPS_EXE,		p6-fp-comps-ops-exe)		\
__PMC_EV(P6, FP_ASSIST,			p6-fp-assist)			\
__PMC_EV(P6, MUL,			p6-mul)				\
__PMC_EV(P6, DIV,			p6-div)				\
__PMC_EV(P6, CYCLES_DIV_BUSY,		p6-cycles-div-busy)		\
__PMC_EV(P6, LD_BLOCKS,			p6-ld-blocks)			\
__PMC_EV(P6, SB_DRAINS,			p6-sb-drains)			\
__PMC_EV(P6, MISALIGN_MEM_REF,		p6-misalign-mem-ref)		\
__PMC_EV(P6, EMON_KNI_PREF_DISPATCHED,	p6-emon-kni-pref-dispatched)	\
__PMC_EV(P6, EMON_KNI_PREF_MISS,	p6-emon-kni-pref-miss)		\
__PMC_EV(P6, INST_RETIRED,		p6-inst-retired)		\
__PMC_EV(P6, UOPS_RETIRED,		p6-uops-retired)		\
__PMC_EV(P6, INST_DECODED,		p6-inst-decoded)		\
__PMC_EV(P6, EMON_KNI_INST_RETIRED,	p6-emon-kni-inst-retired)	\
__PMC_EV(P6, EMON_KNI_COMP_INST_RET,	p6-emon-kni-comp-inst-ret)	\
__PMC_EV(P6, HW_INT_RX,			p6-hw-int-rx)			\
__PMC_EV(P6, CYCLES_INT_MASKED,		p6-cycles-int-masked)		\
__PMC_EV(P6, CYCLES_INT_PENDING_AND_MASKED,				\
			    p6-cycles-in-pending-and-masked)		\
__PMC_EV(P6, BR_INST_RETIRED,		p6-br-inst-retired)		\
__PMC_EV(P6, BR_MISS_PRED_RETIRED,	p6-br-miss-pred-retired)	\
__PMC_EV(P6, BR_TAKEN_RETIRED,		p6-br-taken-retired)		\
__PMC_EV(P6, BR_MISS_PRED_TAKEN_RET,	p6-br-miss-pred-taken-ret)	\
__PMC_EV(P6, BR_INST_DECODED,		p6-br-inst-decoded)		\
__PMC_EV(P6, BTB_MISSES,		p6-btb-misses)			\
__PMC_EV(P6, BR_BOGUS,			p6-br-bogus)			\
__PMC_EV(P6, BACLEARS,			p6-baclears)			\
__PMC_EV(P6, RESOURCE_STALLS,		p6-resource-stalls)		\
__PMC_EV(P6, PARTIAL_RAT_STALLS,	p6-partial-rat-stalls)		\
__PMC_EV(P6, SEGMENT_REG_LOADS,		p6-segment-reg-loads)		\
__PMC_EV(P6, CPU_CLK_UNHALTED,		p6-cpu-clk-unhalted)		\
__PMC_EV(P6, MMX_INSTR_EXEC,		p6-mmx-instr-exec)		\
__PMC_EV(P6, MMX_SAT_INSTR_EXEC,	p6-mmx-sat-instr-exec)		\
__PMC_EV(P6, MMX_UOPS_EXEC,		p6-mmx-uops-exec)		\
__PMC_EV(P6, MMX_INSTR_TYPE_EXEC,	p6-mmx-instr-type-exec)		\
__PMC_EV(P6, FP_MMX_TRANS,		p6-fp-mmx-trans)		\
__PMC_EV(P6, MMX_ASSIST,		p6-mmx-assist)			\
__PMC_EV(P6, MMX_INSTR_RET,		p6-mmx-instr-ret)		\
__PMC_EV(P6, SEG_RENAME_STALLS,		p6-seg-rename-stalls)		\
__PMC_EV(P6, SEG_REG_RENAMES,		p6-seg-reg-renames)		\
__PMC_EV(P6, RET_SEG_RENAMES,		p6-ret-seg-renames)		\
__PMC_EV(P6, EMON_EST_TRANS,		p6-emon-est-trans)		\
__PMC_EV(P6, EMON_THERMAL_TRIP,		p6-emon-thermal-trip)		\
__PMC_EV(P6, BR_INST_EXEC,		p6-br-inst-exec)		\
__PMC_EV(P6, BR_MISSP_EXEC,		p6-br-missp-exec)		\
__PMC_EV(P6, BR_BAC_MISSP_EXEC,		p6-br-bac-missp-exec)		\
__PMC_EV(P6, BR_CND_EXEC,		p6-br-cnd-exec)			\
__PMC_EV(P6, BR_CND_MISSP_EXEC,		p6-br-cnd-missp-exec)		\
__PMC_EV(P6, BR_IND_EXEC,		p6-br-ind-exec)			\
__PMC_EV(P6, BR_IND_MISSP_EXEC,		p6-br-ind-missp-exec)		\
__PMC_EV(P6, BR_RET_EXEC,		p6-br-ret-exec)			\
__PMC_EV(P6, BR_RET_MISSP_EXEC,		p6-br-ret-missp-exec)		\
__PMC_EV(P6, BR_RET_BAC_MISSP_EXEC,	p6-br-ret-bac-missp-exec)	\
__PMC_EV(P6, BR_CALL_EXEC,		p6-br-call-exec)		\
__PMC_EV(P6, BR_CALL_MISSP_EXEC,	p6-br-call-missp-exec)		\
__PMC_EV(P6, BR_IND_CALL_EXEC,		p6-br-ind-call-exec)		\
__PMC_EV(P6, EMON_SIMD_INSTR_RETIRED,	p6-emon-simd-instr-retired)	\
__PMC_EV(P6, EMON_SYNCH_UOPS,		p6-emon-synch-uops)		\
__PMC_EV(P6, EMON_ESP_UOPS,		p6-emon-esp-uops)		\
__PMC_EV(P6, EMON_FUSED_UOPS_RET,	p6-emon-fused-uops-ret)		\
__PMC_EV(P6, EMON_UNFUSION,		p6-emon-unfusion)		\
__PMC_EV(P6, EMON_PREF_RQSTS_UP,	p6-emon-pref-rqsts-up)		\
__PMC_EV(P6, EMON_PREF_RQSTS_DN,	p6-emon-pref-rqsts-dn)		\
__PMC_EV(P6, EMON_SSE_SSE2_INST_RETIRED,				\
				p6-emon-sse-sse2-inst-retired)		\
__PMC_EV(P6, EMON_SSE_SSE2_COMP_INST_RETIRED,				\
				p6-emon-sse-sse2-comp-inst-retired)


#define	PMC_EV_P6_FIRST	PMC_EV_P6_DATA_MEM_REFS
#define	PMC_EV_P6_LAST	PMC_EV_P6_EMON_SSE_SSE2_COMP_INST_RETIRED

/* AMD K8 PMCs */

#define	__PMC_EV_K8()							\
__PMC_EV(K8, FP_DISPATCHED_FPU_OPS,	k8-fp-dispatched-fpu-ops)	\
__PMC_EV(K8, FP_CYCLES_WITH_NO_FPU_OPS_RETIRED,				\
		k8-fp-cycles-with-no-fpu-ops-retired)			\
__PMC_EV(K8, FP_DISPATCHED_FPU_FAST_FLAG_OPS,				\
		k8-fp-dispatched-fpu-fast-flag-ops)			\
__PMC_EV(K8, LS_SEGMENT_REGISTER_LOAD,	k8-ls-segment-register-load)	\
__PMC_EV(K8, LS_MICROARCHITECTURAL_RESYNC_BY_SELF_MODIFYING_CODE,	\
		k8-ls-microarchitectural-resync-by-self-modifying-code)	\
__PMC_EV(K8, LS_MICROARCHITECTURAL_RESYNC_BY_SNOOP,			\
		k8-ls-microarchitectural-resync-by-snoop)		\
__PMC_EV(K8, LS_BUFFER2_FULL,		k8-ls-buffer2-full)		\
__PMC_EV(K8, LS_LOCKED_OPERATION,	k8-ls-locked-operation)		\
__PMC_EV(K8, LS_MICROARCHITECTURAL_LATE_CANCEL,				\
		k8-ls-microarchitectural-late-cancel)			\
__PMC_EV(K8, LS_RETIRED_CFLUSH_INSTRUCTIONS,				\
		k8-ls-retired-cflush-instructions)			\
__PMC_EV(K8, LS_RETIRED_CPUID_INSTRUCTIONS,				\
		k8-ls-retired-cpuid-instructions)			\
__PMC_EV(K8, DC_ACCESS,			k8-dc-access)			\
__PMC_EV(K8, DC_MISS,			k8-dc-miss)			\
__PMC_EV(K8, DC_REFILL_FROM_L2,		k8-dc-refill-from-l2)		\
__PMC_EV(K8, DC_REFILL_FROM_SYSTEM,	k8-dc-refill-from-system)	\
__PMC_EV(K8, DC_COPYBACK,		k8-dc-copyback)			\
__PMC_EV(K8, DC_L1_DTLB_MISS_AND_L2_DTLB_HIT,				\
		k8-dc-l1-dtlb-miss-and-l2-dtlb-hit)			\
__PMC_EV(K8, DC_L1_DTLB_MISS_AND_L2_DTLB_MISS,				\
		k8-dc-l1-dtlb-miss-and-l2-dtlb-miss)			\
__PMC_EV(K8, DC_MISALIGNED_DATA_REFERENCE,				\
		k8-dc-misaligned-data-reference)			\
__PMC_EV(K8, DC_MICROARCHITECTURAL_LATE_CANCEL,				\
		k8-dc-microarchitectural-late-cancel-of-an-access)	\
__PMC_EV(K8, DC_MICROARCHITECTURAL_EARLY_CANCEL,			\
		k8-dc-microarchitectural-early-cancel-of-an-access)	\
__PMC_EV(K8, DC_ONE_BIT_ECC_ERROR,	k8-dc-one-bit-ecc-error)	\
__PMC_EV(K8, DC_DISPATCHED_PREFETCH_INSTRUCTIONS,			\
		k8-dc-dispatched-prefetch-instructions)			\
__PMC_EV(K8, DC_DCACHE_ACCESSES_BY_LOCKS,				\
		k8-dc-dcache-accesses-by-locks)				\
__PMC_EV(K8, BU_CPU_CLK_UNHALTED,	k8-bu-cpu-clk-unhalted)		\
__PMC_EV(K8, BU_INTERNAL_L2_REQUEST,	k8-bu-internal-l2-request)	\
__PMC_EV(K8, BU_FILL_REQUEST_L2_MISS,	k8-bu-fill-request-l2-miss)	\
__PMC_EV(K8, BU_FILL_INTO_L2,		k8-bu-fill-into-l2)		\
__PMC_EV(K8, IC_FETCH,			k8-ic-fetch)			\
__PMC_EV(K8, IC_MISS,			k8-ic-miss)			\
__PMC_EV(K8, IC_REFILL_FROM_L2,		k8-ic-refill-from-l2)		\
__PMC_EV(K8, IC_REFILL_FROM_SYSTEM,	k8-ic-refill-from-system)	\
__PMC_EV(K8, IC_L1_ITLB_MISS_AND_L2_ITLB_HIT,				\
		k8-ic-l1-itlb-miss-and-l2-itlb-hit)			\
__PMC_EV(K8, IC_L1_ITLB_MISS_AND_L2_ITLB_MISS,				\
		k8-ic-l1-itlb-miss-and-l2-itlb-miss)			\
__PMC_EV(K8, IC_MICROARCHITECTURAL_RESYNC_BY_SNOOP,			\
		k8-ic-microarchitectural-resync-by-snoop)		\
__PMC_EV(K8, IC_INSTRUCTION_FETCH_STALL,				\
		k8-ic-instruction-fetch-stall)				\
__PMC_EV(K8, IC_RETURN_STACK_HIT,	k8-ic-return-stack-hit)		\
__PMC_EV(K8, IC_RETURN_STACK_OVERFLOW,	k8-ic-return-stack-overflow)	\
__PMC_EV(K8, FR_RETIRED_X86_INSTRUCTIONS,				\
		k8-fr-retired-x86-instructions)				\
__PMC_EV(K8, FR_RETIRED_UOPS,		k8-fr-retired-uops)		\
__PMC_EV(K8, FR_RETIRED_BRANCHES,	k8-fr-retired-branches)		\
__PMC_EV(K8, FR_RETIRED_BRANCHES_MISPREDICTED,				\
		k8-fr-retired-branches-mispredicted)			\
__PMC_EV(K8, FR_RETIRED_TAKEN_BRANCHES,					\
		k8-fr-retired-taken-branches)				\
__PMC_EV(K8, FR_RETIRED_TAKEN_BRANCHES_MISPREDICTED,			\
		k8-fr-retired-taken-branches-mispredicted)		\
__PMC_EV(K8, FR_RETIRED_FAR_CONTROL_TRANSFERS,				\
		k8-fr-retired-far-control-transfers)			\
__PMC_EV(K8, FR_RETIRED_RESYNCS,	k8-fr-retired-resyncs)		\
__PMC_EV(K8, FR_RETIRED_NEAR_RETURNS,	k8-fr-retired-near-returns)	\
__PMC_EV(K8, FR_RETIRED_NEAR_RETURNS_MISPREDICTED,			\
		k8-fr-retired-near-returns-mispredicted)		\
__PMC_EV(K8,								\
	FR_RETIRED_TAKEN_BRANCHES_MISPREDICTED_BY_ADDR_MISCOMPARE,	\
	k8-fr-retired-taken-branches-mispredicted-by-addr-miscompare)	\
__PMC_EV(K8, FR_RETIRED_FPU_INSTRUCTIONS,				\
		k8-fr-retired-fpu-instructions)				\
__PMC_EV(K8, FR_RETIRED_FASTPATH_DOUBLE_OP_INSTRUCTIONS,		\
		k8-fr-retired-fastpath-double-op-instructions)		\
__PMC_EV(K8, FR_INTERRUPTS_MASKED_CYCLES,				\
		k8-fr-interrupts-masked-cycles)				\
__PMC_EV(K8, FR_INTERRUPTS_MASKED_WHILE_PENDING_CYCLES,			\
		k8-fr-interrupts-masked-while-pending-cycles)		\
__PMC_EV(K8, FR_TAKEN_HARDWARE_INTERRUPTS,				\
		k8-fr-taken-hardware-interrupts)			\
__PMC_EV(K8, FR_DECODER_EMPTY,		k8-fr-decoder-empty)		\
__PMC_EV(K8, FR_DISPATCH_STALLS,	k8-fr-dispatch-stalls)		\
__PMC_EV(K8, FR_DISPATCH_STALL_FROM_BRANCH_ABORT_TO_RETIRE,		\
		k8-fr-dispatch-stall-from-branch-abort-to-retire)	\
__PMC_EV(K8, FR_DISPATCH_STALL_FOR_SERIALIZATION,			\
		k8-fr-dispatch-stall-for-serialization)			\
__PMC_EV(K8, FR_DISPATCH_STALL_FOR_SEGMENT_LOAD,			\
		k8-fr-dispatch-stall-for-segment-load)			\
__PMC_EV(K8, FR_DISPATCH_STALL_WHEN_REORDER_BUFFER_IS_FULL,		\
		k8-fr-dispatch-stall-when-reorder-buffer-is-full)	\
__PMC_EV(K8,								\
	FR_DISPATCH_STALL_WHEN_RESERVATION_STATIONS_ARE_FULL,		\
	k8-fr-dispatch-stall-when-reservation-stations-are-full)	\
__PMC_EV(K8, FR_DISPATCH_STALL_WHEN_FPU_IS_FULL,			\
		k8-fr-dispatch-stall-when-fpu-is-full)			\
__PMC_EV(K8, FR_DISPATCH_STALL_WHEN_LS_IS_FULL,				\
		k8-fr-dispatch-stall-when-ls-is-full)			\
__PMC_EV(K8, FR_DISPATCH_STALL_WHEN_WAITING_FOR_ALL_TO_BE_QUIET,	\
		k8-fr-dispatch-stall-when-waiting-for-all-to-be-quiet)	\
__PMC_EV(K8,								\
	FR_DISPATCH_STALL_WHEN_FAR_XFER_OR_RESYNC_BRANCH_PENDING,	\
	k8-fr-dispatch-stall-when-far-xfer-or-resync-branch-pending)	\
__PMC_EV(K8, FR_FPU_EXCEPTIONS,		k8-fr-fpu-exceptions)		\
__PMC_EV(K8, FR_NUMBER_OF_BREAKPOINTS_FOR_DR0,				\
		k8-fr-number-of-breakpoints-for-dr0)			\
__PMC_EV(K8, FR_NUMBER_OF_BREAKPOINTS_FOR_DR1,				\
		k8-fr-number-of-breakpoints-for-dr1)			\
__PMC_EV(K8, FR_NUMBER_OF_BREAKPOINTS_FOR_DR2,				\
		k8-fr-number-of-breakpoints-for-dr2)			\
__PMC_EV(K8, FR_NUMBER_OF_BREAKPOINTS_FOR_DR3,				\
		k8-fr-number-of-breakpoints-for-dr3)			\
__PMC_EV(K8, NB_MEMORY_CONTROLLER_PAGE_ACCESS_EVENT,			\
		k8-nb-memory-controller-page-access-event)		\
__PMC_EV(K8, NB_MEMORY_CONTROLLER_PAGE_TABLE_OVERFLOW,			\
		k8-nb-memory-controller-page-table-overflow)		\
__PMC_EV(K8, NB_MEMORY_CONTROLLER_DRAM_COMMAND_SLOTS_MISSED,		\
		k8-nb-memory-controller-dram-slots-missed)		\
__PMC_EV(K8, NB_MEMORY_CONTROLLER_TURNAROUND,				\
		k8-nb-memory-controller-turnaround)			\
__PMC_EV(K8, NB_MEMORY_CONTROLLER_BYPASS_SATURATION,			\
		k8-nb-memory-controller-bypass-saturation)		\
__PMC_EV(K8, NB_SIZED_COMMANDS,		k8-nb-sized-commands)		\
__PMC_EV(K8, NB_PROBE_RESULT,		k8-nb-probe-result)		\
__PMC_EV(K8, NB_HT_BUS0_BANDWIDTH,	k8-nb-ht-bus0-bandwidth)	\
__PMC_EV(K8, NB_HT_BUS1_BANDWIDTH,	k8-nb-ht-bus1-bandwidth)	\
__PMC_EV(K8, NB_HT_BUS2_BANDWIDTH,	k8-nb-ht-bus2-bandwidth)

#define	PMC_EV_K8_FIRST		PMC_EV_K8_FP_DISPATCHED_FPU_OPS
#define	PMC_EV_K8_LAST		PMC_EV_K8_NB_HT_BUS2_BANDWIDTH


/* Intel Pentium Events */
#define	__PMC_EV_P5()							\
__PMC_EV(P5, DATA_READ, p5-data-read)					\
__PMC_EV(P5, DATA_WRITE, p5-data-write)					\
__PMC_EV(P5, DATA_TLB_MISS, p5-data-tlb-miss)				\
__PMC_EV(P5, DATA_READ_MISS, p5-data-read-miss)				\
__PMC_EV(P5, DATA_WRITE_MISS, p5-data-write-miss)			\
__PMC_EV(P5, WRITE_HIT_TO_M_OR_E_STATE_LINES,				\
    p5-write-hit-to-m-or-e-state-lines)					\
__PMC_EV(P5, DATA_CACHE_LINES_WRITTEN_BACK,				\
    p4-data-cache-lines-written-back)					\
__PMC_EV(P5, EXTERNAL_SNOOPS, p5-external-snoops)			\
__PMC_EV(P5, EXTERNAL_DATA_CACHE_SNOOP_HITS,				\
    p5-external-data-cache-snoop-hits)					\
__PMC_EV(P5, MEMORY_ACCESSES_IN_BOTH_PIPES,				\
    p5-memory-accesses-in-both-pipes)					\
__PMC_EV(P5, BANK_CONFLICTS, p5-bank-conflicts)				\
__PMC_EV(P5, MISALIGNED_DATA_OR_IO_REFERENCES,				\
    p5-misaligned-data-or-io-references)				\
__PMC_EV(P5, CODE_READ, p5-code-read)					\
__PMC_EV(P5, CODE_TLB_MISS, p5-code-tlb-miss)				\
__PMC_EV(P5, CODE_CACHE_MISS, p5-code-cache-miss)			\
__PMC_EV(P5, ANY_SEGMENT_REGISTER_LOADED,				\
    p5-any-segment-register-loaded)					\
__PMC_EV(P5, BRANCHES, p5-branches)					\
__PMC_EV(P5, BTB_HITS, p5-btb-hits)					\
__PMC_EV(P5, TAKEN_BRANCH_OR_BTB_HIT,					\
    p5-taken-branch-or-btb-hit)						\
__PMC_EV(P5, PIPELINE_FLUSHES, p5-pipeline-flushes)			\
__PMC_EV(P5, INSTRUCTIONS_EXECUTED, p5-instructions-executed)		\
__PMC_EV(P5, INSTRUCTIONS_EXECUTED_V_PIPE,				\
    p5-instructions-executed-v-pipe)					\
__PMC_EV(P5, BUS_CYCLE_DURATION, p5-bus-cycle-duration)			\
__PMC_EV(P5, WRITE_BUFFER_FULL_STALL_DURATION,				\
    p5-write-buffer-full-stall-duration)				\
__PMC_EV(P5, WAITING_FOR_DATA_MEMORY_READ_STALL_DURATION,		\
    p5-waiting-for-data-memory-read-stall-duration)			\
__PMC_EV(P5, STALL_ON_WRITE_TO_AN_E_OR_M_STATE_LINE,			\
    p5-stall-on-write-to-an-e-or-m-state-line)				\
__PMC_EV(P5, LOCKED_BUS_CYCLE, p5-locked-bus-cycle)			\
__PMC_EV(P5, IO_READ_OR_WRITE_CYCLE, p5-io-read-or-write-cycle)		\
__PMC_EV(P5, NONCACHEABLE_MEMORY_READS,					\
    p5-noncacheable-memory-reads)					\
__PMC_EV(P5, PIPELINE_AGI_STALLS, p5-pipeline-agi-stalls)		\
__PMC_EV(P5, FLOPS, p5-flops)						\
__PMC_EV(P5, BREAKPOINT_MATCH_ON_DR0_REGISTER,				\
    p5-breakpoint-match-on-dr0-register)				\
__PMC_EV(P5, BREAKPOINT_MATCH_ON_DR1_REGISTER,				\
    p5-breakpoint-match-on-dr1-register)				\
__PMC_EV(P5, BREAKPOINT_MATCH_ON_DR2_REGISTER,				\
    p5-breakpoint-match-on-dr2-register)				\
__PMC_EV(P5, BREAKPOINT_MATCH_ON_DR3_REGISTER,				\
    p5-breakpoint-match-on-dr3-register)				\
__PMC_EV(P5, HARDWARE_INTERRUPTS, p5-hardware-interrupts)		\
__PMC_EV(P5, DATA_READ_OR_WRITE, p5-data-read-or-write)			\
__PMC_EV(P5, DATA_READ_MISS_OR_WRITE_MISS,				\
    p5-data-read-miss-or-write-miss)					\
__PMC_EV(P5, BUS_OWNERSHIP_LATENCY, p5-bus-ownership-latency)		\
__PMC_EV(P5, BUS_OWNERSHIP_TRANSFERS, p5-bus-ownership-transfers)	\
__PMC_EV(P5, MMX_INSTRUCTIONS_EXECUTED_U_PIPE,				\
    p5-mmx-instructions-executed-u-pipe)				\
__PMC_EV(P5, MMX_INSTRUCTIONS_EXECUTED_V_PIPE,				\
    p5-mmx-instructions-executed-v-pipe)				\
__PMC_EV(P5, CACHE_M_LINE_SHARING, p5-cache-m-line-sharing)		\
__PMC_EV(P5, CACHE_LINE_SHARING, p5-cache-line-sharing)			\
__PMC_EV(P5, EMMS_INSTRUCTIONS_EXECUTED,				\
    p5-emms-instructions-executed)					\
__PMC_EV(P5, TRANSITIONS_BETWEEN_MMX_AND_FP_INSTRUCTIONS,		\
    p5-transitions-between-mmx-and-fp-instructions)			\
__PMC_EV(P5, BUS_UTILIZATION_DUE_TO_PROCESSOR_ACTIVITY,			\
    p5-bus-utilization-due-to-processor-activity)			\
__PMC_EV(P5, WRITES_TO_NONCACHEABLE_MEMORY,				\
    p5-writes-to-noncacheable-memory)					\
__PMC_EV(P5, SATURATING_MMX_INSTRUCTIONS_EXECUTED,			\
    p5-saturating-mmx-instructions-executed)				\
__PMC_EV(P5, SATURATIONS_PERFORMED, p5-saturations-performed)		\
__PMC_EV(P5, NUMBER_OF_CYCLES_NOT_IN_HALT_STATE,			\
    p5-number-of-cycles-not-in-halt-state)				\
__PMC_EV(P5, DATA_CACHE_TLB_MISS_STALL_DURATION,			\
    p5-data-cache-tlb-miss-stall-duration)				\
__PMC_EV(P5, MMX_INSTRUCTION_DATA_READS,				\
    p5-mmx-instruction-data-reads)					\
__PMC_EV(P5, MMX_INSTRUCTION_DATA_READ_MISSES,				\
    p5-mmx-instruction-data-read-misses)				\
__PMC_EV(P5, FLOATING_POINT_STALLS_DURATION,				\
    p5-floating-point-stalls-duration)					\
__PMC_EV(P5, TAKEN_BRANCHES, p5-taken-branches)				\
__PMC_EV(P5, D1_STARVATION_AND_FIFO_IS_EMPTY,				\
    p5-d1-starvation-and-fifo-is-empty)					\
__PMC_EV(P5, D1_STARVATION_AND_ONLY_ONE_INSTRUCTION_IN_FIFO,		\
    p5-d1-starvation-and-only-instruction-in-fifo)			\
__PMC_EV(P5, MMX_INSTRUCTION_DATA_WRITES,				\
    p5-mmx-instruction-data-writes)					\
__PMC_EV(P5, MMX_INSTRUCTION_DATA_WRITE_MISSES,				\
    p5-mmx-instruction-data-write-misses)				\
__PMC_EV(P5, PIPELINE_FLUSHES_DUE_TO_WRONG_BRANCH_PREDICTIONS,		\
    p5-pipeline-flushes-due-to-wrong-branch-predictions)		\
__PMC_EV(P5,								\
    PIPELINE_FLUSHES_DUE_TO_WRONG_BRANCH_PREDICTIONS_RESOLVED_IN_WB_STAGE, \
    p5-pipeline-flushes-due-to-wrong-branch-predictions-resolved-in-wb-stage) \
__PMC_EV(P5, MISALIGNED_DATA_MEMORY_REFERENCE_ON_MMX_INSTRUCTIONS,	\
    p5-misaligned-data-memory-reference-on-mmx-instructions)		\
__PMC_EV(P5, PIPELINE_STALL_FOR_MMX_INSTRUCTION_DATA_MEMORY_READS,	\
    p5-pipeline-stall-for-mmx-instruction-data-memory-reads)		\
__PMC_EV(P5, MISPREDICTED_OR_UNPREDICTED_RETURNS,			\
    p5-mispredicted-or-unpredicted-returns)				\
__PMC_EV(P5, PREDICTED_RETURNS, p5-predicted-returns)			\
__PMC_EV(P5, MMX_MULTIPLY_UNIT_INTERLOCK,				\
    p5-mmx-multiply-unit-interlock)					\
__PMC_EV(P5, MOVD_MOVQ_STORE_STALL_DUE_TO_PREVIOUS_MMX_OPERATION,	\
    p5-movd-movq-store-stall-due-to-previous-mmx-operation)		\
__PMC_EV(P5, RETURNS, p5-returns)					\
__PMC_EV(P5, BTB_FALSE_ENTRIES, p5-btb-false-entries)			\
__PMC_EV(P5, BTB_MISS_PREDICTION_ON_NOT_TAKEN_BRANCH,			\
    p5-btb-miss-prediction-on-not-taken-branch)				\
__PMC_EV(P5,								\
    FULL_WRITE_BUFFER_STALL_DURATION_WHILE_EXECUTING_MMX_INSTRUCTIONS,	\
    p5-full-write-buffer-stall-duration-while-executing-mmx-instructions) \
__PMC_EV(P5, STALL_ON_MMX_INSTRUCTION_WRITE_TO_E_OR_M_STATE_LINE,	\
    p5-stall-on-mmx-instruction-write-to-e-o-m-state-line)

#define	PMC_EV_P5_FIRST		PMC_EV_P5_DATA_READ
#define	PMC_EV_P5_LAST	        \
	PMC_EV_P5_STALL_ON_MMX_INSTRUCTION_WRITE_TO_E_OR_M_STATE_LINE

/* timestamp counters. */
#define	__PMC_EV_TSC()							\
	__PMC_EV(TSC, TSC, tsc)

/* All known PMC events */
#define	__PMC_EVENTS()							\
	__PMC_EV_TSC()							\
	__PMC_EV_K7()							\
	__PMC_EV_P6()							\
	__PMC_EV_P4()							\
	__PMC_EV_K8()							\
	__PMC_EV_P5()							\



enum pmc_event {
#undef	__PMC_EV
#define	__PMC_EV(C,N,D) PMC_EV_ ## C ## _ ## N ,
	__PMC_EVENTS()
};

#define	PMC_EVENT_FIRST	PMC_EV_TSC_TSC
#define	PMC_EVENT_LAST	PMC_EV_P5_LAST

/*
 * Counter capabilities
 *
 * __PMC_CAPS(NAME, VALUE, DESCRIPTION)
 */

#define	__PMC_CAPS()							\
	__PMC_CAP(INTERRUPT,	0, "generate interrupts")		\
	__PMC_CAP(USER,		1, "count user-mode events")		\
	__PMC_CAP(SYSTEM,	2, "count system-mode events")		\
	__PMC_CAP(EDGE,		3, "do edge detection of events")	\
	__PMC_CAP(THRESHOLD,	4, "ignore events below a threshold")	\
	__PMC_CAP(READ,		5, "read PMC counter")			\
	__PMC_CAP(WRITE,	6, "reprogram PMC counter")		\
	__PMC_CAP(INVERT,	7, "invert comparision sense")		\
	__PMC_CAP(QUALIFIER,	8, "further qualify monitored events")	\
	__PMC_CAP(PRECISE,	9, "perform precise sampling")		\
	__PMC_CAP(TAGGING,	10, "tag upstream events")		\
	__PMC_CAP(CASCADE,	11, "cascade counters")

enum pmc_caps
{
#undef	__PMC_CAP
#define	__PMC_CAP(NAME, VALUE, DESCR)	PMC_CAP_##NAME = (1 << VALUE) ,
	__PMC_CAPS()
};

#define	PMC_CAP_FIRST		PMC_CAP_INTERRUPT
#define	PMC_CAP_LAST		PMC_CAP_CASCADE

/*
 * PMC SYSCALL INTERFACE
 */

/*
 * "PMC_OPS" -- these are the commands recognized by the kernel
 * module, and are used when performing a system call from userland.
 */

#define	__PMC_OPS()							\
	__PMC_OP(CONFIGURELOG, "Set log file")				\
	__PMC_OP(GETCPUINFO, "Get system CPU information")		\
	__PMC_OP(GETDRIVERSTATS, "Get driver statistics")		\
	__PMC_OP(GETMODULEVERSION, "Get module version")		\
	__PMC_OP(GETPMCINFO, "Get per-cpu PMC information")		\
	__PMC_OP(PMCADMIN, "Set PMC state")				\
	__PMC_OP(PMCALLOCATE, "Allocate and configure a PMC")		\
	__PMC_OP(PMCATTACH, "Attach a PMC to a process")		\
	__PMC_OP(PMCDETACH, "Detach a PMC from a process")		\
	__PMC_OP(PMCRELEASE, "Release a PMC")				\
	__PMC_OP(PMCRW, "Read/Set a PMC")				\
	__PMC_OP(PMCSETCOUNT, "Set initial count/sampling rate")	\
	__PMC_OP(PMCSTART, "Start a PMC")				\
	__PMC_OP(PMCSTOP, "Start a PMC")				\
	__PMC_OP(WRITELOG, "Write a log file entry")			\
	__PMC_OP(PMCX86GETMSR, "(x86 architectures) retrieve MSR")

enum pmc_ops {
#undef	__PMC_OP
#define	__PMC_OP(N, D)	PMC_OP_##N,
	__PMC_OPS()
};


/*
 * Flags used in operations.
 */

#define	PMC_F_FORCE		0x00000001 /*OP ADMIN force operation */
#define	PMC_F_DESCENDANTS	0x00000002 /*OP ALLOCATE track descendants */
#define	PMC_F_LOG_TC_CSW	0x00000004 /*OP CONFIGURELOG ctx switches */
#define	PMC_F_LOG_TC_PROCEXIT	0x00000008 /*OP CONFIGURELOG log proc exits */
#define	PMC_F_NEWVALUE		0x00000010 /*OP RW write new value */
#define	PMC_F_OLDVALUE		0x00000020 /*OP RW get old value */

/*
 * Cookies used to denote allocated PMCs, and the values of PMCs.
 */

typedef uint32_t	pmc_id_t;
typedef uint64_t	pmc_value_t;

#define	PMC_ID_INVALID	(~ (pmc_id_t) 0)

/*
 * Data structures for system calls supported by the pmc driver.
 */

/*
 * OP PMCALLOCATE
 *
 * Allocate a PMC on the named CPU.
 */

#define	PMC_CPU_ANY	~0

struct pmc_op_pmcallocate {
	uint32_t	pm_caps;	/* PMC_CAP_* */
	uint32_t	pm_cpu;		/* CPU number or PMC_CPU_ANY */
	enum pmc_class	pm_class;	/* class of PMC desired */
	enum pmc_event	pm_ev;		/* [enum pmc_event] desired */
	uint32_t	pm_flags;	/* additional modifiers PMC_F_* */
	enum pmc_mode	pm_mode;	/* desired mode */
	pmc_id_t	pm_pmcid;	/* [return] process pmc id */

	/*
	 * Machine dependent extensions
	 */

#if	__i386__
	uint32_t	pm_config1;
	uint32_t	pm_config2;
#define	pm_amd_config		pm_config1
#define	pm_p4_cccrconfig	pm_config1
#define	pm_p4_escrconfig	pm_config2
#define	pm_p6_config		pm_config1

#elif	__amd64__
	uint32_t	pm_k8_config;
#define	pm_amd_config		pm_k8_config
#endif
};

/*
 * OP PMCADMIN
 *
 * Set the administrative state (i.e., whether enabled or disabled) of
 * a PMC 'pm_pmc' on CPU 'pm_cpu'.  Note that 'pm_pmc' specifies an
 * absolute PMC number and need not have been first allocated by the
 * calling process.
 */

struct pmc_op_pmcadmin {
	int		pm_cpu;		/* CPU# */
	uint32_t	pm_flags;	/* flags */
	int		pm_pmc;         /* PMC# */
	enum pmc_state  pm_state;	/* desired state */
};

/*
 * OP PMCATTACH / OP PMCDETACH
 *
 * Attach/detach a PMC and a process.
 */

struct pmc_op_pmcattach {
	pmc_id_t	pm_pmc;		/* PMC to attach to */
	pid_t		pm_pid;		/* target process */
};

/*
 * OP PMCSETCOUNT
 *
 * Set the sampling rate (i.e., the reload count) for statistical counters.
 * 'pm_pmcid' need to have been previously allocated using PMCALLOCATE.
 */

struct pmc_op_pmcsetcount {
	pmc_value_t	pm_count;	/* initial/sample count */
	pmc_id_t	pm_pmcid;	/* PMC id to set */
};


/*
 * OP PMCRW
 *
 * Read the value of a PMC named by 'pm_pmcid'.  'pm_pmcid' needs
 * to have been previously allocated using PMCALLOCATE.
 */


struct pmc_op_pmcrw {
	uint32_t	pm_flags;	/* PMC_F_{OLD,NEW}VALUE*/
	pmc_id_t	pm_pmcid;	/* pmc id */
	pmc_value_t	pm_value;	/* new&returned value */
};


/*
 * OP GETPMCINFO
 *
 * retrieve PMC state for a named CPU.  The caller is expected to
 * allocate 'npmc' * 'struct pmc_info' bytes of space for the return
 * values.
 */

struct pmc_info {
	uint32_t	pm_caps;	/* counter capabilities */
	enum pmc_class	pm_class;	/* enum pmc_class */
	int		pm_enabled;	/* whether enabled */
	enum pmc_event	pm_event;	/* current event */
	uint32_t	pm_flags;	/* counter flags */
	enum pmc_mode	pm_mode;	/* current mode [enum pmc_mode] */
	pid_t		pm_ownerpid;	/* owner, or -1 */
	pmc_value_t	pm_reloadcount;	/* sampling counters only */
	enum pmc_disp	pm_rowdisp;	/* FREE, THREAD or STANDLONE */
	uint32_t	pm_width;	/* width of the PMC */
	char		pm_name[PMC_NAME_MAX]; /* pmc name */
};

struct pmc_op_getpmcinfo {
	int32_t		pm_cpu;		/* 0 <= cpu < mp_maxid */
	struct pmc_info	pm_pmcs[];	/* space for 'npmc' structures */
};


/*
 * OP GETCPUINFO
 *
 * Retrieve system CPU information.
 */

struct pmc_op_getcpuinfo {
	enum pmc_cputype pm_cputype; /* what kind of CPU */
	uint32_t	pm_nclass;  /* #classes of PMCs */
	uint32_t	pm_ncpu;    /* number of CPUs */
	uint32_t	pm_npmc;    /* #PMCs per CPU */
	enum pmc_class  pm_classes[PMC_CLASS_MAX];
};

/*
 * OP CONFIGURELOG
 *
 * Configure a log file for writing system-wide statistics to.
 */

struct pmc_op_configurelog {
	int		pm_flags;
	int		pm_logfd;   /* logfile fd (or -1) */
};

/*
 * OP GETDRIVERSTATS
 *
 * Retrieve pmc(4) driver-wide statistics.
 */

struct pmc_op_getdriverstats {
	int	pm_intr_ignored;	/* #interrupts ignored */
	int	pm_intr_processed;	/* #interrupts processed */
	int	pm_syscalls;		/* #syscalls */
	int	pm_syscall_errors;	/* #syscalls with errors */
};

/*
 * OP RELEASE / OP START / OP STOP
 *
 * Simple operations on a PMC id.
 */

struct pmc_op_simple {
	pmc_id_t	pm_pmcid;
};

#if	__i386__ || __amd64__

/*
 * OP X86_GETMSR
 *
 * Retrieve the model specific register assoicated with the
 * allocated PMC.  This number can be used subsequently with
 * RDPMC instructions.
 */

struct pmc_op_x86_getmsr {
	uint32_t	pm_msr;		/* MSR for the PMC */
	pmc_id_t	pm_pmcid;	/* allocated pmc id */
};
#endif


#ifdef _KERNEL

#include <sys/malloc.h>
#include <sys/sysctl.h>

#define	PMC_REQUEST_POOL_SIZE			128
#define	PMC_HASH_SIZE				16
#define	PMC_PCPU_BUFFER_SIZE			4096
#define	PMC_MTXPOOL_SIZE			32

/*
 * PMC commands
 */

struct pmc_syscall_args {
	uint32_t	pmop_code;	/* one of PMC_OP_* */
	void		*pmop_data;	/* syscall parameter */
};

/*
 * Interface to processor specific s1tuff
 */

/*
 * struct pmc_descr
 *
 * Machine independent (i.e., the common parts) of a human readable
 * PMC description.
 */

struct pmc_descr {
	const char	pd_name[PMC_NAME_MAX]; /* name */
	uint32_t	pd_caps;	/* capabilities */
	enum pmc_class	pd_class;	/* class of the PMC */
	uint32_t	pd_width;	/* width in bits */
};

/*
 * struct pmc_target
 *
 * This structure records all the target processes associated with a
 * PMC.
 */

struct pmc_target {
	LIST_ENTRY(pmc_target)	pt_next;
	struct pmc_process	*pt_process; /* target descriptor */
};

/*
 * struct pmc
 *
 * Describes each allocated PMC.
 *
 * Each PMC has precisely one owner, namely the process that allocated
 * the PMC.
 *
 * Multiple target process may be being monitored by a PMC.  The
 * 'pm_targets' field links all the target processes being monitored
 * by this PMC.
 *
 * The 'pm_savedvalue' field is protected by a mutex.
 *
 * On a multi-cpu machine, multiple target threads associated with a
 * process-virtual PMC could be concurrently executing on different
 * CPUs.  The 'pm_runcount' field is atomically incremented every time
 * the PMC gets scheduled on a CPU and atomically decremented when it
 * get descheduled.  Deletion of a PMC is only permitted when this
 * field is '0'.
 *
 */

struct pmc {
	LIST_HEAD(,pmc_target) pm_targets;	/* list of target processes */

	/*
	 * Global PMCs are allocated on a CPU and are not moved around.
	 * For global PMCs we need to record the CPU the PMC was allocated
	 * on.
	 *
	 * Virtual PMCs run on whichever CPU is currently executing
	 * their owner threads.  For these PMCs we need to save their
	 * current PMC counter values when they are taken off CPU.
	 */

	union {
		uint32_t	pm_cpu;		/* System-wide PMCs */
		pmc_value_t	pm_savedvalue;	/* Virtual PMCS */
	} pm_gv;

	/*
	 * for sampling modes, we keep track of the PMC's "reload
	 * count", which is the counter value to be loaded in when
	 * arming the PMC for the next counting session.  For counting
	 * modes on PMCs that are read-only (e.g., the x86 TSC), we
	 * keep track of the initial value at the start of
	 * counting-mode operation.
	 */

	union {
		pmc_value_t	pm_reloadcount;	/* sampling PMC modes */
		pmc_value_t	pm_initial;	/* counting PMC modes */
	} pm_sc;

	uint32_t	pm_caps;	/* PMC capabilities */
	enum pmc_class	pm_class;	/* class of PMC */
	enum pmc_event	pm_event;	/* event being measured */
	uint32_t	pm_flags;	/* additional flags PMC_F_... */
	enum pmc_mode	pm_mode;	/* current mode */
	struct pmc_owner *pm_owner;	/* owner thread state */
	uint32_t	pm_rowindex;	/* row index */
	uint32_t	pm_runcount;	/* #cpus currently on */
	enum pmc_state	pm_state;	/* state (active/inactive only) */

	/* md extensions */
#if	__i386__
	union {
		/* AMD Athlon counters */
		struct {
			uint32_t	pm_amd_evsel;
		} pm_amd;

		/* Intel P4 counters */
		struct {
			uint32_t	pm_p4_cccrvalue;
			uint32_t	pm_p4_escrvalue;
			uint32_t	pm_p4_escr;
			uint32_t	pm_p4_escrmsr;
		} pm_p4;

		/* Intel P6 counters */
		struct {
			uint32_t	pm_p6_evsel;
		} pm_p6;
	} pm_md;

#elif	__amd64__
	union {
		/* AMD Athlon counters */
		struct {
			uint32_t	pm_amd_evsel;
		} pm_amd;
	} pm_md;

#else

#error	Unsupported PMC architecture.

#endif
};

/*
 * struct pmc_list
 *
 * Describes a list of PMCs.
 */

struct pmc_list {
	LIST_ENTRY(pmc_list) pl_next;
	struct pmc	*pl_pmc;	/* PMC descriptor */
};

/*
 * struct pmc_process
 *
 * Record a 'target' process being profiled.
 *
 * The target process being profiled could be different from the owner
 * process which allocated the PMCs.  Each target process descriptor
 * is associated with NHWPMC 'struct pmc *' pointers.  Each PMC at a
 * given hardware row-index 'n' will use slot 'n' of the 'pp_pmcs[]'
 * array.  The size of this structure is thus PMC architecture
 * dependent.
 *
 * TODO: Only process-private counting mode PMCs may be attached to a
 * process different from the allocator process (since we do not have
 * the infrastructure to make sense of an interrupted PC value from a
 * 'target' process (yet)).
 *
 */

struct pmc_targetstate {
	struct pmc	*pp_pmc;   /* target PMC */
	pmc_value_t	pp_pmcval; /* per-process value */
};

struct pmc_process {
	LIST_ENTRY(pmc_process) pp_next;	/* hash chain */
	int		pp_refcnt;		/* reference count */
	struct proc	*pp_proc;		/* target thread */
	struct pmc_targetstate pp_pmcs[];       /* NHWPMCs */
};


/*
 * struct pmc_owner
 *
 * We associate a PMC with an 'owner' process.
 *
 * A process can be associated with 0..NCPUS*NHWPMC PMCs during its
 * lifetime, where NCPUS is the numbers of CPUS in the system and
 * NHWPMC is the number of hardware PMCs per CPU.  These are
 * maintained in the list headed by the 'po_pmcs' to save on space.
 *
 */

struct pmc_owner  {
	LIST_ENTRY(pmc_owner) po_next;	/* hash chain */
	LIST_HEAD(, pmc_list) po_pmcs;	/* list of owned PMCs */
	uint32_t	po_flags;	/* PMC_FLAG_* */
	struct proc	*po_owner;	/* owner proc */
	int		po_logfd;       /* XXX for now */
};

#define	PMC_FLAG_IS_OWNER	0x01
#define	PMC_FLAG_HAS_TS_PMC	0x02
#define	PMC_FLAG_OWNS_LOGFILE	0x04 /* owns system-sampling log file */

/*
 * struct pmc_hw -- describe the state of the PMC hardware
 *
 * When in use, a HW PMC is associated with one allocated 'struct pmc'
 * pointed to by field 'phw_pmc'.  When inactive, this field is NULL.
 *
 * On an SMP box, one or more HW PMC's in process virtual mode with
 * the same 'phw_pmc' could be executing on different CPUs.  In order
 * to handle this case correctly, we need to ensure that only
 * incremental counts get added to the saved value in the associated
 * 'struct pmc'.  The 'phw_save' field is used to keep the saved PMC
 * value at the time the hardware is started during this context
 * switch (i.e., the difference between the new (hardware) count and
 * the saved count is atomically added to the count field in 'struct
 * pmc' at context switch time).
 *
 */

struct pmc_hw {
	uint32_t	phw_state;	/* see PHW_* macros below */
	struct pmc	*phw_pmc;	/* current thread PMC */
};

#define	PMC_PHW_RI_MASK		0x000000FF
#define	PMC_PHW_CPU_SHIFT	8
#define	PMC_PHW_CPU_MASK	0x0000FF00
#define	PMC_PHW_FLAGS_SHIFT	16
#define	PMC_PHW_FLAGS_MASK	0xFFFF0000

#define	PMC_PHW_INDEX_TO_STATE(ri)	((ri) & PMC_PHW_RI_MASK)
#define	PMC_PHW_STATE_TO_INDEX(state)	((state) & PMC_PHW_RI_MASK)
#define	PMC_PHW_CPU_TO_STATE(cpu)	(((cpu) << PMC_PHW_CPU_SHIFT) & \
	PMC_PHW_CPU_MASK)
#define	PMC_PHW_STATE_TO_CPU(state)	(((state) & PMC_PHW_CPU_MASK) >> \
	PMC_PHW_CPU_SHIFT)
#define	PMC_PHW_FLAGS_TO_STATE(flags)	(((flags) << PMC_PHW_FLAGS_SHIFT) & \
	PMC_PHW_FLAGS_MASK)
#define	PMC_PHW_STATE_TO_FLAGS(state)	(((state) & PMC_PHW_FLAGS_MASK) >> \
	PMC_PHW_FLAGS_SHIFT)
#define	PMC_PHW_FLAG_IS_ENABLED		(PMC_PHW_FLAGS_TO_STATE(0x01))
#define	PMC_PHW_FLAG_IS_SHAREABLE	(PMC_PHW_FLAGS_TO_STATE(0x02))

/*
 * struct pmc_cpustate
 *
 * A CPU is modelled as a collection of HW PMCs with space for additional
 * flags.
 */

struct pmc_cpu {
	uint32_t	pc_state;	/* physical cpu number + flags */
	struct pmc_hw	*pc_hwpmcs[];	/* 'npmc' pointers */
	/* other machine dependent fields come here */
};

#define	PMC_PCPU_CPU_MASK		0x000000FF
#define	PMC_PCPU_FLAGS_MASK		0xFFFFFF00
#define	PMC_PCPU_FLAGS_SHIFT		8
#define	PMC_PCPU_STATE_TO_CPU(S)	((S) & PMC_PCPU_CPU_MASK)
#define	PMC_PCPU_STATE_TO_FLAGS(S)	(((S) & PMC_PCPU_FLAGS_MASK) >> PMC_PCPU_FLAGS_SHIFT)
#define	PMC_PCPU_FLAGS_TO_STATE(F)	(((F) << PMC_PCPU_FLAGS_SHIFT) & PMC_PCPU_FLAGS_MASK)
#define	PMC_PCPU_CPU_TO_STATE(C)	((C) & PMC_PCPU_CPU_MASK)
#define	PMC_PCPU_FLAG_HTT		(PMC_PCPU_FLAGS_TO_STATE(0x1))

/*
 * struct pmc_binding
 *
 * CPU binding information.
 */

struct pmc_binding {
	int	pb_bound;	/* is bound? */
	int	pb_cpu;		/* if so, to which CPU */
};

/*
 * struct pmc_mdep
 *
 * Machine dependent bits needed per CPU type.
 */

struct pmc_mdep  {
	enum pmc_class  pmd_classes[PMC_CLASS_MAX];
	int		pmd_nclasspmcs[PMC_CLASS_MAX];

	uint32_t	pmd_cputype;    /* from enum pmc_cputype */
	uint32_t	pmd_nclass;	/* # PMC classes supported */
	uint32_t	pmd_npmc;	/* max PMCs per CPU */

	/*
	 * Methods
	 */

	int (*pmd_init)(int _cpu);    /* machine dependent initialization */
	int (*pmd_cleanup)(int _cpu); /* machine dependent cleanup  */

	/* thread context switch in */
	int (*pmd_switch_in)(struct pmc_cpu *_p);

	/* thread context switch out  */
	int (*pmd_switch_out)(struct pmc_cpu *_p);

	/* configuring/reading/writing the hardware PMCs */
	int (*pmd_config_pmc)(int _cpu, int _ri, struct pmc *_pm);
	int (*pmd_read_pmc)(int _cpu, int _ri, pmc_value_t *_value);
	int (*pmd_write_pmc)(int _cpu, int _ri, pmc_value_t _value);

	/* pmc allocation/release */
	int (*pmd_allocate_pmc)(int _cpu, int _ri, struct pmc *_t,
		const struct pmc_op_pmcallocate *_a);
	int (*pmd_release_pmc)(int _cpu, int _ri, struct pmc *_pm);

	/* starting and stopping PMCs */
	int (*pmd_start_pmc)(int _cpu, int _ri);
	int (*pmd_stop_pmc)(int _cpu, int _ri);

	/* handle a PMC interrupt */
	int (*pmd_intr)(int _cpu, uintptr_t _pc);

	int (*pmd_describe)(int _cpu, int _ri, struct pmc_info *_pi,
		struct pmc **_ppmc);

	/* Machine dependent methods */
#if __i386__ || __amd64__
	int (*pmd_get_msr)(int _ri, uint32_t *_msr);
#endif

};

/*
 * Per-CPU state.  This is an array of 'mp_ncpu' pointers
 * to struct pmc_cpu descriptors.
 */

extern struct pmc_cpu **pmc_pcpu;

/* driver statistics */
extern struct pmc_op_getdriverstats pmc_stats;

#if	DEBUG

/* debug flags */
extern unsigned int pmc_debugflags; /* [Maj:12bits] [Min:16bits] [level:4] */

#define	PMC_DEBUG_DEFAULT_FLAGS		0
#define	PMC_DEBUG_STRSIZE		128

#define	__PMCDFMAJ(M)	(1 << (PMC_DEBUG_MAJ_##M+20))
#define	__PMCDFMIN(M)	(1 << (PMC_DEBUG_MIN_##M+4))

#define	__PMCDF(M,N)	(__PMCDFMAJ(M) | __PMCDFMIN(N))
#define	PMCDBG(M,N,L,F,...) do {					\
	if (((pmc_debugflags & __PMCDF(M,N)) == __PMCDF(M,N)) &&	\
	    ((pmc_debugflags & 0xF) > (L)))				\
		printf(#M ":" #N ": " F "\n", __VA_ARGS__);		\
} while (0)

/* Major numbers */
#define	PMC_DEBUG_MAJ_MOD		0 /* misc module infrastructure */
#define	PMC_DEBUG_MAJ_PMC		1 /* pmc management */
#define	PMC_DEBUG_MAJ_CTX		2 /* context switches */
#define	PMC_DEBUG_MAJ_OWN		3 /* owner */
#define	PMC_DEBUG_MAJ_PRC		4 /* processes */
#define	PMC_DEBUG_MAJ_MDP		5 /* machine dependent */
#define	PMC_DEBUG_MAJ_CPU		6 /* cpu switches */

/* Minor numbers */

/* Common (8 bits) */
#define	PMC_DEBUG_MIN_ALL		0 /* allocation */
#define	PMC_DEBUG_MIN_REL		1 /* release */
#define	PMC_DEBUG_MIN_OPS		2 /* ops: start, stop, ... */
#define	PMC_DEBUG_MIN_INI		3 /* init */
#define	PMC_DEBUG_MIN_FND		4 /* find */

/* MODULE */
#define	PMC_DEBUG_MIN_PMH 	       14 /* pmc_hook */
#define	PMC_DEBUG_MIN_PMS	       15 /* pmc_syscall */

/* OWN */
#define	PMC_DEBUG_MIN_ORM		8 /* owner remove */
#define	PMC_DEBUG_MIN_OMR		9 /* owner maybe remove */

/* PROCESSES */
#define	PMC_DEBUG_MIN_TLK		8 /* link target */
#define	PMC_DEBUG_MIN_TUL		9 /* unlink target */
#define	PMC_DEBUG_MIN_EXT	       10 /* process exit */
#define	PMC_DEBUG_MIN_EXC	       11 /* process exec */
#define	PMC_DEBUG_MIN_FRK	       12 /* process fork */
#define	PMC_DEBUG_MIN_ATT	       13 /* attach/detach */

/* CONTEXT SWITCHES */
#define	PMC_DEBUG_MIN_SWI		8 /* switch in */
#define	PMC_DEBUG_MIN_SWO		9 /* switch out */

/* PMC */
#define	PMC_DEBUG_MIN_REG		8 /* pmc register */
#define	PMC_DEBUG_MIN_ALR		9 /* allocate row */

/* MACHINE DEPENDENT LAYER */
#define	PMC_DEBUG_MIN_REA		8 /* read */
#define	PMC_DEBUG_MIN_WRI		9 /* write */
#define	PMC_DEBUG_MIN_CFG	       10 /* config */
#define	PMC_DEBUG_MIN_STA	       11 /* start */
#define	PMC_DEBUG_MIN_STO	       12 /* stop */

/* CPU */
#define	PMC_DEBUG_MIN_BND	       	8 /* bind */
#define	PMC_DEBUG_MIN_SEL		9 /* select */

#else
#define	PMCDBG(M,N,L,F,...)		/* nothing */
#endif

/* declare a dedicated memory pool */
MALLOC_DECLARE(M_PMC);

/*
 * Functions
 */

void	pmc_update_histogram(struct pmc_hw *phw, uintptr_t pc);
void	pmc_send_signal(struct pmc *pmc);
int	pmc_getrowdisp(int ri);

#endif /* _KERNEL */
#endif /* _SYS_PMC_H_ */
