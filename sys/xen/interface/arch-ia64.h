/******************************************************************************
 * arch-ia64/hypervisor-if.h
 * 
 * Guest OS interface to IA64 Xen.
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
 */

#ifndef __HYPERVISOR_IF_IA64_H__
#define __HYPERVISOR_IF_IA64_H__

/* Structural guest handles introduced in 0x00030201. */
#if __XEN_INTERFACE_VERSION__ >= 0x00030201
#define __DEFINE_XEN_GUEST_HANDLE(name, type) \
    typedef struct { type *p; } __guest_handle_ ## name
#else
#define __DEFINE_XEN_GUEST_HANDLE(name, type) \
    typedef type * __guest_handle_ ## name
#endif

#define DEFINE_XEN_GUEST_HANDLE(name)   __DEFINE_XEN_GUEST_HANDLE(name, name)
#define XEN_GUEST_HANDLE(name)          __guest_handle_ ## name
#define XEN_GUEST_HANDLE_64(name)       XEN_GUEST_HANDLE(name)
#define uint64_aligned_t                uint64_t
#define set_xen_guest_handle(hnd, val)  do { (hnd).p = val; } while (0)
#ifdef __XEN_TOOLS__
#define get_xen_guest_handle(val, hnd)  do { val = (hnd).p; } while (0)
#endif

#ifndef __ASSEMBLY__
/* Guest handles for primitive C types. */
__DEFINE_XEN_GUEST_HANDLE(uchar, unsigned char);
__DEFINE_XEN_GUEST_HANDLE(uint,  unsigned int);
__DEFINE_XEN_GUEST_HANDLE(ulong, unsigned long);
__DEFINE_XEN_GUEST_HANDLE(u64,   unsigned long);
DEFINE_XEN_GUEST_HANDLE(char);
DEFINE_XEN_GUEST_HANDLE(int);
DEFINE_XEN_GUEST_HANDLE(long);
DEFINE_XEN_GUEST_HANDLE(void);

typedef unsigned long xen_pfn_t;
DEFINE_XEN_GUEST_HANDLE(xen_pfn_t);
#define PRI_xen_pfn "lx"
#endif

/* Arch specific VIRQs definition */
#define VIRQ_ITC        VIRQ_ARCH_0 /* V. Virtual itc timer */
#define VIRQ_MCA_CMC    VIRQ_ARCH_1 /* MCA cmc interrupt */
#define VIRQ_MCA_CPE    VIRQ_ARCH_2 /* MCA cpe interrupt */

/* Maximum number of virtual CPUs in multi-processor guests. */
/* WARNING: before changing this, check that shared_info fits on a page */
#define MAX_VIRT_CPUS 64

#ifndef __ASSEMBLY__

typedef unsigned long xen_ulong_t;

#define INVALID_MFN       (~0UL)

#define MEM_G   (1UL << 30)
#define MEM_M   (1UL << 20)
#define MEM_K   (1UL << 10)

#define MMIO_START       (3 * MEM_G)
#define MMIO_SIZE        (512 * MEM_M)

#define VGA_IO_START     0xA0000UL
#define VGA_IO_SIZE      0x20000

#define LEGACY_IO_START  (MMIO_START + MMIO_SIZE)
#define LEGACY_IO_SIZE   (64*MEM_M)

#define IO_PAGE_START (LEGACY_IO_START + LEGACY_IO_SIZE)
#define IO_PAGE_SIZE  PAGE_SIZE

#define STORE_PAGE_START (IO_PAGE_START + IO_PAGE_SIZE)
#define STORE_PAGE_SIZE  PAGE_SIZE

#define BUFFER_IO_PAGE_START (STORE_PAGE_START+STORE_PAGE_SIZE)
#define BUFFER_IO_PAGE_SIZE PAGE_SIZE

#define BUFFER_PIO_PAGE_START (BUFFER_IO_PAGE_START+BUFFER_IO_PAGE_SIZE)
#define BUFFER_PIO_PAGE_SIZE PAGE_SIZE

#define IO_SAPIC_START   0xfec00000UL
#define IO_SAPIC_SIZE    0x100000

#define PIB_START 0xfee00000UL
#define PIB_SIZE 0x200000

#define GFW_START        (4*MEM_G -16*MEM_M)
#define GFW_SIZE         (16*MEM_M)

/* Nvram belongs to GFW memory space  */
#define NVRAM_SIZE       (MEM_K * 64)
#define NVRAM_START      (GFW_START + 10 * MEM_M)

#define NVRAM_VALID_SIG 0x4650494e45584948 		// "HIXENIPF"
struct nvram_save_addr {
    unsigned long addr;
    unsigned long signature;
};

struct pt_fpreg {
    union {
        unsigned long bits[2];
        long double __dummy;    /* force 16-byte alignment */
    } u;
};

struct cpu_user_regs {
    /* The following registers are saved by SAVE_MIN: */
    unsigned long b6;  /* scratch */
    unsigned long b7;  /* scratch */

    unsigned long ar_csd; /* used by cmp8xchg16 (scratch) */
    unsigned long ar_ssd; /* reserved for future use (scratch) */

    unsigned long r8;  /* scratch (return value register 0) */
    unsigned long r9;  /* scratch (return value register 1) */
    unsigned long r10; /* scratch (return value register 2) */
    unsigned long r11; /* scratch (return value register 3) */

    unsigned long cr_ipsr; /* interrupted task's psr */
    unsigned long cr_iip;  /* interrupted task's instruction pointer */
    unsigned long cr_ifs;  /* interrupted task's function state */

    unsigned long ar_unat; /* interrupted task's NaT register (preserved) */
    unsigned long ar_pfs;  /* prev function state  */
    unsigned long ar_rsc;  /* RSE configuration */
    /* The following two are valid only if cr_ipsr.cpl > 0: */
    unsigned long ar_rnat;  /* RSE NaT */
    unsigned long ar_bspstore; /* RSE bspstore */

    unsigned long pr;  /* 64 predicate registers (1 bit each) */
    unsigned long b0;  /* return pointer (bp) */
    unsigned long loadrs;  /* size of dirty partition << 16 */

    unsigned long r1;  /* the gp pointer */
    unsigned long r12; /* interrupted task's memory stack pointer */
    unsigned long r13; /* thread pointer */

    unsigned long ar_fpsr;  /* floating point status (preserved) */
    unsigned long r15;  /* scratch */

 /* The remaining registers are NOT saved for system calls.  */

    unsigned long r14;  /* scratch */
    unsigned long r2;  /* scratch */
    unsigned long r3;  /* scratch */
    unsigned long r16;  /* scratch */
    unsigned long r17;  /* scratch */
    unsigned long r18;  /* scratch */
    unsigned long r19;  /* scratch */
    unsigned long r20;  /* scratch */
    unsigned long r21;  /* scratch */
    unsigned long r22;  /* scratch */
    unsigned long r23;  /* scratch */
    unsigned long r24;  /* scratch */
    unsigned long r25;  /* scratch */
    unsigned long r26;  /* scratch */
    unsigned long r27;  /* scratch */
    unsigned long r28;  /* scratch */
    unsigned long r29;  /* scratch */
    unsigned long r30;  /* scratch */
    unsigned long r31;  /* scratch */
    unsigned long ar_ccv;  /* compare/exchange value (scratch) */

    /*
     * Floating point registers that the kernel considers scratch:
     */
    struct pt_fpreg f6;  /* scratch */
    struct pt_fpreg f7;  /* scratch */
    struct pt_fpreg f8;  /* scratch */
    struct pt_fpreg f9;  /* scratch */
    struct pt_fpreg f10;  /* scratch */
    struct pt_fpreg f11;  /* scratch */
    unsigned long r4;  /* preserved */
    unsigned long r5;  /* preserved */
    unsigned long r6;  /* preserved */
    unsigned long r7;  /* preserved */
    unsigned long eml_unat;    /* used for emulating instruction */
    unsigned long pad0;     /* alignment pad */

};
typedef struct cpu_user_regs cpu_user_regs_t;

union vac {
    unsigned long value;
    struct {
        int a_int:1;
        int a_from_int_cr:1;
        int a_to_int_cr:1;
        int a_from_psr:1;
        int a_from_cpuid:1;
        int a_cover:1;
        int a_bsw:1;
        long reserved:57;
    };
};
typedef union vac vac_t;

union vdc {
    unsigned long value;
    struct {
        int d_vmsw:1;
        int d_extint:1;
        int d_ibr_dbr:1;
        int d_pmc:1;
        int d_to_pmd:1;
        int d_itm:1;
        long reserved:58;
    };
};
typedef union vdc vdc_t;

struct mapped_regs {
    union vac   vac;
    union vdc   vdc;
    unsigned long  virt_env_vaddr;
    unsigned long  reserved1[29];
    unsigned long  vhpi;
    unsigned long  reserved2[95];
    union {
        unsigned long  vgr[16];
        unsigned long bank1_regs[16]; // bank1 regs (r16-r31) when bank0 active
    };
    union {
        unsigned long  vbgr[16];
        unsigned long bank0_regs[16]; // bank0 regs (r16-r31) when bank1 active
    };
    unsigned long  vnat;
    unsigned long  vbnat;
    unsigned long  vcpuid[5];
    unsigned long  reserved3[11];
    unsigned long  vpsr;
    unsigned long  vpr;
    unsigned long  reserved4[76];
    union {
        unsigned long  vcr[128];
        struct {
            unsigned long dcr;  // CR0
            unsigned long itm;
            unsigned long iva;
            unsigned long rsv1[5];
            unsigned long pta;  // CR8
            unsigned long rsv2[7];
            unsigned long ipsr;  // CR16
            unsigned long isr;
            unsigned long rsv3;
            unsigned long iip;
            unsigned long ifa;
            unsigned long itir;
            unsigned long iipa;
            unsigned long ifs;
            unsigned long iim;  // CR24
            unsigned long iha;
            unsigned long rsv4[38];
            unsigned long lid;  // CR64
            unsigned long ivr;
            unsigned long tpr;
            unsigned long eoi;
            unsigned long irr[4];
            unsigned long itv;  // CR72
            unsigned long pmv;
            unsigned long cmcv;
            unsigned long rsv5[5];
            unsigned long lrr0;  // CR80
            unsigned long lrr1;
            unsigned long rsv6[46];
        };
    };
    union {
        unsigned long  reserved5[128];
        struct {
            unsigned long precover_ifs;
            unsigned long unat;  // not sure if this is needed until NaT arch is done
            int interrupt_collection_enabled; // virtual psr.ic
            /* virtual interrupt deliverable flag is evtchn_upcall_mask in
             * shared info area now. interrupt_mask_addr is the address
             * of evtchn_upcall_mask for current vcpu
             */
            unsigned char *interrupt_mask_addr;
            int pending_interruption;
            unsigned char vpsr_pp;
            unsigned char vpsr_dfh;
            unsigned char hpsr_dfh;
            unsigned char hpsr_mfh;
            unsigned long reserved5_1[4];
            int metaphysical_mode; // 1 = use metaphys mapping, 0 = use virtual
            int banknum; // 0 or 1, which virtual register bank is active
            unsigned long rrs[8]; // region registers
            unsigned long krs[8]; // kernel registers
            unsigned long pkrs[8]; // protection key registers
            unsigned long tmp[8]; // temp registers (e.g. for hyperprivops)
        };
    };
};
typedef struct mapped_regs mapped_regs_t;

struct vpd {
    struct mapped_regs vpd_low;
    unsigned long  reserved6[3456];
    unsigned long  vmm_avail[128];
    unsigned long  reserved7[4096];
};
typedef struct vpd vpd_t;

struct arch_vcpu_info {
};
typedef struct arch_vcpu_info arch_vcpu_info_t;

struct arch_shared_info {
    /* PFN of the start_info page.  */
    unsigned long start_info_pfn;

    /* Interrupt vector for event channel.  */
    int evtchn_vector;

    uint64_t pad[32];
};
typedef struct arch_shared_info arch_shared_info_t;

typedef unsigned long xen_callback_t;

struct ia64_tr_entry {
    unsigned long pte;
    unsigned long itir;
    unsigned long vadr;
    unsigned long rid;
};

struct vcpu_extra_regs {
    struct ia64_tr_entry itrs[8];
    struct ia64_tr_entry dtrs[8];
    unsigned long iva;
    unsigned long dcr;
    unsigned long event_callback_ip;
};

struct vcpu_guest_context {
#define VGCF_EXTRA_REGS (1<<1)	/* Get/Set extra regs.  */
    unsigned long flags;       /* VGCF_* flags */

    struct cpu_user_regs user_regs;
    struct vcpu_extra_regs extra_regs;
    unsigned long privregs_pfn;
};
typedef struct vcpu_guest_context vcpu_guest_context_t;
DEFINE_XEN_GUEST_HANDLE(vcpu_guest_context_t);

/* dom0 vp op */
#define __HYPERVISOR_ia64_dom0vp_op     __HYPERVISOR_arch_0
/*  Map io space in machine address to dom0 physical address space.
    Currently physical assigned address equals to machine address.  */
#define IA64_DOM0VP_ioremap             0

/* Convert a pseudo physical page frame number to the corresponding
   machine page frame number. If no page is assigned, INVALID_MFN or
   GPFN_INV_MASK is returned depending on domain's non-vti/vti mode.  */
#define IA64_DOM0VP_phystomach          1

/* Convert a machine page frame number to the corresponding pseudo physical
   page frame number of the caller domain.  */
#define IA64_DOM0VP_machtophys          3

/* Reserved for future use.  */
#define IA64_DOM0VP_iounmap             4

/* Unmap and free pages contained in the specified pseudo physical region.  */
#define IA64_DOM0VP_zap_physmap         5

/* Assign machine page frame to dom0's pseudo physical address space.  */
#define IA64_DOM0VP_add_physmap         6

/* expose the p2m table into domain */
#define IA64_DOM0VP_expose_p2m          7

/* xen perfmon */
#define IA64_DOM0VP_perfmon             8

/* gmfn version of IA64_DOM0VP_add_physmap */
#define IA64_DOM0VP_add_physmap_with_gmfn       9

/* Add an I/O port space range */
#define IA64_DOM0VP_add_io_space        11

// flags for page assignement to pseudo physical address space
#define _ASSIGN_readonly                0
#define ASSIGN_readonly                 (1UL << _ASSIGN_readonly)
#define ASSIGN_writable                 (0UL << _ASSIGN_readonly) // dummy flag
/* Internal only: memory attribute must be WC/UC/UCE.  */
#define _ASSIGN_nocache                 1
#define ASSIGN_nocache                  (1UL << _ASSIGN_nocache)
// tlb tracking
#define _ASSIGN_tlb_track               2
#define ASSIGN_tlb_track                (1UL << _ASSIGN_tlb_track)
/* Internal only: associated with PGC_allocated bit */
#define _ASSIGN_pgc_allocated           3
#define ASSIGN_pgc_allocated            (1UL << _ASSIGN_pgc_allocated)

/* This structure has the same layout of struct ia64_boot_param, defined in
   <asm/system.h>.  It is redefined here to ease use.  */
struct xen_ia64_boot_param {
	unsigned long command_line;	/* physical address of cmd line args */
	unsigned long efi_systab;	/* physical address of EFI system table */
	unsigned long efi_memmap;	/* physical address of EFI memory map */
	unsigned long efi_memmap_size;	/* size of EFI memory map */
	unsigned long efi_memdesc_size;	/* size of an EFI memory map descriptor */
	unsigned int  efi_memdesc_version;	/* memory descriptor version */
	struct {
		unsigned short num_cols;	/* number of columns on console.  */
		unsigned short num_rows;	/* number of rows on console.  */
		unsigned short orig_x;	/* cursor's x position */
		unsigned short orig_y;	/* cursor's y position */
	} console_info;
	unsigned long fpswa;		/* physical address of the fpswa interface */
	unsigned long initrd_start;
	unsigned long initrd_size;
	unsigned long domain_start;	/* va where the boot time domain begins */
	unsigned long domain_size;	/* how big is the boot domain */
};

#endif /* !__ASSEMBLY__ */

/* Size of the shared_info area (this is not related to page size).  */
#define XSI_SHIFT			14
#define XSI_SIZE			(1 << XSI_SHIFT)
/* Log size of mapped_regs area (64 KB - only 4KB is used).  */
#define XMAPPEDREGS_SHIFT		12
#define XMAPPEDREGS_SIZE		(1 << XMAPPEDREGS_SHIFT)
/* Offset of XASI (Xen arch shared info) wrt XSI_BASE.  */
#define XMAPPEDREGS_OFS			XSI_SIZE

/* Hyperprivops.  */
#define HYPERPRIVOP_START		0x1
#define HYPERPRIVOP_RFI			(HYPERPRIVOP_START + 0x0)
#define HYPERPRIVOP_RSM_DT		(HYPERPRIVOP_START + 0x1)
#define HYPERPRIVOP_SSM_DT		(HYPERPRIVOP_START + 0x2)
#define HYPERPRIVOP_COVER		(HYPERPRIVOP_START + 0x3)
#define HYPERPRIVOP_ITC_D		(HYPERPRIVOP_START + 0x4)
#define HYPERPRIVOP_ITC_I		(HYPERPRIVOP_START + 0x5)
#define HYPERPRIVOP_SSM_I		(HYPERPRIVOP_START + 0x6)
#define HYPERPRIVOP_GET_IVR		(HYPERPRIVOP_START + 0x7)
#define HYPERPRIVOP_GET_TPR		(HYPERPRIVOP_START + 0x8)
#define HYPERPRIVOP_SET_TPR		(HYPERPRIVOP_START + 0x9)
#define HYPERPRIVOP_EOI			(HYPERPRIVOP_START + 0xa)
#define HYPERPRIVOP_SET_ITM		(HYPERPRIVOP_START + 0xb)
#define HYPERPRIVOP_THASH		(HYPERPRIVOP_START + 0xc)
#define HYPERPRIVOP_PTC_GA		(HYPERPRIVOP_START + 0xd)
#define HYPERPRIVOP_ITR_D		(HYPERPRIVOP_START + 0xe)
#define HYPERPRIVOP_GET_RR		(HYPERPRIVOP_START + 0xf)
#define HYPERPRIVOP_SET_RR		(HYPERPRIVOP_START + 0x10)
#define HYPERPRIVOP_SET_KR		(HYPERPRIVOP_START + 0x11)
#define HYPERPRIVOP_FC			(HYPERPRIVOP_START + 0x12)
#define HYPERPRIVOP_GET_CPUID		(HYPERPRIVOP_START + 0x13)
#define HYPERPRIVOP_GET_PMD		(HYPERPRIVOP_START + 0x14)
#define HYPERPRIVOP_GET_EFLAG		(HYPERPRIVOP_START + 0x15)
#define HYPERPRIVOP_SET_EFLAG		(HYPERPRIVOP_START + 0x16)
#define HYPERPRIVOP_RSM_BE		(HYPERPRIVOP_START + 0x17)
#define HYPERPRIVOP_GET_PSR		(HYPERPRIVOP_START + 0x18)
#define HYPERPRIVOP_MAX			(0x19)

/* Fast and light hypercalls.  */
#define __HYPERVISOR_ia64_fast_eoi	__HYPERVISOR_arch_1

/* Xencomm macros.  */
#define XENCOMM_INLINE_MASK 0xf800000000000000UL
#define XENCOMM_INLINE_FLAG 0x8000000000000000UL

#define XENCOMM_IS_INLINE(addr) \
  (((unsigned long)(addr) & XENCOMM_INLINE_MASK) == XENCOMM_INLINE_FLAG)
#define XENCOMM_INLINE_ADDR(addr) \
  ((unsigned long)(addr) & ~XENCOMM_INLINE_MASK)

/* xen perfmon */
#ifdef XEN
#ifndef __ASSEMBLY__
#ifndef _ASM_IA64_PERFMON_H

#include <xen/list.h>   // asm/perfmon.h requires struct list_head
#include <asm/perfmon.h>
// for PFM_xxx and pfarg_features_t, pfarg_context_t, pfarg_reg_t, pfarg_load_t

#endif /* _ASM_IA64_PERFMON_H */

DEFINE_XEN_GUEST_HANDLE(pfarg_features_t);
DEFINE_XEN_GUEST_HANDLE(pfarg_context_t);
DEFINE_XEN_GUEST_HANDLE(pfarg_reg_t);
DEFINE_XEN_GUEST_HANDLE(pfarg_load_t);
#endif /* __ASSEMBLY__ */
#endif /* XEN */

#endif /* __HYPERVISOR_IF_IA64_H__ */

/*
 * Local variables:
 * mode: C
 * c-set-style: "BSD"
 * c-basic-offset: 4
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
