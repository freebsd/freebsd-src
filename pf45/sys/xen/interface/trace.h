/******************************************************************************
 * include/public/trace.h
 * 
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
 * Mark Williamson, (C) 2004 Intel Research Cambridge
 * Copyright (C) 2005 Bin Ren
 */

#ifndef __XEN_PUBLIC_TRACE_H__
#define __XEN_PUBLIC_TRACE_H__

#define TRACE_EXTRA_MAX    7
#define TRACE_EXTRA_SHIFT 28

/* Trace classes */
#define TRC_CLS_SHIFT 16
#define TRC_GEN      0x0001f000    /* General trace            */
#define TRC_SCHED    0x0002f000    /* Xen Scheduler trace      */
#define TRC_DOM0OP   0x0004f000    /* Xen DOM0 operation trace */
#define TRC_HVM      0x0008f000    /* Xen HVM trace            */
#define TRC_MEM      0x0010f000    /* Xen memory trace         */
#define TRC_PV       0x0020f000    /* Xen PV traces            */
#define TRC_SHADOW   0x0040f000    /* Xen shadow tracing       */
#define TRC_ALL      0x0ffff000
#define TRC_HD_TO_EVENT(x) ((x)&0x0fffffff)
#define TRC_HD_CYCLE_FLAG (1UL<<31)
#define TRC_HD_INCLUDES_CYCLE_COUNT(x) ( !!( (x) & TRC_HD_CYCLE_FLAG ) )
#define TRC_HD_EXTRA(x)    (((x)>>TRACE_EXTRA_SHIFT)&TRACE_EXTRA_MAX)

/* Trace subclasses */
#define TRC_SUBCLS_SHIFT 12

/* trace subclasses for SVM */
#define TRC_HVM_ENTRYEXIT 0x00081000   /* VMENTRY and #VMEXIT       */
#define TRC_HVM_HANDLER   0x00082000   /* various HVM handlers      */

#define TRC_SCHED_MIN       0x00021000   /* Just runstate changes */
#define TRC_SCHED_VERBOSE   0x00028000   /* More inclusive scheduling */

/* Trace events per class */
#define TRC_LOST_RECORDS        (TRC_GEN + 1)
#define TRC_TRACE_WRAP_BUFFER  (TRC_GEN + 2)
#define TRC_TRACE_CPU_CHANGE    (TRC_GEN + 3)

#define TRC_SCHED_RUNSTATE_CHANGE (TRC_SCHED_MIN + 1)
#define TRC_SCHED_DOM_ADD        (TRC_SCHED_VERBOSE +  1)
#define TRC_SCHED_DOM_REM        (TRC_SCHED_VERBOSE +  2)
#define TRC_SCHED_SLEEP          (TRC_SCHED_VERBOSE +  3)
#define TRC_SCHED_WAKE           (TRC_SCHED_VERBOSE +  4)
#define TRC_SCHED_YIELD          (TRC_SCHED_VERBOSE +  5)
#define TRC_SCHED_BLOCK          (TRC_SCHED_VERBOSE +  6)
#define TRC_SCHED_SHUTDOWN       (TRC_SCHED_VERBOSE +  7)
#define TRC_SCHED_CTL            (TRC_SCHED_VERBOSE +  8)
#define TRC_SCHED_ADJDOM         (TRC_SCHED_VERBOSE +  9)
#define TRC_SCHED_SWITCH         (TRC_SCHED_VERBOSE + 10)
#define TRC_SCHED_S_TIMER_FN     (TRC_SCHED_VERBOSE + 11)
#define TRC_SCHED_T_TIMER_FN     (TRC_SCHED_VERBOSE + 12)
#define TRC_SCHED_DOM_TIMER_FN   (TRC_SCHED_VERBOSE + 13)
#define TRC_SCHED_SWITCH_INFPREV (TRC_SCHED_VERBOSE + 14)
#define TRC_SCHED_SWITCH_INFNEXT (TRC_SCHED_VERBOSE + 15)

#define TRC_MEM_PAGE_GRANT_MAP      (TRC_MEM + 1)
#define TRC_MEM_PAGE_GRANT_UNMAP    (TRC_MEM + 2)
#define TRC_MEM_PAGE_GRANT_TRANSFER (TRC_MEM + 3)

#define TRC_PV_HYPERCALL             (TRC_PV +  1)
#define TRC_PV_TRAP                  (TRC_PV +  3)
#define TRC_PV_PAGE_FAULT            (TRC_PV +  4)
#define TRC_PV_FORCED_INVALID_OP     (TRC_PV +  5)
#define TRC_PV_EMULATE_PRIVOP        (TRC_PV +  6)
#define TRC_PV_EMULATE_4GB           (TRC_PV +  7)
#define TRC_PV_MATH_STATE_RESTORE    (TRC_PV +  8)
#define TRC_PV_PAGING_FIXUP          (TRC_PV +  9)
#define TRC_PV_GDT_LDT_MAPPING_FAULT (TRC_PV + 10)
#define TRC_PV_PTWR_EMULATION        (TRC_PV + 11)
#define TRC_PV_PTWR_EMULATION_PAE    (TRC_PV + 12)
  /* Indicates that addresses in trace record are 64 bits */
#define TRC_64_FLAG               (0x100) 

#define TRC_SHADOW_NOT_SHADOW                 (TRC_SHADOW +  1)
#define TRC_SHADOW_FAST_PROPAGATE             (TRC_SHADOW +  2)
#define TRC_SHADOW_FAST_MMIO                  (TRC_SHADOW +  3)
#define TRC_SHADOW_FALSE_FAST_PATH            (TRC_SHADOW +  4)
#define TRC_SHADOW_MMIO                       (TRC_SHADOW +  5)
#define TRC_SHADOW_FIXUP                      (TRC_SHADOW +  6)
#define TRC_SHADOW_DOMF_DYING                 (TRC_SHADOW +  7)
#define TRC_SHADOW_EMULATE                    (TRC_SHADOW +  8)
#define TRC_SHADOW_EMULATE_UNSHADOW_USER      (TRC_SHADOW +  9)
#define TRC_SHADOW_EMULATE_UNSHADOW_EVTINJ    (TRC_SHADOW + 10)
#define TRC_SHADOW_EMULATE_UNSHADOW_UNHANDLED (TRC_SHADOW + 11)
#define TRC_SHADOW_WRMAP_BF                   (TRC_SHADOW + 12)
#define TRC_SHADOW_PREALLOC_UNPIN             (TRC_SHADOW + 13)
#define TRC_SHADOW_RESYNC_FULL                (TRC_SHADOW + 14)
#define TRC_SHADOW_RESYNC_ONLY                (TRC_SHADOW + 15)

/* trace events per subclass */
#define TRC_HVM_VMENTRY         (TRC_HVM_ENTRYEXIT + 0x01)
#define TRC_HVM_VMEXIT          (TRC_HVM_ENTRYEXIT + 0x02)
#define TRC_HVM_VMEXIT64        (TRC_HVM_ENTRYEXIT + TRC_64_FLAG + 0x02)
#define TRC_HVM_PF_XEN          (TRC_HVM_HANDLER + 0x01)
#define TRC_HVM_PF_XEN64        (TRC_HVM_HANDLER + TRC_64_FLAG + 0x01)
#define TRC_HVM_PF_INJECT       (TRC_HVM_HANDLER + 0x02)
#define TRC_HVM_PF_INJECT64     (TRC_HVM_HANDLER + TRC_64_FLAG + 0x02)
#define TRC_HVM_INJ_EXC         (TRC_HVM_HANDLER + 0x03)
#define TRC_HVM_INJ_VIRQ        (TRC_HVM_HANDLER + 0x04)
#define TRC_HVM_REINJ_VIRQ      (TRC_HVM_HANDLER + 0x05)
#define TRC_HVM_IO_READ         (TRC_HVM_HANDLER + 0x06)
#define TRC_HVM_IO_WRITE        (TRC_HVM_HANDLER + 0x07)
#define TRC_HVM_CR_READ         (TRC_HVM_HANDLER + 0x08)
#define TRC_HVM_CR_READ64       (TRC_HVM_HANDLER + TRC_64_FLAG + 0x08)
#define TRC_HVM_CR_WRITE        (TRC_HVM_HANDLER + 0x09)
#define TRC_HVM_CR_WRITE64      (TRC_HVM_HANDLER + TRC_64_FLAG + 0x09)
#define TRC_HVM_DR_READ         (TRC_HVM_HANDLER + 0x0A)
#define TRC_HVM_DR_WRITE        (TRC_HVM_HANDLER + 0x0B)
#define TRC_HVM_MSR_READ        (TRC_HVM_HANDLER + 0x0C)
#define TRC_HVM_MSR_WRITE       (TRC_HVM_HANDLER + 0x0D)
#define TRC_HVM_CPUID           (TRC_HVM_HANDLER + 0x0E)
#define TRC_HVM_INTR            (TRC_HVM_HANDLER + 0x0F)
#define TRC_HVM_NMI             (TRC_HVM_HANDLER + 0x10)
#define TRC_HVM_SMI             (TRC_HVM_HANDLER + 0x11)
#define TRC_HVM_VMMCALL         (TRC_HVM_HANDLER + 0x12)
#define TRC_HVM_HLT             (TRC_HVM_HANDLER + 0x13)
#define TRC_HVM_INVLPG          (TRC_HVM_HANDLER + 0x14)
#define TRC_HVM_INVLPG64        (TRC_HVM_HANDLER + TRC_64_FLAG + 0x14)
#define TRC_HVM_MCE             (TRC_HVM_HANDLER + 0x15)
#define TRC_HVM_IO_ASSIST       (TRC_HVM_HANDLER + 0x16)
#define TRC_HVM_MMIO_ASSIST     (TRC_HVM_HANDLER + 0x17)
#define TRC_HVM_CLTS            (TRC_HVM_HANDLER + 0x18)
#define TRC_HVM_LMSW            (TRC_HVM_HANDLER + 0x19)
#define TRC_HVM_LMSW64          (TRC_HVM_HANDLER + TRC_64_FLAG + 0x19)

/* This structure represents a single trace buffer record. */
struct t_rec {
    uint32_t event:28;
    uint32_t extra_u32:3;         /* # entries in trailing extra_u32[] array */
    uint32_t cycles_included:1;   /* u.cycles or u.no_cycles? */
    union {
        struct {
            uint32_t cycles_lo, cycles_hi; /* cycle counter timestamp */
            uint32_t extra_u32[7];         /* event data items */
        } cycles;
        struct {
            uint32_t extra_u32[7];         /* event data items */
        } nocycles;
    } u;
};

/*
 * This structure contains the metadata for a single trace buffer.  The head
 * field, indexes into an array of struct t_rec's.
 */
struct t_buf {
    /* Assume the data buffer size is X.  X is generally not a power of 2.
     * CONS and PROD are incremented modulo (2*X):
     *     0 <= cons < 2*X
     *     0 <= prod < 2*X
     * This is done because addition modulo X breaks at 2^32 when X is not a
     * power of 2:
     *     (((2^32 - 1) % X) + 1) % X != (2^32) % X
     */
    uint32_t cons;   /* Offset of next item to be consumed by control tools. */
    uint32_t prod;   /* Offset of next item to be produced by Xen.           */
    /*  Records follow immediately after the meta-data header.    */
};

#endif /* __XEN_PUBLIC_TRACE_H__ */

/*
 * Local variables:
 * mode: C
 * c-set-style: "BSD"
 * c-basic-offset: 4
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
