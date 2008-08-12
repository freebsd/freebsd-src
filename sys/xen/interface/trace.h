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

/* Trace classes */
#define TRC_CLS_SHIFT 16
#define TRC_GEN     0x0001f000    /* General trace            */
#define TRC_SCHED   0x0002f000    /* Xen Scheduler trace      */
#define TRC_DOM0OP  0x0004f000    /* Xen DOM0 operation trace */
#define TRC_HVM     0x0008f000    /* Xen HVM trace            */
#define TRC_MEM     0x0010f000    /* Xen memory trace         */
#define TRC_ALL     0xfffff000

/* Trace subclasses */
#define TRC_SUBCLS_SHIFT 12

/* trace subclasses for SVM */
#define TRC_HVM_ENTRYEXIT 0x00081000   /* VMENTRY and #VMEXIT       */
#define TRC_HVM_HANDLER   0x00082000   /* various HVM handlers      */

/* Trace events per class */
#define TRC_LOST_RECORDS        (TRC_GEN + 1)

#define TRC_SCHED_DOM_ADD       (TRC_SCHED +  1)
#define TRC_SCHED_DOM_REM       (TRC_SCHED +  2)
#define TRC_SCHED_SLEEP         (TRC_SCHED +  3)
#define TRC_SCHED_WAKE          (TRC_SCHED +  4)
#define TRC_SCHED_YIELD         (TRC_SCHED +  5)
#define TRC_SCHED_BLOCK         (TRC_SCHED +  6)
#define TRC_SCHED_SHUTDOWN      (TRC_SCHED +  7)
#define TRC_SCHED_CTL           (TRC_SCHED +  8)
#define TRC_SCHED_ADJDOM        (TRC_SCHED +  9)
#define TRC_SCHED_SWITCH        (TRC_SCHED + 10)
#define TRC_SCHED_S_TIMER_FN    (TRC_SCHED + 11)
#define TRC_SCHED_T_TIMER_FN    (TRC_SCHED + 12)
#define TRC_SCHED_DOM_TIMER_FN  (TRC_SCHED + 13)
#define TRC_SCHED_SWITCH_INFPREV (TRC_SCHED + 14)
#define TRC_SCHED_SWITCH_INFNEXT (TRC_SCHED + 15)

#define TRC_MEM_PAGE_GRANT_MAP      (TRC_MEM + 1)
#define TRC_MEM_PAGE_GRANT_UNMAP    (TRC_MEM + 2)
#define TRC_MEM_PAGE_GRANT_TRANSFER (TRC_MEM + 3)

/* trace events per subclass */
#define TRC_HVM_VMENTRY         (TRC_HVM_ENTRYEXIT + 0x01)
#define TRC_HVM_VMEXIT          (TRC_HVM_ENTRYEXIT + 0x02)
#define TRC_HVM_PF_XEN          (TRC_HVM_HANDLER + 0x01)
#define TRC_HVM_PF_INJECT       (TRC_HVM_HANDLER + 0x02)
#define TRC_HVM_INJ_EXC         (TRC_HVM_HANDLER + 0x03)
#define TRC_HVM_INJ_VIRQ        (TRC_HVM_HANDLER + 0x04)
#define TRC_HVM_REINJ_VIRQ      (TRC_HVM_HANDLER + 0x05)
#define TRC_HVM_IO_READ         (TRC_HVM_HANDLER + 0x06)
#define TRC_HVM_IO_WRITE        (TRC_HVM_HANDLER + 0x07)
#define TRC_HVM_CR_READ         (TRC_HVM_HANDLER + 0x08)
#define TRC_HVM_CR_WRITE        (TRC_HVM_HANDLER + 0x09)
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
#define TRC_HVM_MCE             (TRC_HVM_HANDLER + 0x15)

/* This structure represents a single trace buffer record. */
struct t_rec {
    uint64_t cycles;          /* cycle counter timestamp */
    uint32_t event;           /* event ID                */
    unsigned long data[5];    /* event data items        */
};

/*
 * This structure contains the metadata for a single trace buffer.  The head
 * field, indexes into an array of struct t_rec's.
 */
struct t_buf {
    uint32_t cons;      /* Next item to be consumed by control tools. */
    uint32_t prod;      /* Next item to be produced by Xen.           */
    /* 'nr_recs' records follow immediately after the meta-data header.    */
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
