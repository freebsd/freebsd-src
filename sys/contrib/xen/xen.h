/******************************************************************************
 * xen.h
 *
 * Guest OS interface to Xen.
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
 * Copyright (c) 2004, K A Fraser
 */

#ifndef __XEN_PUBLIC_XEN_H__
#define __XEN_PUBLIC_XEN_H__

#include "xen-compat.h"

#if defined(__i386__) || defined(__x86_64__)
#include "arch-x86/xen.h"
#elif defined(__arm__) || defined (__aarch64__)
#include "arch-arm.h"
#else
#error "Unsupported architecture"
#endif

#ifndef __ASSEMBLY__
/* Guest handles for primitive C types. */
DEFINE_XEN_GUEST_HANDLE(char);
__DEFINE_XEN_GUEST_HANDLE(uchar, unsigned char);
DEFINE_XEN_GUEST_HANDLE(int);
__DEFINE_XEN_GUEST_HANDLE(uint,  unsigned int);
#if __XEN_INTERFACE_VERSION__ < 0x00040300
DEFINE_XEN_GUEST_HANDLE(long);
__DEFINE_XEN_GUEST_HANDLE(ulong, unsigned long);
#endif
DEFINE_XEN_GUEST_HANDLE(void);

DEFINE_XEN_GUEST_HANDLE(uint64_t);
DEFINE_XEN_GUEST_HANDLE(xen_pfn_t);
DEFINE_XEN_GUEST_HANDLE(xen_ulong_t);

/* Define a variable length array (depends on compiler). */
#if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 199901L
#define XEN_FLEX_ARRAY_DIM
#elif defined(__GNUC__)
#define XEN_FLEX_ARRAY_DIM  0
#else
#define XEN_FLEX_ARRAY_DIM  1 /* variable size */
#endif

/* Turn a plain number into a C unsigned (long (long)) constant. */
#define __xen_mk_uint(x)  x ## U
#define __xen_mk_ulong(x) x ## UL
#ifndef __xen_mk_ullong
# define __xen_mk_ullong(x) x ## ULL
#endif
#define xen_mk_uint(x)    __xen_mk_uint(x)
#define xen_mk_ulong(x)   __xen_mk_ulong(x)
#define xen_mk_ullong(x)  __xen_mk_ullong(x)

#else

/* In assembly code we cannot use C numeric constant suffixes. */
#define xen_mk_uint(x)   x
#define xen_mk_ulong(x)  x
#define xen_mk_ullong(x) x

#endif

/*
 * HYPERCALLS
 */

/* `incontents 100 hcalls List of hypercalls
 * ` enum hypercall_num { // __HYPERVISOR_* => HYPERVISOR_*()
 */

#define __HYPERVISOR_set_trap_table        0
#define __HYPERVISOR_mmu_update            1
#define __HYPERVISOR_set_gdt               2
#define __HYPERVISOR_stack_switch          3
#define __HYPERVISOR_set_callbacks         4
#define __HYPERVISOR_fpu_taskswitch        5
#define __HYPERVISOR_sched_op_compat       6 /* compat since 0x00030101 */
#define __HYPERVISOR_platform_op           7
#define __HYPERVISOR_set_debugreg          8
#define __HYPERVISOR_get_debugreg          9
#define __HYPERVISOR_update_descriptor    10
#define __HYPERVISOR_memory_op            12
#define __HYPERVISOR_multicall            13
#define __HYPERVISOR_update_va_mapping    14
#define __HYPERVISOR_set_timer_op         15
#define __HYPERVISOR_event_channel_op_compat 16 /* compat since 0x00030202 */
#define __HYPERVISOR_xen_version          17
#define __HYPERVISOR_console_io           18
#define __HYPERVISOR_physdev_op_compat    19 /* compat since 0x00030202 */
#define __HYPERVISOR_grant_table_op       20
#define __HYPERVISOR_vm_assist            21
#define __HYPERVISOR_update_va_mapping_otherdomain 22
#define __HYPERVISOR_iret                 23 /* x86 only */
#define __HYPERVISOR_vcpu_op              24
#define __HYPERVISOR_set_segment_base     25 /* x86/64 only */
#define __HYPERVISOR_mmuext_op            26
#define __HYPERVISOR_xsm_op               27
#define __HYPERVISOR_nmi_op               28
#define __HYPERVISOR_sched_op             29
#define __HYPERVISOR_callback_op          30
#define __HYPERVISOR_xenoprof_op          31
#define __HYPERVISOR_event_channel_op     32
#define __HYPERVISOR_physdev_op           33
#define __HYPERVISOR_hvm_op               34
#define __HYPERVISOR_sysctl               35
#define __HYPERVISOR_domctl               36
#define __HYPERVISOR_kexec_op             37
#define __HYPERVISOR_tmem_op              38
#define __HYPERVISOR_argo_op              39
#define __HYPERVISOR_xenpmu_op            40
#define __HYPERVISOR_dm_op                41
#define __HYPERVISOR_hypfs_op             42

/* Architecture-specific hypercall definitions. */
#define __HYPERVISOR_arch_0               48
#define __HYPERVISOR_arch_1               49
#define __HYPERVISOR_arch_2               50
#define __HYPERVISOR_arch_3               51
#define __HYPERVISOR_arch_4               52
#define __HYPERVISOR_arch_5               53
#define __HYPERVISOR_arch_6               54
#define __HYPERVISOR_arch_7               55

/* ` } */

/*
 * HYPERCALL COMPATIBILITY.
 */

/* New sched_op hypercall introduced in 0x00030101. */
#if __XEN_INTERFACE_VERSION__ < 0x00030101
#undef __HYPERVISOR_sched_op
#define __HYPERVISOR_sched_op __HYPERVISOR_sched_op_compat
#endif

/* New event-channel and physdev hypercalls introduced in 0x00030202. */
#if __XEN_INTERFACE_VERSION__ < 0x00030202
#undef __HYPERVISOR_event_channel_op
#define __HYPERVISOR_event_channel_op __HYPERVISOR_event_channel_op_compat
#undef __HYPERVISOR_physdev_op
#define __HYPERVISOR_physdev_op __HYPERVISOR_physdev_op_compat
#endif

/* New platform_op hypercall introduced in 0x00030204. */
#if __XEN_INTERFACE_VERSION__ < 0x00030204
#define __HYPERVISOR_dom0_op __HYPERVISOR_platform_op
#endif

/*
 * VIRTUAL INTERRUPTS
 *
 * Virtual interrupts that a guest OS may receive from Xen.
 *
 * In the side comments, 'V.' denotes a per-VCPU VIRQ while 'G.' denotes a
 * global VIRQ. The former can be bound once per VCPU and cannot be re-bound.
 * The latter can be allocated only once per guest: they must initially be
 * allocated to VCPU0 but can subsequently be re-bound.
 */
/* ` enum virq { */
#define VIRQ_TIMER      0  /* V. Timebase update, and/or requested timeout.  */
#define VIRQ_DEBUG      1  /* V. Request guest to dump debug info.           */
#define VIRQ_CONSOLE    2  /* G. (DOM0) Bytes received on emergency console. */
#define VIRQ_DOM_EXC    3  /* G. (DOM0) Exceptional event for some domain.   */
#define VIRQ_TBUF       4  /* G. (DOM0) Trace buffer has records available.  */
#define VIRQ_DEBUGGER   6  /* G. (DOM0) A domain has paused for debugging.   */
#define VIRQ_XENOPROF   7  /* V. XenOprofile interrupt: new sample available */
#define VIRQ_CON_RING   8  /* G. (DOM0) Bytes received on console            */
#define VIRQ_PCPU_STATE 9  /* G. (DOM0) PCPU state changed                   */
#define VIRQ_MEM_EVENT  10 /* G. (DOM0) A memory event has occurred          */
#define VIRQ_ARGO       11 /* G. Argo interdomain message notification       */
#define VIRQ_ENOMEM     12 /* G. (DOM0) Low on heap memory       */
#define VIRQ_XENPMU     13 /* V.  PMC interrupt                              */

/* Architecture-specific VIRQ definitions. */
#define VIRQ_ARCH_0    16
#define VIRQ_ARCH_1    17
#define VIRQ_ARCH_2    18
#define VIRQ_ARCH_3    19
#define VIRQ_ARCH_4    20
#define VIRQ_ARCH_5    21
#define VIRQ_ARCH_6    22
#define VIRQ_ARCH_7    23
/* ` } */

#define NR_VIRQS       24

/*
 * ` enum neg_errnoval
 * ` HYPERVISOR_mmu_update(const struct mmu_update reqs[],
 * `                       unsigned count, unsigned *done_out,
 * `                       unsigned foreigndom)
 * `
 * @reqs is an array of mmu_update_t structures ((ptr, val) pairs).
 * @count is the length of the above array.
 * @pdone is an output parameter indicating number of completed operations
 * @foreigndom[15:0]: FD, the expected owner of data pages referenced in this
 *                    hypercall invocation. Can be DOMID_SELF.
 * @foreigndom[31:16]: PFD, the expected owner of pagetable pages referenced
 *                     in this hypercall invocation. The value of this field
 *                     (x) encodes the PFD as follows:
 *                     x == 0 => PFD == DOMID_SELF
 *                     x != 0 => PFD == x - 1
 *
 * Sub-commands: ptr[1:0] specifies the appropriate MMU_* command.
 * -------------
 * ptr[1:0] == MMU_NORMAL_PT_UPDATE:
 * Updates an entry in a page table belonging to PFD. If updating an L1 table,
 * and the new table entry is valid/present, the mapped frame must belong to
 * FD. If attempting to map an I/O page then the caller assumes the privilege
 * of the FD.
 * FD == DOMID_IO: Permit /only/ I/O mappings, at the priv level of the caller.
 * FD == DOMID_XEN: Map restricted areas of Xen's heap space.
 * ptr[:2]  -- Machine address of the page-table entry to modify.
 * val      -- Value to write.
 *
 * There also certain implicit requirements when using this hypercall. The
 * pages that make up a pagetable must be mapped read-only in the guest.
 * This prevents uncontrolled guest updates to the pagetable. Xen strictly
 * enforces this, and will disallow any pagetable update which will end up
 * mapping pagetable page RW, and will disallow using any writable page as a
 * pagetable. In practice it means that when constructing a page table for a
 * process, thread, etc, we MUST be very dilligient in following these rules:
 *  1). Start with top-level page (PGD or in Xen language: L4). Fill out
 *      the entries.
 *  2). Keep on going, filling out the upper (PUD or L3), and middle (PMD
 *      or L2).
 *  3). Start filling out the PTE table (L1) with the PTE entries. Once
 *  	done, make sure to set each of those entries to RO (so writeable bit
 *  	is unset). Once that has been completed, set the PMD (L2) for this
 *  	PTE table as RO.
 *  4). When completed with all of the PMD (L2) entries, and all of them have
 *  	been set to RO, make sure to set RO the PUD (L3). Do the same
 *  	operation on PGD (L4) pagetable entries that have a PUD (L3) entry.
 *  5). Now before you can use those pages (so setting the cr3), you MUST also
 *      pin them so that the hypervisor can verify the entries. This is done
 *      via the HYPERVISOR_mmuext_op(MMUEXT_PIN_L4_TABLE, guest physical frame
 *      number of the PGD (L4)). And this point the HYPERVISOR_mmuext_op(
 *      MMUEXT_NEW_BASEPTR, guest physical frame number of the PGD (L4)) can be
 *      issued.
 * For 32-bit guests, the L4 is not used (as there is less pagetables), so
 * instead use L3.
 * At this point the pagetables can be modified using the MMU_NORMAL_PT_UPDATE
 * hypercall. Also if so desired the OS can also try to write to the PTE
 * and be trapped by the hypervisor (as the PTE entry is RO).
 *
 * To deallocate the pages, the operations are the reverse of the steps
 * mentioned above. The argument is MMUEXT_UNPIN_TABLE for all levels and the
 * pagetable MUST not be in use (meaning that the cr3 is not set to it).
 *
 * ptr[1:0] == MMU_MACHPHYS_UPDATE:
 * Updates an entry in the machine->pseudo-physical mapping table.
 * ptr[:2]  -- Machine address within the frame whose mapping to modify.
 *             The frame must belong to the FD, if one is specified.
 * val      -- Value to write into the mapping entry.
 *
 * ptr[1:0] == MMU_PT_UPDATE_PRESERVE_AD:
 * As MMU_NORMAL_PT_UPDATE above, but A/D bits currently in the PTE are ORed
 * with those in @val.
 *
 * ptr[1:0] == MMU_PT_UPDATE_NO_TRANSLATE:
 * As MMU_NORMAL_PT_UPDATE above, but @val is not translated though FD
 * page tables.
 *
 * @val is usually the machine frame number along with some attributes.
 * The attributes by default follow the architecture defined bits. Meaning that
 * if this is a X86_64 machine and four page table layout is used, the layout
 * of val is:
 *  - 63 if set means No execute (NX)
 *  - 46-13 the machine frame number
 *  - 12 available for guest
 *  - 11 available for guest
 *  - 10 available for guest
 *  - 9 available for guest
 *  - 8 global
 *  - 7 PAT (PSE is disabled, must use hypercall to make 4MB or 2MB pages)
 *  - 6 dirty
 *  - 5 accessed
 *  - 4 page cached disabled
 *  - 3 page write through
 *  - 2 userspace accessible
 *  - 1 writeable
 *  - 0 present
 *
 *  The one bits that does not fit with the default layout is the PAGE_PSE
 *  also called PAGE_PAT). The MMUEXT_[UN]MARK_SUPER arguments to the
 *  HYPERVISOR_mmuext_op serve as mechanism to set a pagetable to be 4MB
 *  (or 2MB) instead of using the PAGE_PSE bit.
 *
 *  The reason that the PAGE_PSE (bit 7) is not being utilized is due to Xen
 *  using it as the Page Attribute Table (PAT) bit - for details on it please
 *  refer to Intel SDM 10.12. The PAT allows to set the caching attributes of
 *  pages instead of using MTRRs.
 *
 *  The PAT MSR is as follows (it is a 64-bit value, each entry is 8 bits):
 *                    PAT4                 PAT0
 *  +-----+-----+----+----+----+-----+----+----+
 *  | UC  | UC- | WC | WB | UC | UC- | WC | WB |  <= Linux
 *  +-----+-----+----+----+----+-----+----+----+
 *  | UC  | UC- | WT | WB | UC | UC- | WT | WB |  <= BIOS (default when machine boots)
 *  +-----+-----+----+----+----+-----+----+----+
 *  | rsv | rsv | WP | WC | UC | UC- | WT | WB |  <= Xen
 *  +-----+-----+----+----+----+-----+----+----+
 *
 *  The lookup of this index table translates to looking up
 *  Bit 7, Bit 4, and Bit 3 of val entry:
 *
 *  PAT/PSE (bit 7) ... PCD (bit 4) .. PWT (bit 3).
 *
 *  If all bits are off, then we are using PAT0. If bit 3 turned on,
 *  then we are using PAT1, if bit 3 and bit 4, then PAT2..
 *
 *  As you can see, the Linux PAT1 translates to PAT4 under Xen. Which means
 *  that if a guest that follows Linux's PAT setup and would like to set Write
 *  Combined on pages it MUST use PAT4 entry. Meaning that Bit 7 (PAGE_PAT) is
 *  set. For example, under Linux it only uses PAT0, PAT1, and PAT2 for the
 *  caching as:
 *
 *   WB = none (so PAT0)
 *   WC = PWT (bit 3 on)
 *   UC = PWT | PCD (bit 3 and 4 are on).
 *
 * To make it work with Xen, it needs to translate the WC bit as so:
 *
 *  PWT (so bit 3 on) --> PAT (so bit 7 is on) and clear bit 3
 *
 * And to translate back it would:
 *
 * PAT (bit 7 on) --> PWT (bit 3 on) and clear bit 7.
 */
#define MMU_NORMAL_PT_UPDATE       0 /* checked '*ptr = val'. ptr is MA.      */
#define MMU_MACHPHYS_UPDATE        1 /* ptr = MA of frame to modify entry for */
#define MMU_PT_UPDATE_PRESERVE_AD  2 /* atomically: *ptr = val | (*ptr&(A|D)) */
#define MMU_PT_UPDATE_NO_TRANSLATE 3 /* checked '*ptr = val'. ptr is MA.      */
                                     /* val never translated.                 */

/*
 * MMU EXTENDED OPERATIONS
 *
 * ` enum neg_errnoval
 * ` HYPERVISOR_mmuext_op(mmuext_op_t uops[],
 * `                      unsigned int count,
 * `                      unsigned int *pdone,
 * `                      unsigned int foreigndom)
 */
/* HYPERVISOR_mmuext_op() accepts a list of mmuext_op structures.
 * A foreigndom (FD) can be specified (or DOMID_SELF for none).
 * Where the FD has some effect, it is described below.
 *
 * cmd: MMUEXT_(UN)PIN_*_TABLE
 * mfn: Machine frame number to be (un)pinned as a p.t. page.
 *      The frame must belong to the FD, if one is specified.
 *
 * cmd: MMUEXT_NEW_BASEPTR
 * mfn: Machine frame number of new page-table base to install in MMU.
 *
 * cmd: MMUEXT_NEW_USER_BASEPTR [x86/64 only]
 * mfn: Machine frame number of new page-table base to install in MMU
 *      when in user space.
 *
 * cmd: MMUEXT_TLB_FLUSH_LOCAL
 * No additional arguments. Flushes local TLB.
 *
 * cmd: MMUEXT_INVLPG_LOCAL
 * linear_addr: Linear address to be flushed from the local TLB.
 *
 * cmd: MMUEXT_TLB_FLUSH_MULTI
 * vcpumask: Pointer to bitmap of VCPUs to be flushed.
 *
 * cmd: MMUEXT_INVLPG_MULTI
 * linear_addr: Linear address to be flushed.
 * vcpumask: Pointer to bitmap of VCPUs to be flushed.
 *
 * cmd: MMUEXT_TLB_FLUSH_ALL
 * No additional arguments. Flushes all VCPUs' TLBs.
 *
 * cmd: MMUEXT_INVLPG_ALL
 * linear_addr: Linear address to be flushed from all VCPUs' TLBs.
 *
 * cmd: MMUEXT_FLUSH_CACHE
 * No additional arguments. Writes back and flushes cache contents.
 *
 * cmd: MMUEXT_FLUSH_CACHE_GLOBAL
 * No additional arguments. Writes back and flushes cache contents
 * on all CPUs in the system.
 *
 * cmd: MMUEXT_SET_LDT
 * linear_addr: Linear address of LDT base (NB. must be page-aligned).
 * nr_ents: Number of entries in LDT.
 *
 * cmd: MMUEXT_CLEAR_PAGE
 * mfn: Machine frame number to be cleared.
 *
 * cmd: MMUEXT_COPY_PAGE
 * mfn: Machine frame number of the destination page.
 * src_mfn: Machine frame number of the source page.
 *
 * cmd: MMUEXT_[UN]MARK_SUPER
 * mfn: Machine frame number of head of superpage to be [un]marked.
 */
/* ` enum mmuext_cmd { */
#define MMUEXT_PIN_L1_TABLE      0
#define MMUEXT_PIN_L2_TABLE      1
#define MMUEXT_PIN_L3_TABLE      2
#define MMUEXT_PIN_L4_TABLE      3
#define MMUEXT_UNPIN_TABLE       4
#define MMUEXT_NEW_BASEPTR       5
#define MMUEXT_TLB_FLUSH_LOCAL   6
#define MMUEXT_INVLPG_LOCAL      7
#define MMUEXT_TLB_FLUSH_MULTI   8
#define MMUEXT_INVLPG_MULTI      9
#define MMUEXT_TLB_FLUSH_ALL    10
#define MMUEXT_INVLPG_ALL       11
#define MMUEXT_FLUSH_CACHE      12
#define MMUEXT_SET_LDT          13
#define MMUEXT_NEW_USER_BASEPTR 15
#define MMUEXT_CLEAR_PAGE       16
#define MMUEXT_COPY_PAGE        17
#define MMUEXT_FLUSH_CACHE_GLOBAL 18
#define MMUEXT_MARK_SUPER       19
#define MMUEXT_UNMARK_SUPER     20
/* ` } */

#ifndef __ASSEMBLY__
struct mmuext_op {
    unsigned int cmd; /* => enum mmuext_cmd */
    union {
        /* [UN]PIN_TABLE, NEW_BASEPTR, NEW_USER_BASEPTR
         * CLEAR_PAGE, COPY_PAGE, [UN]MARK_SUPER */
        xen_pfn_t     mfn;
        /* INVLPG_LOCAL, INVLPG_ALL, SET_LDT */
        unsigned long linear_addr;
    } arg1;
    union {
        /* SET_LDT */
        unsigned int nr_ents;
        /* TLB_FLUSH_MULTI, INVLPG_MULTI */
#if __XEN_INTERFACE_VERSION__ >= 0x00030205
        XEN_GUEST_HANDLE(const_void) vcpumask;
#else
        const void *vcpumask;
#endif
        /* COPY_PAGE */
        xen_pfn_t src_mfn;
    } arg2;
};
typedef struct mmuext_op mmuext_op_t;
DEFINE_XEN_GUEST_HANDLE(mmuext_op_t);
#endif

/*
 * ` enum neg_errnoval
 * ` HYPERVISOR_update_va_mapping(unsigned long va, u64 val,
 * `                              enum uvm_flags flags)
 * `
 * ` enum neg_errnoval
 * ` HYPERVISOR_update_va_mapping_otherdomain(unsigned long va, u64 val,
 * `                                          enum uvm_flags flags,
 * `                                          domid_t domid)
 * `
 * ` @va: The virtual address whose mapping we want to change
 * ` @val: The new page table entry, must contain a machine address
 * ` @flags: Control TLB flushes
 */
/* These are passed as 'flags' to update_va_mapping. They can be ORed. */
/* When specifying UVMF_MULTI, also OR in a pointer to a CPU bitmap.   */
/* UVMF_LOCAL is merely UVMF_MULTI with a NULL bitmap pointer.         */
/* ` enum uvm_flags { */
#define UVMF_NONE           (xen_mk_ulong(0)<<0) /* No flushing at all.   */
#define UVMF_TLB_FLUSH      (xen_mk_ulong(1)<<0) /* Flush entire TLB(s).  */
#define UVMF_INVLPG         (xen_mk_ulong(2)<<0) /* Flush only one entry. */
#define UVMF_FLUSHTYPE_MASK (xen_mk_ulong(3)<<0)
#define UVMF_MULTI          (xen_mk_ulong(0)<<2) /* Flush subset of TLBs. */
#define UVMF_LOCAL          (xen_mk_ulong(0)<<2) /* Flush local TLB.      */
#define UVMF_ALL            (xen_mk_ulong(1)<<2) /* Flush all TLBs.       */
/* ` } */

/*
 * ` int
 * ` HYPERVISOR_console_io(unsigned int cmd,
 * `                       unsigned int count,
 * `                       char buffer[]);
 *
 * @cmd: Command (see below)
 * @count: Size of the buffer to read/write
 * @buffer: Pointer in the guest memory
 *
 * List of commands:
 *
 *  * CONSOLEIO_write: Write the buffer to Xen console.
 *      For the hardware domain, all the characters in the buffer will
 *      be written. Characters will be printed directly to the console.
 *      For all the other domains, only the printable characters will be
 *      written. Characters may be buffered until a newline (i.e '\n') is
 *      found.
 *      @return 0 on success, otherwise return an error code.
 *  * CONSOLEIO_read: Attempts to read up to @count characters from Xen
 *      console. The maximum buffer size (i.e. @count) supported is 2GB.
 *      @return the number of characters read on success, otherwise return
 *      an error code.
 */
#define CONSOLEIO_write         0
#define CONSOLEIO_read          1

/*
 * Commands to HYPERVISOR_vm_assist().
 */
#define VMASST_CMD_enable                0
#define VMASST_CMD_disable               1

/* x86/32 guests: simulate full 4GB segment limits. */
#define VMASST_TYPE_4gb_segments         0

/* x86/32 guests: trap (vector 15) whenever above vmassist is used. */
#define VMASST_TYPE_4gb_segments_notify  1

/*
 * x86 guests: support writes to bottom-level PTEs.
 * NB1. Page-directory entries cannot be written.
 * NB2. Guest must continue to remove all writable mappings of PTEs.
 */
#define VMASST_TYPE_writable_pagetables  2

/* x86/PAE guests: support PDPTs above 4GB. */
#define VMASST_TYPE_pae_extended_cr3     3

/*
 * x86 guests: Sane behaviour for virtual iopl
 *  - virtual iopl updated from do_iret() hypercalls.
 *  - virtual iopl reported in bounce frames.
 *  - guest kernels assumed to be level 0 for the purpose of iopl checks.
 */
#define VMASST_TYPE_architectural_iopl   4

/*
 * All guests: activate update indicator in vcpu_runstate_info
 * Enable setting the XEN_RUNSTATE_UPDATE flag in guest memory mapped
 * vcpu_runstate_info during updates of the runstate information.
 */
#define VMASST_TYPE_runstate_update_flag 5

/*
 * x86/64 guests: strictly hide M2P from user mode.
 * This allows the guest to control respective hypervisor behavior:
 * - when not set, L4 tables get created with the respective slot blank,
 *   and whenever the L4 table gets used as a kernel one the missing
 *   mapping gets inserted,
 * - when set, L4 tables get created with the respective slot initialized
 *   as before, and whenever the L4 table gets used as a user one the
 *   mapping gets zapped.
 */
#define VMASST_TYPE_m2p_strict           32

#if __XEN_INTERFACE_VERSION__ < 0x00040600
#define MAX_VMASST_TYPE                  3
#endif

/* Domain ids >= DOMID_FIRST_RESERVED cannot be used for ordinary domains. */
#define DOMID_FIRST_RESERVED xen_mk_uint(0x7FF0)

/* DOMID_SELF is used in certain contexts to refer to oneself. */
#define DOMID_SELF           xen_mk_uint(0x7FF0)

/*
 * DOMID_IO is used to restrict page-table updates to mapping I/O memory.
 * Although no Foreign Domain need be specified to map I/O pages, DOMID_IO
 * is useful to ensure that no mappings to the OS's own heap are accidentally
 * installed. (e.g., in Linux this could cause havoc as reference counts
 * aren't adjusted on the I/O-mapping code path).
 * This only makes sense as HYPERVISOR_mmu_update()'s and
 * HYPERVISOR_update_va_mapping_otherdomain()'s "foreigndom" argument. For
 * HYPERVISOR_mmu_update() context it can be specified by any calling domain,
 * otherwise it's only permitted if the caller is privileged.
 */
#define DOMID_IO             xen_mk_uint(0x7FF1)

/*
 * DOMID_XEN is used to allow privileged domains to map restricted parts of
 * Xen's heap space (e.g., the machine_to_phys table).
 * This only makes sense as
 * - HYPERVISOR_mmu_update()'s, HYPERVISOR_mmuext_op()'s, or
 *   HYPERVISOR_update_va_mapping_otherdomain()'s "foreigndom" argument,
 * - with XENMAPSPACE_gmfn_foreign,
 * and is only permitted if the caller is privileged.
 */
#define DOMID_XEN            xen_mk_uint(0x7FF2)

/*
 * DOMID_COW is used as the owner of sharable pages */
#define DOMID_COW            xen_mk_uint(0x7FF3)

/* DOMID_INVALID is used to identify pages with unknown owner. */
#define DOMID_INVALID        xen_mk_uint(0x7FF4)

/* Idle domain. */
#define DOMID_IDLE           xen_mk_uint(0x7FFF)

/* Mask for valid domain id values */
#define DOMID_MASK           xen_mk_uint(0x7FFF)

#ifndef __ASSEMBLY__

typedef uint16_t domid_t;

/*
 * Send an array of these to HYPERVISOR_mmu_update().
 * NB. The fields are natural pointer/address size for this architecture.
 */
struct mmu_update {
    uint64_t ptr;       /* Machine address of PTE. */
    uint64_t val;       /* New contents of PTE.    */
};
typedef struct mmu_update mmu_update_t;
DEFINE_XEN_GUEST_HANDLE(mmu_update_t);

/*
 * ` enum neg_errnoval
 * ` HYPERVISOR_multicall(multicall_entry_t call_list[],
 * `                      uint32_t nr_calls);
 *
 * NB. The fields are logically the natural register size for this
 * architecture. In cases where xen_ulong_t is larger than this then
 * any unused bits in the upper portion must be zero.
 */
struct multicall_entry {
    xen_ulong_t op, result;
    xen_ulong_t args[6];
};
typedef struct multicall_entry multicall_entry_t;
DEFINE_XEN_GUEST_HANDLE(multicall_entry_t);

#if __XEN_INTERFACE_VERSION__ < 0x00040400
/*
 * Event channel endpoints per domain (when using the 2-level ABI):
 *  1024 if a long is 32 bits; 4096 if a long is 64 bits.
 */
#define NR_EVENT_CHANNELS EVTCHN_2L_NR_CHANNELS
#endif

struct vcpu_time_info {
    /*
     * Updates to the following values are preceded and followed by an
     * increment of 'version'. The guest can therefore detect updates by
     * looking for changes to 'version'. If the least-significant bit of
     * the version number is set then an update is in progress and the guest
     * must wait to read a consistent set of values.
     * The correct way to interact with the version number is similar to
     * Linux's seqlock: see the implementations of read_seqbegin/read_seqretry.
     */
    uint32_t version;
    uint32_t pad0;
    uint64_t tsc_timestamp;   /* TSC at last update of time vals.  */
    uint64_t system_time;     /* Time, in nanosecs, since boot.    */
    /*
     * Current system time:
     *   system_time +
     *   ((((tsc - tsc_timestamp) << tsc_shift) * tsc_to_system_mul) >> 32)
     * CPU frequency (Hz):
     *   ((10^9 << 32) / tsc_to_system_mul) >> tsc_shift
     */
    uint32_t tsc_to_system_mul;
    int8_t   tsc_shift;
#if __XEN_INTERFACE_VERSION__ > 0x040600
    uint8_t  flags;
    uint8_t  pad1[2];
#else
    int8_t   pad1[3];
#endif
}; /* 32 bytes */
typedef struct vcpu_time_info vcpu_time_info_t;

#define XEN_PVCLOCK_TSC_STABLE_BIT     (1 << 0)
#define XEN_PVCLOCK_GUEST_STOPPED      (1 << 1)

struct vcpu_info {
    /*
     * 'evtchn_upcall_pending' is written non-zero by Xen to indicate
     * a pending notification for a particular VCPU. It is then cleared
     * by the guest OS /before/ checking for pending work, thus avoiding
     * a set-and-check race. Note that the mask is only accessed by Xen
     * on the CPU that is currently hosting the VCPU. This means that the
     * pending and mask flags can be updated by the guest without special
     * synchronisation (i.e., no need for the x86 LOCK prefix).
     * This may seem suboptimal because if the pending flag is set by
     * a different CPU then an IPI may be scheduled even when the mask
     * is set. However, note:
     *  1. The task of 'interrupt holdoff' is covered by the per-event-
     *     channel mask bits. A 'noisy' event that is continually being
     *     triggered can be masked at source at this very precise
     *     granularity.
     *  2. The main purpose of the per-VCPU mask is therefore to restrict
     *     reentrant execution: whether for concurrency control, or to
     *     prevent unbounded stack usage. Whatever the purpose, we expect
     *     that the mask will be asserted only for short periods at a time,
     *     and so the likelihood of a 'spurious' IPI is suitably small.
     * The mask is read before making an event upcall to the guest: a
     * non-zero mask therefore guarantees that the VCPU will not receive
     * an upcall activation. The mask is cleared when the VCPU requests
     * to block: this avoids wakeup-waiting races.
     */
    uint8_t evtchn_upcall_pending;
#ifdef XEN_HAVE_PV_UPCALL_MASK
    uint8_t evtchn_upcall_mask;
#else /* XEN_HAVE_PV_UPCALL_MASK */
    uint8_t pad0;
#endif /* XEN_HAVE_PV_UPCALL_MASK */
    xen_ulong_t evtchn_pending_sel;
    struct arch_vcpu_info arch;
    vcpu_time_info_t time;
}; /* 64 bytes (x86) */
#ifndef __XEN__
typedef struct vcpu_info vcpu_info_t;
#endif

/*
 * `incontents 200 startofday_shared Start-of-day shared data structure
 * Xen/kernel shared data -- pointer provided in start_info.
 *
 * This structure is defined to be both smaller than a page, and the
 * only data on the shared page, but may vary in actual size even within
 * compatible Xen versions; guests should not rely on the size
 * of this structure remaining constant.
 */
struct shared_info {
    struct vcpu_info vcpu_info[XEN_LEGACY_MAX_VCPUS];

    /*
     * A domain can create "event channels" on which it can send and receive
     * asynchronous event notifications. There are three classes of event that
     * are delivered by this mechanism:
     *  1. Bi-directional inter- and intra-domain connections. Domains must
     *     arrange out-of-band to set up a connection (usually by allocating
     *     an unbound 'listener' port and avertising that via a storage service
     *     such as xenstore).
     *  2. Physical interrupts. A domain with suitable hardware-access
     *     privileges can bind an event-channel port to a physical interrupt
     *     source.
     *  3. Virtual interrupts ('events'). A domain can bind an event-channel
     *     port to a virtual interrupt source, such as the virtual-timer
     *     device or the emergency console.
     *
     * Event channels are addressed by a "port index". Each channel is
     * associated with two bits of information:
     *  1. PENDING -- notifies the domain that there is a pending notification
     *     to be processed. This bit is cleared by the guest.
     *  2. MASK -- if this bit is clear then a 0->1 transition of PENDING
     *     will cause an asynchronous upcall to be scheduled. This bit is only
     *     updated by the guest. It is read-only within Xen. If a channel
     *     becomes pending while the channel is masked then the 'edge' is lost
     *     (i.e., when the channel is unmasked, the guest must manually handle
     *     pending notifications as no upcall will be scheduled by Xen).
     *
     * To expedite scanning of pending notifications, any 0->1 pending
     * transition on an unmasked channel causes a corresponding bit in a
     * per-vcpu selector word to be set. Each bit in the selector covers a
     * 'C long' in the PENDING bitfield array.
     */
    xen_ulong_t evtchn_pending[sizeof(xen_ulong_t) * 8];
    xen_ulong_t evtchn_mask[sizeof(xen_ulong_t) * 8];

    /*
     * Wallclock time: updated by control software or RTC emulation.
     * Guests should base their gettimeofday() syscall on this
     * wallclock-base value.
     * The values of wc_sec and wc_nsec are offsets from the Unix epoch
     * adjusted by the domain's 'time offset' (in seconds) as set either
     * by XEN_DOMCTL_settimeoffset, or adjusted via a guest write to the
     * emulated RTC.
     */
    uint32_t wc_version;      /* Version counter: see vcpu_time_info_t. */
    uint32_t wc_sec;
    uint32_t wc_nsec;
#if !defined(__i386__)
    uint32_t wc_sec_hi;
# define xen_wc_sec_hi wc_sec_hi
#elif !defined(__XEN__) && !defined(__XEN_TOOLS__)
# define xen_wc_sec_hi arch.wc_sec_hi
#endif

    struct arch_shared_info arch;

};
#ifndef __XEN__
typedef struct shared_info shared_info_t;
#endif

/*
 * `incontents 200 startofday Start-of-day memory layout
 *
 *  1. The domain is started within contiguous virtual-memory region.
 *  2. The contiguous region ends on an aligned 4MB boundary.
 *  3. This the order of bootstrap elements in the initial virtual region:
 *      a. relocated kernel image
 *      b. initial ram disk              [mod_start, mod_len]
 *         (may be omitted)
 *      c. list of allocated page frames [mfn_list, nr_pages]
 *         (unless relocated due to XEN_ELFNOTE_INIT_P2M)
 *      d. start_info_t structure        [register rSI (x86)]
 *         in case of dom0 this page contains the console info, too
 *      e. unless dom0: xenstore ring page
 *      f. unless dom0: console ring page
 *      g. bootstrap page tables         [pt_base and CR3 (x86)]
 *      h. bootstrap stack               [register ESP (x86)]
 *  4. Bootstrap elements are packed together, but each is 4kB-aligned.
 *  5. The list of page frames forms a contiguous 'pseudo-physical' memory
 *     layout for the domain. In particular, the bootstrap virtual-memory
 *     region is a 1:1 mapping to the first section of the pseudo-physical map.
 *  6. All bootstrap elements are mapped read-writable for the guest OS. The
 *     only exception is the bootstrap page table, which is mapped read-only.
 *  7. There is guaranteed to be at least 512kB padding after the final
 *     bootstrap element. If necessary, the bootstrap virtual region is
 *     extended by an extra 4MB to ensure this.
 *
 * Note: Prior to 25833:bb85bbccb1c9. ("x86/32-on-64 adjust Dom0 initial page
 * table layout") a bug caused the pt_base (3.g above) and cr3 to not point
 * to the start of the guest page tables (it was offset by two pages).
 * This only manifested itself on 32-on-64 dom0 kernels and not 32-on-64 domU
 * or 64-bit kernels of any colour. The page tables for a 32-on-64 dom0 got
 * allocated in the order: 'first L1','first L2', 'first L3', so the offset
 * to the page table base is by two pages back. The initial domain if it is
 * 32-bit and runs under a 64-bit hypervisor should _NOT_ use two of the
 * pages preceding pt_base and mark them as reserved/unused.
 */
#ifdef XEN_HAVE_PV_GUEST_ENTRY
struct start_info {
    /* THE FOLLOWING ARE FILLED IN BOTH ON INITIAL BOOT AND ON RESUME.    */
    char magic[32];             /* "xen-<version>-<platform>".            */
    unsigned long nr_pages;     /* Total pages allocated to this domain.  */
    unsigned long shared_info;  /* MACHINE address of shared info struct. */
    uint32_t flags;             /* SIF_xxx flags.                         */
    xen_pfn_t store_mfn;        /* MACHINE page number of shared page.    */
    uint32_t store_evtchn;      /* Event channel for store communication. */
    union {
        struct {
            xen_pfn_t mfn;      /* MACHINE page number of console page.   */
            uint32_t  evtchn;   /* Event channel for console page.        */
        } domU;
        struct {
            uint32_t info_off;  /* Offset of console_info struct.         */
            uint32_t info_size; /* Size of console_info struct from start.*/
        } dom0;
    } console;
    /* THE FOLLOWING ARE ONLY FILLED IN ON INITIAL BOOT (NOT RESUME).     */
    unsigned long pt_base;      /* VIRTUAL address of page directory.     */
    unsigned long nr_pt_frames; /* Number of bootstrap p.t. frames.       */
    unsigned long mfn_list;     /* VIRTUAL address of page-frame list.    */
    unsigned long mod_start;    /* VIRTUAL address of pre-loaded module   */
                                /* (PFN of pre-loaded module if           */
                                /*  SIF_MOD_START_PFN set in flags).      */
    unsigned long mod_len;      /* Size (bytes) of pre-loaded module.     */
#define MAX_GUEST_CMDLINE 1024
    int8_t cmd_line[MAX_GUEST_CMDLINE];
    /* The pfn range here covers both page table and p->m table frames.   */
    unsigned long first_p2m_pfn;/* 1st pfn forming initial P->M table.    */
    unsigned long nr_p2m_frames;/* # of pfns forming initial P->M table.  */
};
typedef struct start_info start_info_t;

/* New console union for dom0 introduced in 0x00030203. */
#if __XEN_INTERFACE_VERSION__ < 0x00030203
#define console_mfn    console.domU.mfn
#define console_evtchn console.domU.evtchn
#endif
#endif /* XEN_HAVE_PV_GUEST_ENTRY */

/* These flags are passed in the 'flags' field of start_info_t. */
#define SIF_PRIVILEGED    (1<<0)  /* Is the domain privileged? */
#define SIF_INITDOMAIN    (1<<1)  /* Is this the initial control domain? */
#define SIF_MULTIBOOT_MOD (1<<2)  /* Is mod_start a multiboot module? */
#define SIF_MOD_START_PFN (1<<3)  /* Is mod_start a PFN? */
#define SIF_VIRT_P2M_4TOOLS (1<<4) /* Do Xen tools understand a virt. mapped */
                                   /* P->M making the 3 level tree obsolete? */
#define SIF_PM_MASK       (0xFF<<8) /* reserve 1 byte for xen-pm options */

/*
 * A multiboot module is a package containing modules very similar to a
 * multiboot module array. The only differences are:
 * - the array of module descriptors is by convention simply at the beginning
 *   of the multiboot module,
 * - addresses in the module descriptors are based on the beginning of the
 *   multiboot module,
 * - the number of modules is determined by a termination descriptor that has
 *   mod_start == 0.
 *
 * This permits to both build it statically and reference it in a configuration
 * file, and let the PV guest easily rebase the addresses to virtual addresses
 * and at the same time count the number of modules.
 */
struct xen_multiboot_mod_list
{
    /* Address of first byte of the module */
    uint32_t mod_start;
    /* Address of last byte of the module (inclusive) */
    uint32_t mod_end;
    /* Address of zero-terminated command line */
    uint32_t cmdline;
    /* Unused, must be zero */
    uint32_t pad;
};
/*
 * `incontents 200 startofday_dom0_console Dom0_console
 *
 * The console structure in start_info.console.dom0
 *
 * This structure includes a variety of information required to
 * have a working VGA/VESA console.
 */
typedef struct dom0_vga_console_info {
    uint8_t video_type; /* DOM0_VGA_CONSOLE_??? */
#define XEN_VGATYPE_TEXT_MODE_3 0x03
#define XEN_VGATYPE_VESA_LFB    0x23
#define XEN_VGATYPE_EFI_LFB     0x70

    union {
        struct {
            /* Font height, in pixels. */
            uint16_t font_height;
            /* Cursor location (column, row). */
            uint16_t cursor_x, cursor_y;
            /* Number of rows and columns (dimensions in characters). */
            uint16_t rows, columns;
        } text_mode_3;

        struct {
            /* Width and height, in pixels. */
            uint16_t width, height;
            /* Bytes per scan line. */
            uint16_t bytes_per_line;
            /* Bits per pixel. */
            uint16_t bits_per_pixel;
            /* LFB physical address, and size (in units of 64kB). */
            uint32_t lfb_base;
            uint32_t lfb_size;
            /* RGB mask offsets and sizes, as defined by VBE 1.2+ */
            uint8_t  red_pos, red_size;
            uint8_t  green_pos, green_size;
            uint8_t  blue_pos, blue_size;
            uint8_t  rsvd_pos, rsvd_size;
#if __XEN_INTERFACE_VERSION__ >= 0x00030206
            /* VESA capabilities (offset 0xa, VESA command 0x4f00). */
            uint32_t gbl_caps;
            /* Mode attributes (offset 0x0, VESA command 0x4f01). */
            uint16_t mode_attrs;
            uint16_t pad;
#endif
#if __XEN_INTERFACE_VERSION__ >= 0x00040d00
            /* high 32 bits of lfb_base */
            uint32_t ext_lfb_base;
#endif
        } vesa_lfb;
    } u;
} dom0_vga_console_info_t;
#define xen_vga_console_info dom0_vga_console_info
#define xen_vga_console_info_t dom0_vga_console_info_t

typedef uint8_t xen_domain_handle_t[16];

__DEFINE_XEN_GUEST_HANDLE(uint8,  uint8_t);
__DEFINE_XEN_GUEST_HANDLE(uint16, uint16_t);
__DEFINE_XEN_GUEST_HANDLE(uint32, uint32_t);
__DEFINE_XEN_GUEST_HANDLE(uint64, uint64_t);

typedef struct {
    uint8_t a[16];
} xen_uuid_t;

/*
 * XEN_DEFINE_UUID(0x00112233, 0x4455, 0x6677, 0x8899,
 *                 0xaa, 0xbb, 0xcc, 0xdd, 0xee, 0xff)
 * will construct UUID 00112233-4455-6677-8899-aabbccddeeff presented as
 * {0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88,
 * 0x99, 0xaa, 0xbb, 0xcc, 0xdd, 0xee, 0xff};
 *
 * NB: This is compatible with Linux kernel and with libuuid, but it is not
 * compatible with Microsoft, as they use mixed-endian encoding (some
 * components are little-endian, some are big-endian).
 */
#define XEN_DEFINE_UUID_(a, b, c, d, e1, e2, e3, e4, e5, e6)            \
    {{((a) >> 24) & 0xFF, ((a) >> 16) & 0xFF,                           \
      ((a) >>  8) & 0xFF, ((a) >>  0) & 0xFF,                           \
      ((b) >>  8) & 0xFF, ((b) >>  0) & 0xFF,                           \
      ((c) >>  8) & 0xFF, ((c) >>  0) & 0xFF,                           \
      ((d) >>  8) & 0xFF, ((d) >>  0) & 0xFF,                           \
                e1, e2, e3, e4, e5, e6}}

#if defined(__STDC_VERSION__) ? __STDC_VERSION__ >= 199901L : defined(__GNUC__)
#define XEN_DEFINE_UUID(a, b, c, d, e1, e2, e3, e4, e5, e6)             \
    ((xen_uuid_t)XEN_DEFINE_UUID_(a, b, c, d, e1, e2, e3, e4, e5, e6))
#else
#define XEN_DEFINE_UUID(a, b, c, d, e1, e2, e3, e4, e5, e6)             \
    XEN_DEFINE_UUID_(a, b, c, d, e1, e2, e3, e4, e5, e6)
#endif /* __STDC_VERSION__ / __GNUC__ */

#endif /* !__ASSEMBLY__ */

/* Default definitions for macros used by domctl/sysctl. */
#if defined(__XEN__) || defined(__XEN_TOOLS__)

#ifndef int64_aligned_t
#define int64_aligned_t int64_t
#endif
#ifndef uint64_aligned_t
#define uint64_aligned_t uint64_t
#endif
#ifndef XEN_GUEST_HANDLE_64
#define XEN_GUEST_HANDLE_64(name) XEN_GUEST_HANDLE(name)
#endif

#ifndef __ASSEMBLY__
struct xenctl_bitmap {
    XEN_GUEST_HANDLE_64(uint8) bitmap;
    uint32_t nr_bits;
};
typedef struct xenctl_bitmap xenctl_bitmap_t;
#endif

#endif /* defined(__XEN__) || defined(__XEN_TOOLS__) */

#endif /* __XEN_PUBLIC_XEN_H__ */

/*
 * Local variables:
 * mode: C
 * c-file-style: "BSD"
 * c-basic-offset: 4
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
