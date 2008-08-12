/******************************************************************************
 * arch-x86/xen.h
 * 
 * Guest OS interface to x86 Xen.
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
 * Copyright (c) 2004-2006, K A Fraser
 */

#ifndef __XEN_PUBLIC_ARCH_X86_XEN_H__
#define __XEN_PUBLIC_ARCH_X86_XEN_H__

#if defined(__i386__)
#include <xen/interface/arch-x86/xen-x86_32.h>
#elif defined(__x86_64__)
#include "xen-x86_64.h"
#endif

#ifndef __ASSEMBLY__
/* Guest handles for primitive C types. */
__DEFINE_XEN_GUEST_HANDLE(uchar, unsigned char);
__DEFINE_XEN_GUEST_HANDLE(uint,  unsigned int);
__DEFINE_XEN_GUEST_HANDLE(ulong, unsigned long);
#if 0
DEFINE_XEN_GUEST_HANDLE(char);
DEFINE_XEN_GUEST_HANDLE(int);
DEFINE_XEN_GUEST_HANDLE(long);
DEFINE_XEN_GUEST_HANDLE(void);
#endif
typedef unsigned long xen_pfn_t;
DEFINE_XEN_GUEST_HANDLE(xen_pfn_t);
#define PRI_xen_pfn "lx"
#endif

/*
 * SEGMENT DESCRIPTOR TABLES
 */
/*
 * A number of GDT entries are reserved by Xen. These are not situated at the
 * start of the GDT because some stupid OSes export hard-coded selector values
 * in their ABI. These hard-coded values are always near the start of the GDT,
 * so Xen places itself out of the way, at the far end of the GDT.
 */
#define FIRST_RESERVED_GDT_PAGE  14
#define FIRST_RESERVED_GDT_BYTE  (FIRST_RESERVED_GDT_PAGE * 4096)
#define FIRST_RESERVED_GDT_ENTRY (FIRST_RESERVED_GDT_BYTE / 8)

/* Maximum number of virtual CPUs in multi-processor guests. */
#define MAX_VIRT_CPUS 32

#ifndef __ASSEMBLY__

typedef unsigned long xen_ulong_t;

/*
 * Send an array of these to HYPERVISOR_set_trap_table().
 * The privilege level specifies which modes may enter a trap via a software
 * interrupt. On x86/64, since rings 1 and 2 are unavailable, we allocate
 * privilege levels as follows:
 *  Level == 0: Noone may enter
 *  Level == 1: Kernel may enter
 *  Level == 2: Kernel may enter
 *  Level == 3: Everyone may enter
 */
#define TI_GET_DPL(_ti)      ((_ti)->flags & 3)
#define TI_GET_IF(_ti)       ((_ti)->flags & 4)
#define TI_SET_DPL(_ti,_dpl) ((_ti)->flags |= (_dpl))
#define TI_SET_IF(_ti,_if)   ((_ti)->flags |= ((!!(_if))<<2))
struct trap_info {
    uint8_t       vector;  /* exception vector                              */
    uint8_t       flags;   /* 0-3: privilege level; 4: clear event enable?  */
    uint16_t      cs;      /* code selector                                 */
    unsigned long address; /* code offset                                   */
};
typedef struct trap_info trap_info_t;
DEFINE_XEN_GUEST_HANDLE(trap_info_t);

typedef uint64_t tsc_timestamp_t; /* RDTSC timestamp */

/*
 * The following is all CPU context. Note that the fpu_ctxt block is filled 
 * in by FXSAVE if the CPU has feature FXSR; otherwise FSAVE is used.
 */
struct vcpu_guest_context {
    /* FPU registers come first so they can be aligned for FXSAVE/FXRSTOR. */
    struct { char x[512]; } fpu_ctxt;       /* User-level FPU registers     */
#define VGCF_I387_VALID                (1<<0)
#define VGCF_IN_KERNEL                 (1<<2)
#define _VGCF_i387_valid               0
#define VGCF_i387_valid                (1<<_VGCF_i387_valid)
#define _VGCF_in_kernel                2
#define VGCF_in_kernel                 (1<<_VGCF_in_kernel)
#define _VGCF_failsafe_disables_events 3
#define VGCF_failsafe_disables_events  (1<<_VGCF_failsafe_disables_events)
#define _VGCF_syscall_disables_events  4
#define VGCF_syscall_disables_events   (1<<_VGCF_syscall_disables_events)
#define _VGCF_online                   5
#define VGCF_online                    (1<<_VGCF_online)
    unsigned long flags;                    /* VGCF_* flags                 */
    struct cpu_user_regs user_regs;         /* User-level CPU registers     */
    struct trap_info trap_ctxt[256];        /* Virtual IDT                  */
    unsigned long ldt_base, ldt_ents;       /* LDT (linear address, # ents) */
    unsigned long gdt_frames[16], gdt_ents; /* GDT (machine frames, # ents) */
    unsigned long kernel_ss, kernel_sp;     /* Virtual TSS (only SS1/SP1)   */
    /* NB. User pagetable on x86/64 is placed in ctrlreg[1]. */
    unsigned long ctrlreg[8];               /* CR0-CR7 (control registers)  */
    unsigned long debugreg[8];              /* DB0-DB7 (debug registers)    */
#ifdef __i386__
    unsigned long event_callback_cs;        /* CS:EIP of event callback     */
    unsigned long event_callback_eip;
    unsigned long failsafe_callback_cs;     /* CS:EIP of failsafe callback  */
    unsigned long failsafe_callback_eip;
#else
    unsigned long event_callback_eip;
    unsigned long failsafe_callback_eip;
#ifdef __XEN__
    union {
        unsigned long syscall_callback_eip;
        struct {
            unsigned int event_callback_cs;    /* compat CS of event cb     */
            unsigned int failsafe_callback_cs; /* compat CS of failsafe cb  */
        };
    } u;
#else
    unsigned long syscall_callback_eip;
#endif
#endif
    unsigned long vm_assist;                /* VMASST_TYPE_* bitmap */
#ifdef __x86_64__
    /* Segment base addresses. */
    uint64_t      fs_base;
    uint64_t      gs_base_kernel;
    uint64_t      gs_base_user;
#endif
};
typedef struct vcpu_guest_context vcpu_guest_context_t;
DEFINE_XEN_GUEST_HANDLE(vcpu_guest_context_t);

struct arch_shared_info {
    unsigned long max_pfn;                  /* max pfn that appears in table */
    /* Frame containing list of mfns containing list of mfns containing p2m. */
    xen_pfn_t     pfn_to_mfn_frame_list_list;
    unsigned long nmi_reason;
    uint64_t pad[32];
};
typedef struct arch_shared_info arch_shared_info_t;

#endif /* !__ASSEMBLY__ */

/*
 * Prefix forces emulation of some non-trapping instructions.
 * Currently only CPUID.
 */
#ifdef __ASSEMBLY__
#define XEN_EMULATE_PREFIX .byte 0x0f,0x0b,0x78,0x65,0x6e ;
#define XEN_CPUID          XEN_EMULATE_PREFIX cpuid
#else
#define XEN_EMULATE_PREFIX ".byte 0x0f,0x0b,0x78,0x65,0x6e ; "
#define XEN_CPUID          XEN_EMULATE_PREFIX "cpuid"
#endif

#endif /* __XEN_PUBLIC_ARCH_X86_XEN_H__ */

/*
 * Local variables:
 * mode: C
 * c-set-style: "BSD"
 * c-basic-offset: 4
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
