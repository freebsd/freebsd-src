/*-
 * Copyright (c) 2005  Joseph Koshy
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

#ifndef _DEV_HWPMC_PMC_EVENTS_H_
#define	_DEV_HWPMC_PMC_EVENTS_H_

/*
 * PMC event codes.
 *
 * __PMC_EV(CLASS, SYMBOLIC-NAME, VALUE, READABLE-NAME)
 *
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

#define	PMC_EVENT_FIRST	PMC_EV_TSC_TSC
#define	PMC_EVENT_LAST	PMC_EV_P5_LAST

#endif /* _DEV_HWPMC_PMC_EVENTS_H_ */
