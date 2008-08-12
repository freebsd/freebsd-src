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
 * Copyright (C) IBM Corp. 2005, 2006
 *
 * Authors: Hollis Blanchard <hollisb@us.ibm.com>
 */

#ifndef __XEN_PUBLIC_ARCH_PPC_64_H__
#define __XEN_PUBLIC_ARCH_PPC_64_H__

#define __DEFINE_XEN_GUEST_HANDLE(name, type) \
    typedef struct { \
        int __pad[(sizeof (long long) - sizeof (void *)) / sizeof (int)]; \
        type *p; \
    } __attribute__((__aligned__(8))) __guest_handle_ ## name

#define DEFINE_XEN_GUEST_HANDLE(name) __DEFINE_XEN_GUEST_HANDLE(name, name)
#define XEN_GUEST_HANDLE(name)        __guest_handle_ ## name
#define set_xen_guest_handle(hnd, val) \
    do { \
        if (sizeof ((hnd).__pad)) \
            (hnd).__pad[0] = 0; \
        (hnd).p = val; \
    } while (0)

#ifdef __XEN_TOOLS__
#define get_xen_guest_handle(val, hnd)  do { val = (hnd).p; } while (0)
#endif

#ifndef __ASSEMBLY__
/* Guest handles for primitive C types. */
__DEFINE_XEN_GUEST_HANDLE(uchar, unsigned char);
__DEFINE_XEN_GUEST_HANDLE(uint,  unsigned int);
__DEFINE_XEN_GUEST_HANDLE(ulong, unsigned long);
DEFINE_XEN_GUEST_HANDLE(char);
DEFINE_XEN_GUEST_HANDLE(int);
DEFINE_XEN_GUEST_HANDLE(long);
DEFINE_XEN_GUEST_HANDLE(void);

typedef unsigned long long xen_pfn_t;
DEFINE_XEN_GUEST_HANDLE(xen_pfn_t);
#define PRI_xen_pfn "llx"
#endif

/*
 * Pointers and other address fields inside interface structures are padded to
 * 64 bits. This means that field alignments aren't different between 32- and
 * 64-bit architectures. 
 */
/* NB. Multi-level macro ensures __LINE__ is expanded before concatenation. */
#define __MEMORY_PADDING(_X)
#define _MEMORY_PADDING(_X)  __MEMORY_PADDING(_X)
#define MEMORY_PADDING       _MEMORY_PADDING(__LINE__)

/* And the trap vector is... */
#define TRAP_INSTR "li 0,-1; sc" /* XXX just "sc"? */

#ifndef __ASSEMBLY__

#define XENCOMM_INLINE_FLAG (1UL << 63)

typedef uint64_t xen_ulong_t;

/* User-accessible registers: nost of these need to be saved/restored
 * for every nested Xen invocation. */
struct cpu_user_regs
{
    uint64_t gprs[32];
    uint64_t lr;
    uint64_t ctr;
    uint64_t srr0;
    uint64_t srr1;
    uint64_t pc;
    uint64_t msr;
    uint64_t fpscr;             /* XXX Is this necessary */
    uint64_t xer;
    uint64_t hid4;              /* debug only */
    uint64_t dar;               /* debug only */
    uint32_t dsisr;             /* debug only */
    uint32_t cr;
    uint32_t __pad;             /* good spot for another 32bit reg */
    uint32_t entry_vector;
};
typedef struct cpu_user_regs cpu_user_regs_t;

typedef uint64_t tsc_timestamp_t; /* RDTSC timestamp */ /* XXX timebase */

/* ONLY used to communicate with dom0! See also struct exec_domain. */
struct vcpu_guest_context {
    cpu_user_regs_t user_regs;         /* User-level CPU registers     */
    uint64_t sdr1;                     /* Pagetable base               */
    /* XXX etc */
};
typedef struct vcpu_guest_context vcpu_guest_context_t;
DEFINE_XEN_GUEST_HANDLE(vcpu_guest_context_t);

struct arch_shared_info {
    uint64_t boot_timebase;
};

struct arch_vcpu_info {
};

/* Support for multi-processor guests. */
#define MAX_VIRT_CPUS 32
#endif

#endif
