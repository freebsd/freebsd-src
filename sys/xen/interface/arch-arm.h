/******************************************************************************
 * arch-arm.h
 *
 * Guest OS interface to ARM Xen.
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
 * Copyright 2011 (C) Citrix Systems
 */

#ifndef __XEN_PUBLIC_ARCH_ARM_H__
#define __XEN_PUBLIC_ARCH_ARM_H__

/* hypercall calling convention
 * ----------------------------
 *
 * A hypercall is issued using the ARM HVC instruction.
 *
 * A hypercall can take up to 5 arguments. These are passed in
 * registers, the first argument in r0, the second argument in r1, the
 * third in r2, the forth in r3 and the fifth in r4.
 *
 * The hypercall number is passed in r12.
 *
 * The HVC ISS must contain a Xen specific TAG: XEN_HYPERCALL_TAG.
 *
 * The return value is in r0.
 *
 * The hypercall will clobber r12 and the argument registers used by
 * that hypercall (except r0 which is the return value) i.e. a 2
 * argument hypercall will clobber r1 and a 4 argument hypercall will
 * clobber r1, r2 and r3.
 *
 */

#define XEN_HYPERCALL_TAG   0XEA1


#ifndef __ASSEMBLY__
#define ___DEFINE_XEN_GUEST_HANDLE(name, type) \
    typedef struct { type *p; } __guest_handle_ ## name

#define __DEFINE_XEN_GUEST_HANDLE(name, type) \
    ___DEFINE_XEN_GUEST_HANDLE(name, type);   \
    ___DEFINE_XEN_GUEST_HANDLE(const_##name, const type)
#define DEFINE_XEN_GUEST_HANDLE(name)   __DEFINE_XEN_GUEST_HANDLE(name, name)
#define __XEN_GUEST_HANDLE(name)        __guest_handle_ ## name
#define XEN_GUEST_HANDLE(name)          __XEN_GUEST_HANDLE(name)
#define set_xen_guest_handle_raw(hnd, val)  do { (hnd).p = val; } while (0)
#ifdef __XEN_TOOLS__
#define get_xen_guest_handle(val, hnd)  do { val = (hnd).p; } while (0)
#endif
#define set_xen_guest_handle(hnd, val) set_xen_guest_handle_raw(hnd, val)

struct cpu_user_regs
{
    uint32_t r0;
    uint32_t r1;
    uint32_t r2;
    uint32_t r3;
    uint32_t r4;
    uint32_t r5;
    uint32_t r6;
    uint32_t r7;
    uint32_t r8;
    uint32_t r9;
    uint32_t r10;
    union {
        uint32_t r11;
        uint32_t fp;
    };
    uint32_t r12;

    uint32_t sp; /* r13 - SP: Valid for Hyp. frames only, o/w banked (see below) */

    /* r14 - LR: is the same physical register as LR_usr */
    union {
        uint32_t lr; /* r14 - LR: Valid for Hyp. Same physical register as lr_usr. */
        uint32_t lr_usr;
    };

    uint32_t pc; /* Return IP */
    uint32_t cpsr; /* Return mode */
    uint32_t pad0; /* Doubleword-align the kernel half of the frame */

    /* Outer guest frame only from here on... */

    uint32_t r8_fiq, r9_fiq, r10_fiq, r11_fiq, r12_fiq;

    uint32_t sp_usr; /* LR_usr is the same register as LR, see above */

    uint32_t sp_svc, sp_abt, sp_und, sp_irq, sp_fiq;
    uint32_t lr_svc, lr_abt, lr_und, lr_irq, lr_fiq;

    uint32_t spsr_svc, spsr_abt, spsr_und, spsr_irq, spsr_fiq;

    uint32_t pad1; /* Doubleword-align the user half of the frame */
};
typedef struct cpu_user_regs cpu_user_regs_t;
DEFINE_XEN_GUEST_HANDLE(cpu_user_regs_t);

typedef uint64_t xen_pfn_t;
#define PRI_xen_pfn PRIx64

/* Maximum number of virtual CPUs in legacy multi-processor guests. */
/* Only one. All other VCPUS must use VCPUOP_register_vcpu_info */
#define XEN_LEGACY_MAX_VCPUS 1

typedef uint32_t xen_ulong_t;

struct vcpu_guest_context {
    struct cpu_user_regs user_regs;         /* User-level CPU registers     */

    uint32_t sctlr;
    uint32_t ttbr0, ttbr1, ttbcr;
};
typedef struct vcpu_guest_context vcpu_guest_context_t;
DEFINE_XEN_GUEST_HANDLE(vcpu_guest_context_t);

struct arch_vcpu_info { };
typedef struct arch_vcpu_info arch_vcpu_info_t;

struct arch_shared_info { };
typedef struct arch_shared_info arch_shared_info_t;
typedef uint64_t xen_callback_t;

#endif /* ifndef __ASSEMBLY __ */

/* PSR bits (CPSR, SPSR)*/

/* 0-4: Mode */
#define PSR_MODE_MASK 0x1f
#define PSR_MODE_USR 0x10
#define PSR_MODE_FIQ 0x11
#define PSR_MODE_IRQ 0x12
#define PSR_MODE_SVC 0x13
#define PSR_MODE_MON 0x16
#define PSR_MODE_ABT 0x17
#define PSR_MODE_HYP 0x1a
#define PSR_MODE_UND 0x1b
#define PSR_MODE_SYS 0x1f

#define PSR_THUMB       (1<<5)        /* Thumb Mode enable */
#define PSR_FIQ_MASK    (1<<6)        /* Fast Interrupt mask */
#define PSR_IRQ_MASK    (1<<7)        /* Interrupt mask */
#define PSR_ABT_MASK    (1<<8)        /* Asynchronous Abort mask */
#define PSR_BIG_ENDIAN  (1<<9)        /* Big Endian Mode */
#define PSR_JAZELLE     (1<<24)       /* Jazelle Mode */

#endif /*  __XEN_PUBLIC_ARCH_ARM_H__ */

/*
 * Local variables:
 * mode: C
 * c-set-style: "BSD"
 * c-basic-offset: 4
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
