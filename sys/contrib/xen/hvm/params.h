/*
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 *
 * Copyright (c) 2007, Keir Fraser
 */

#ifndef __XEN_PUBLIC_HVM_PARAMS_H__
#define __XEN_PUBLIC_HVM_PARAMS_H__

#include "hvm_op.h"

/* These parameters are deprecated and their meaning is undefined. */
#if defined(__XEN__) || defined(__XEN_TOOLS__)

#define HVM_PARAM_PAE_ENABLED                4
#define HVM_PARAM_DM_DOMAIN                 13
#define HVM_PARAM_MEMORY_EVENT_CR0          20
#define HVM_PARAM_MEMORY_EVENT_CR3          21
#define HVM_PARAM_MEMORY_EVENT_CR4          22
#define HVM_PARAM_MEMORY_EVENT_INT3         23
#define HVM_PARAM_NESTEDHVM                 24
#define HVM_PARAM_MEMORY_EVENT_SINGLE_STEP  25
#define HVM_PARAM_BUFIOREQ_EVTCHN           26
#define HVM_PARAM_MEMORY_EVENT_MSR          30

#endif /* defined(__XEN__) || defined(__XEN_TOOLS__) */

/*
 * Parameter space for HVMOP_{set,get}_param.
 */

#define HVM_PARAM_CALLBACK_IRQ 0
#define HVM_PARAM_CALLBACK_IRQ_TYPE_MASK xen_mk_ullong(0xFF00000000000000)
/*
 * How should CPU0 event-channel notifications be delivered?
 *
 * If val == 0 then CPU0 event-channel notifications are not delivered.
 * If val != 0, val[63:56] encodes the type, as follows:
 */

#define HVM_PARAM_CALLBACK_TYPE_GSI      0
/*
 * val[55:0] is a delivery GSI.  GSI 0 cannot be used, as it aliases val == 0,
 * and disables all notifications.
 */

#define HVM_PARAM_CALLBACK_TYPE_PCI_INTX 1
/*
 * val[55:0] is a delivery PCI INTx line:
 * Domain = val[47:32], Bus = val[31:16] DevFn = val[15:8], IntX = val[1:0]
 */

#if defined(__i386__) || defined(__x86_64__)
#define HVM_PARAM_CALLBACK_TYPE_VECTOR   2
/*
 * val[7:0] is a vector number.  Check for XENFEAT_hvm_callback_vector to know
 * if this delivery method is available.
 */
#elif defined(__arm__) || defined(__aarch64__)
#define HVM_PARAM_CALLBACK_TYPE_PPI      2
/*
 * val[55:16] needs to be zero.
 * val[15:8] is interrupt flag of the PPI used by event-channel:
 *  bit 8: the PPI is edge(1) or level(0) triggered
 *  bit 9: the PPI is active low(1) or high(0)
 * val[7:0] is a PPI number used by event-channel.
 * This is only used by ARM/ARM64 and masking/eoi the interrupt associated to
 * the notification is handled by the interrupt controller.
 */
#define HVM_PARAM_CALLBACK_TYPE_PPI_FLAG_MASK      0xFF00
#define HVM_PARAM_CALLBACK_TYPE_PPI_FLAG_LOW_LEVEL 2
#endif

/*
 * These are not used by Xen. They are here for convenience of HVM-guest
 * xenbus implementations.
 */
#define HVM_PARAM_STORE_PFN    1
#define HVM_PARAM_STORE_EVTCHN 2

#define HVM_PARAM_IOREQ_PFN    5

#define HVM_PARAM_BUFIOREQ_PFN 6

#if defined(__i386__) || defined(__x86_64__)

/*
 * Viridian enlightenments
 *
 * (See http://download.microsoft.com/download/A/B/4/AB43A34E-BDD0-4FA6-BDEF-79EEF16E880B/Hypervisor%20Top%20Level%20Functional%20Specification%20v4.0.docx)
 *
 * To expose viridian enlightenments to the guest set this parameter
 * to the desired feature mask. The base feature set must be present
 * in any valid feature mask.
 */
#define HVM_PARAM_VIRIDIAN     9

/* Base+Freq viridian feature sets:
 *
 * - Hypercall MSRs (HV_X64_MSR_GUEST_OS_ID and HV_X64_MSR_HYPERCALL)
 * - APIC access MSRs (HV_X64_MSR_EOI, HV_X64_MSR_ICR and HV_X64_MSR_TPR)
 * - Virtual Processor index MSR (HV_X64_MSR_VP_INDEX)
 * - Timer frequency MSRs (HV_X64_MSR_TSC_FREQUENCY and
 *   HV_X64_MSR_APIC_FREQUENCY)
 */
#define _HVMPV_base_freq 0
#define HVMPV_base_freq  (1 << _HVMPV_base_freq)

/* Feature set modifications */

/* Disable timer frequency MSRs (HV_X64_MSR_TSC_FREQUENCY and
 * HV_X64_MSR_APIC_FREQUENCY).
 * This modification restores the viridian feature set to the
 * original 'base' set exposed in releases prior to Xen 4.4.
 */
#define _HVMPV_no_freq 1
#define HVMPV_no_freq  (1 << _HVMPV_no_freq)

/* Enable Partition Time Reference Counter (HV_X64_MSR_TIME_REF_COUNT) */
#define _HVMPV_time_ref_count 2
#define HVMPV_time_ref_count  (1 << _HVMPV_time_ref_count)

/* Enable Reference TSC Page (HV_X64_MSR_REFERENCE_TSC) */
#define _HVMPV_reference_tsc 3
#define HVMPV_reference_tsc  (1 << _HVMPV_reference_tsc)

/* Use Hypercall for remote TLB flush */
#define _HVMPV_hcall_remote_tlb_flush 4
#define HVMPV_hcall_remote_tlb_flush (1 << _HVMPV_hcall_remote_tlb_flush)

/* Use APIC assist */
#define _HVMPV_apic_assist 5
#define HVMPV_apic_assist (1 << _HVMPV_apic_assist)

/* Enable crash MSRs */
#define _HVMPV_crash_ctl 6
#define HVMPV_crash_ctl (1 << _HVMPV_crash_ctl)

/* Enable SYNIC MSRs */
#define _HVMPV_synic 7
#define HVMPV_synic (1 << _HVMPV_synic)

/* Enable STIMER MSRs */
#define _HVMPV_stimer 8
#define HVMPV_stimer (1 << _HVMPV_stimer)

/* Use Synthetic Cluster IPI Hypercall */
#define _HVMPV_hcall_ipi 9
#define HVMPV_hcall_ipi (1 << _HVMPV_hcall_ipi)

/* Enable ExProcessorMasks */
#define _HVMPV_ex_processor_masks 10
#define HVMPV_ex_processor_masks (1 << _HVMPV_ex_processor_masks)

/* Allow more than 64 VPs */
#define _HVMPV_no_vp_limit 11
#define HVMPV_no_vp_limit (1 << _HVMPV_no_vp_limit)

/* Enable vCPU hotplug */
#define _HVMPV_cpu_hotplug 12
#define HVMPV_cpu_hotplug (1 << _HVMPV_cpu_hotplug)

#define HVMPV_feature_mask \
        (HVMPV_base_freq | \
         HVMPV_no_freq | \
         HVMPV_time_ref_count | \
         HVMPV_reference_tsc | \
         HVMPV_hcall_remote_tlb_flush | \
         HVMPV_apic_assist | \
         HVMPV_crash_ctl | \
         HVMPV_synic | \
         HVMPV_stimer | \
         HVMPV_hcall_ipi | \
         HVMPV_ex_processor_masks | \
         HVMPV_no_vp_limit | \
         HVMPV_cpu_hotplug)

#endif

/*
 * Set mode for virtual timers (currently x86 only):
 *  delay_for_missed_ticks (default):
 *   Do not advance a vcpu's time beyond the correct delivery time for
 *   interrupts that have been missed due to preemption. Deliver missed
 *   interrupts when the vcpu is rescheduled and advance the vcpu's virtual
 *   time stepwise for each one.
 *  no_delay_for_missed_ticks:
 *   As above, missed interrupts are delivered, but guest time always tracks
 *   wallclock (i.e., real) time while doing so.
 *  no_missed_ticks_pending:
 *   No missed interrupts are held pending. Instead, to ensure ticks are
 *   delivered at some non-zero rate, if we detect missed ticks then the
 *   internal tick alarm is not disabled if the VCPU is preempted during the
 *   next tick period.
 *  one_missed_tick_pending:
 *   Missed interrupts are collapsed together and delivered as one 'late tick'.
 *   Guest time always tracks wallclock (i.e., real) time.
 */
#define HVM_PARAM_TIMER_MODE   10
#define HVMPTM_delay_for_missed_ticks    0
#define HVMPTM_no_delay_for_missed_ticks 1
#define HVMPTM_no_missed_ticks_pending   2
#define HVMPTM_one_missed_tick_pending   3

/* Boolean: Enable virtual HPET (high-precision event timer)? (x86-only) */
#define HVM_PARAM_HPET_ENABLED 11

/* Identity-map page directory used by Intel EPT when CR0.PG=0. */
#define HVM_PARAM_IDENT_PT     12

/* ACPI S state: currently support S0 and S3 on x86. */
#define HVM_PARAM_ACPI_S_STATE 14

/* TSS used on Intel when CR0.PE=0. */
#define HVM_PARAM_VM86_TSS     15

/* Boolean: Enable aligning all periodic vpts to reduce interrupts */
#define HVM_PARAM_VPT_ALIGN    16

/* Console debug shared memory ring and event channel */
#define HVM_PARAM_CONSOLE_PFN    17
#define HVM_PARAM_CONSOLE_EVTCHN 18

/*
 * Select location of ACPI PM1a and TMR control blocks. Currently two locations
 * are supported, specified by version 0 or 1 in this parameter:
 *   - 0: default, use the old addresses
 *        PM1A_EVT == 0x1f40; PM1A_CNT == 0x1f44; PM_TMR == 0x1f48
 *   - 1: use the new default qemu addresses
 *        PM1A_EVT == 0xb000; PM1A_CNT == 0xb004; PM_TMR == 0xb008
 * You can find these address definitions in <hvm/ioreq.h>
 */
#define HVM_PARAM_ACPI_IOPORTS_LOCATION 19

/* Params for the mem event rings */
#define HVM_PARAM_PAGING_RING_PFN   27
#define HVM_PARAM_MONITOR_RING_PFN  28
#define HVM_PARAM_SHARING_RING_PFN  29

/* SHUTDOWN_* action in case of a triple fault */
#define HVM_PARAM_TRIPLE_FAULT_REASON 31

#define HVM_PARAM_IOREQ_SERVER_PFN 32
#define HVM_PARAM_NR_IOREQ_SERVER_PAGES 33

/* Location of the VM Generation ID in guest physical address space. */
#define HVM_PARAM_VM_GENERATION_ID_ADDR 34

/*
 * Set mode for altp2m:
 *  disabled: don't activate altp2m (default)
 *  mixed: allow access to all altp2m ops for both in-guest and external tools
 *  external: allow access to external privileged tools only
 *  limited: guest only has limited access (ie. control VMFUNC and #VE)
 *
 * Note that 'mixed' mode has not been evaluated for safety from a
 * security perspective.  Before using this mode in a
 * security-critical environment, each subop should be evaluated for
 * safety, with unsafe subops blacklisted in XSM.
 */
#define HVM_PARAM_ALTP2M       35
#define XEN_ALTP2M_disabled      0
#define XEN_ALTP2M_mixed         1
#define XEN_ALTP2M_external      2
#define XEN_ALTP2M_limited       3

/*
 * Size of the x87 FPU FIP/FDP registers that the hypervisor needs to
 * save/restore.  This is a workaround for a hardware limitation that
 * does not allow the full FIP/FDP and FCS/FDS to be restored.
 *
 * Valid values are:
 *
 * 8: save/restore 64-bit FIP/FDP and clear FCS/FDS (default if CPU
 *    has FPCSDS feature).
 *
 * 4: save/restore 32-bit FIP/FDP, FCS/FDS, and clear upper 32-bits of
 *    FIP/FDP.
 *
 * 0: allow hypervisor to choose based on the value of FIP/FDP
 *    (default if CPU does not have FPCSDS).
 *
 * If FPCSDS (bit 13 in CPUID leaf 0x7, subleaf 0x0) is set, the CPU
 * never saves FCS/FDS and this parameter should be left at the
 * default of 8.
 */
#define HVM_PARAM_X87_FIP_WIDTH 36

/*
 * TSS (and its size) used on Intel when CR0.PE=0. The address occupies
 * the low 32 bits, while the size is in the high 32 ones.
 */
#define HVM_PARAM_VM86_TSS_SIZED 37

/* Enable MCA capabilities. */
#define HVM_PARAM_MCA_CAP 38
#define XEN_HVM_MCA_CAP_LMCE   (xen_mk_ullong(1) << 0)
#define XEN_HVM_MCA_CAP_MASK   XEN_HVM_MCA_CAP_LMCE

#define HVM_NR_PARAMS 39

#endif /* __XEN_PUBLIC_HVM_PARAMS_H__ */
