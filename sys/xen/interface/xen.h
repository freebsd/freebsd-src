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

#include <xen/interface/xen-compat.h>

#if defined(__i386__) || defined(__x86_64__)
#include <xen/interface/arch-x86/xen.h>
#elif defined(__ia64__)
#include "arch-ia64.h"
#elif defined(__powerpc__)
#include "arch-powerpc.h"
#else
#error "Unsupported architecture"
#endif

/*
 * HYPERCALLS
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
#define __HYPERVISOR_acm_op               27
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

/* Architecture-specific hypercall definitions. */
#define __HYPERVISOR_arch_0               48
#define __HYPERVISOR_arch_1               49
#define __HYPERVISOR_arch_2               50
#define __HYPERVISOR_arch_3               51
#define __HYPERVISOR_arch_4               52
#define __HYPERVISOR_arch_5               53
#define __HYPERVISOR_arch_6               54
#define __HYPERVISOR_arch_7               55

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
#warning using compat ops
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
#define VIRQ_TIMER      0  /* V. Timebase update, and/or requested timeout.  */
#define VIRQ_DEBUG      1  /* V. Request guest to dump debug info.           */
#define VIRQ_CONSOLE    2  /* G. (DOM0) Bytes received on emergency console. */
#define VIRQ_DOM_EXC    3  /* G. (DOM0) Exceptional event for some domain.   */
#define VIRQ_TBUF       4  /* G. (DOM0) Trace buffer has records available.  */
#define VIRQ_DEBUGGER   6  /* G. (DOM0) A domain has paused for debugging.   */
#define VIRQ_XENOPROF   7  /* V. XenOprofile interrupt: new sample available */
#define VIRQ_CON_RING   8  /* G. (DOM0) Bytes received on console            */

/* Architecture-specific VIRQ definitions. */
#define VIRQ_ARCH_0    16
#define VIRQ_ARCH_1    17
#define VIRQ_ARCH_2    18
#define VIRQ_ARCH_3    19
#define VIRQ_ARCH_4    20
#define VIRQ_ARCH_5    21
#define VIRQ_ARCH_6    22
#define VIRQ_ARCH_7    23

#define NR_VIRQS       24

/*
 * MMU-UPDATE REQUESTS
 * 
 * HYPERVISOR_mmu_update() accepts a list of (ptr, val) pairs.
 * A foreigndom (FD) can be specified (or DOMID_SELF for none).
 * Where the FD has some effect, it is described below.
 * ptr[1:0] specifies the appropriate MMU_* command.
 * 
 * ptr[1:0] == MMU_NORMAL_PT_UPDATE:
 * Updates an entry in a page table. If updating an L1 table, and the new
 * table entry is valid/present, the mapped frame must belong to the FD, if
 * an FD has been specified. If attempting to map an I/O page then the
 * caller assumes the privilege of the FD.
 * FD == DOMID_IO: Permit /only/ I/O mappings, at the priv level of the caller.
 * FD == DOMID_XEN: Map restricted areas of Xen's heap space.
 * ptr[:2]  -- Machine address of the page-table entry to modify.
 * val      -- Value to write.
 * 
 * ptr[1:0] == MMU_MACHPHYS_UPDATE:
 * Updates an entry in the machine->pseudo-physical mapping table.
 * ptr[:2]  -- Machine address within the frame whose mapping to modify.
 *             The frame must belong to the FD, if one is specified.
 * val      -- Value to write into the mapping entry.
 */
#define MMU_NORMAL_PT_UPDATE     0 /* checked '*ptr = val'. ptr is MA.       */
#define MMU_MACHPHYS_UPDATE      1 /* ptr = MA of frame to modify entry for  */

/*
 * MMU EXTENDED OPERATIONS
 * 
 * HYPERVISOR_mmuext_op() accepts a list of mmuext_op structures.
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
 * cmd: MMUEXT_SET_LDT
 * linear_addr: Linear address of LDT base (NB. must be page-aligned).
 * nr_ents: Number of entries in LDT.
 */
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

#ifndef __ASSEMBLY__
struct mmuext_op {
    unsigned int cmd;
    union {
        /* [UN]PIN_TABLE, NEW_BASEPTR, NEW_USER_BASEPTR */
        xen_pfn_t     mfn;
        /* INVLPG_LOCAL, INVLPG_ALL, SET_LDT */
        unsigned long linear_addr;
    } arg1;
    union {
        /* SET_LDT */
        unsigned int nr_ents;
        /* TLB_FLUSH_MULTI, INVLPG_MULTI */
        XEN_GUEST_HANDLE_00030205(void) vcpumask;
    } arg2;
};
typedef struct mmuext_op mmuext_op_t;
DEFINE_XEN_GUEST_HANDLE(mmuext_op_t);
#endif

/* These are passed as 'flags' to update_va_mapping. They can be ORed. */
/* When specifying UVMF_MULTI, also OR in a pointer to a CPU bitmap.   */
/* UVMF_LOCAL is merely UVMF_MULTI with a NULL bitmap pointer.         */
#define UVMF_NONE               (0UL<<0) /* No flushing at all.   */
#define UVMF_TLB_FLUSH          (1UL<<0) /* Flush entire TLB(s).  */
#define UVMF_INVLPG             (2UL<<0) /* Flush only one entry. */
#define UVMF_FLUSHTYPE_MASK     (3UL<<0)
#define UVMF_MULTI              (0UL<<2) /* Flush subset of TLBs. */
#define UVMF_LOCAL              (0UL<<2) /* Flush local TLB.      */
#define UVMF_ALL                (1UL<<2) /* Flush all TLBs.       */

/*
 * Commands to HYPERVISOR_console_io().
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

#define MAX_VMASST_TYPE                  3

#ifndef __ASSEMBLY__

typedef uint16_t domid_t;

/* Domain ids >= DOMID_FIRST_RESERVED cannot be used for ordinary domains. */
#define DOMID_FIRST_RESERVED (0x7FF0U)

/* DOMID_SELF is used in certain contexts to refer to oneself. */
#define DOMID_SELF (0x7FF0U)

/*
 * DOMID_IO is used to restrict page-table updates to mapping I/O memory.
 * Although no Foreign Domain need be specified to map I/O pages, DOMID_IO
 * is useful to ensure that no mappings to the OS's own heap are accidentally
 * installed. (e.g., in Linux this could cause havoc as reference counts
 * aren't adjusted on the I/O-mapping code path).
 * This only makes sense in MMUEXT_SET_FOREIGNDOM, but in that context can
 * be specified by any calling domain.
 */
#define DOMID_IO   (0x7FF1U)

/*
 * DOMID_XEN is used to allow privileged domains to map restricted parts of
 * Xen's heap space (e.g., the machine_to_phys table).
 * This only makes sense in MMUEXT_SET_FOREIGNDOM, and is only permitted if
 * the caller is privileged.
 */
#define DOMID_XEN  (0x7FF2U)

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
 * Send an array of these to HYPERVISOR_multicall().
 * NB. The fields are natural register size for this architecture.
 */
struct multicall_entry {
    unsigned long op, result;
    unsigned long args[6];
};
typedef struct multicall_entry multicall_entry_t;
DEFINE_XEN_GUEST_HANDLE(multicall_entry_t);

/*
 * Event channel endpoints per domain:
 *  1024 if a long is 32 bits; 4096 if a long is 64 bits.
 */
#define NR_EVENT_CHANNELS (sizeof(unsigned long) * sizeof(unsigned long) * 64)

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
    int8_t   pad1[3];
}; /* 32 bytes */
typedef struct vcpu_time_info vcpu_time_info_t;

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
    uint8_t evtchn_upcall_mask;
    unsigned long evtchn_pending_sel;
    struct arch_vcpu_info arch;
    struct vcpu_time_info time;
}; /* 64 bytes (x86) */
#ifndef __XEN__
typedef struct vcpu_info vcpu_info_t;
#endif

/*
 * Xen/kernel shared data -- pointer provided in start_info.
 *
 * This structure is defined to be both smaller than a page, and the
 * only data on the shared page, but may vary in actual size even within
 * compatible Xen versions; guests should not rely on the size
 * of this structure remaining constant.
 */
struct shared_info {
    struct vcpu_info vcpu_info[MAX_VIRT_CPUS];

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
    unsigned long evtchn_pending[sizeof(unsigned long) * 8];
    unsigned long evtchn_mask[sizeof(unsigned long) * 8];

    /*
     * Wallclock time: updated only by control software. Guests should base
     * their gettimeofday() syscall on this wallclock-base value.
     */
    uint32_t wc_version;      /* Version counter: see vcpu_time_info_t. */
    uint32_t wc_sec;          /* Secs  00:00:00 UTC, Jan 1, 1970.  */
    uint32_t wc_nsec;         /* Nsecs 00:00:00 UTC, Jan 1, 1970.  */

    struct arch_shared_info arch;

};
#ifndef __XEN__
typedef struct shared_info shared_info_t;
#endif

/*
 * Start-of-day memory layout:
 *  1. The domain is started within contiguous virtual-memory region.
 *  2. The contiguous region ends on an aligned 4MB boundary.
 *  3. This the order of bootstrap elements in the initial virtual region:
 *      a. relocated kernel image
 *      b. initial ram disk              [mod_start, mod_len]
 *      c. list of allocated page frames [mfn_list, nr_pages]
 *      d. start_info_t structure        [register ESI (x86)]
 *      e. bootstrap page tables         [pt_base, CR3 (x86)]
 *      f. bootstrap stack               [register ESP (x86)]
 *  4. Bootstrap elements are packed together, but each is 4kB-aligned.
 *  5. The initial ram disk may be omitted.
 *  6. The list of page frames forms a contiguous 'pseudo-physical' memory
 *     layout for the domain. In particular, the bootstrap virtual-memory
 *     region is a 1:1 mapping to the first section of the pseudo-physical map.
 *  7. All bootstrap elements are mapped read-writable for the guest OS. The
 *     only exception is the bootstrap page table, which is mapped read-only.
 *  8. There is guaranteed to be at least 512kB padding after the final
 *     bootstrap element. If necessary, the bootstrap virtual region is
 *     extended by an extra 4MB to ensure this.
 */

#define MAX_GUEST_CMDLINE 1024
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
    unsigned long mod_start;    /* VIRTUAL address of pre-loaded module.  */
    unsigned long mod_len;      /* Size (bytes) of pre-loaded module.     */
    int8_t cmd_line[MAX_GUEST_CMDLINE];
};
typedef struct start_info start_info_t;

/* New console union for dom0 introduced in 0x00030203. */
#if __XEN_INTERFACE_VERSION__ < 0x00030203
#define console_mfn    console.domU.mfn
#define console_evtchn console.domU.evtchn
#endif

/* These flags are passed in the 'flags' field of start_info_t. */
#define SIF_PRIVILEGED    (1<<0)  /* Is the domain privileged? */
#define SIF_INITDOMAIN    (1<<1)  /* Is this the initial control domain? */

typedef struct dom0_vga_console_info {
    uint8_t video_type; /* DOM0_VGA_CONSOLE_??? */
#define XEN_VGATYPE_TEXT_MODE_3 0x03
#define XEN_VGATYPE_VESA_LFB    0x23

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
        } vesa_lfb;
    } u;
} dom0_vga_console_info_t;
#define xen_vga_console_info dom0_vga_console_info
#define xen_vga_console_info_t dom0_vga_console_info_t

typedef uint8_t xen_domain_handle_t[16];

/* Turn a plain number into a C unsigned long constant. */
#define __mk_unsigned_long(x) x ## UL
#define mk_unsigned_long(x) __mk_unsigned_long(x)

DEFINE_XEN_GUEST_HANDLE(uint8_t);
DEFINE_XEN_GUEST_HANDLE(uint16_t);
DEFINE_XEN_GUEST_HANDLE(uint32_t);
DEFINE_XEN_GUEST_HANDLE(uint64_t);

#else /* __ASSEMBLY__ */

/* In assembly code we cannot use C numeric constant suffixes. */
#define mk_unsigned_long(x) x

#endif /* !__ASSEMBLY__ */

/* Default definitions for macros used by domctl/sysctl. */
#if defined(__XEN__) || defined(__XEN_TOOLS__)
#ifndef uint64_aligned_t
#define uint64_aligned_t uint64_t
#endif
#ifndef XEN_GUEST_HANDLE_64
#define XEN_GUEST_HANDLE_64(name) XEN_GUEST_HANDLE(name)
#endif
#endif

#endif /* __XEN_PUBLIC_XEN_H__ */

/*
 * Local variables:
 * mode: C
 * c-set-style: "BSD"
 * c-basic-offset: 4
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
