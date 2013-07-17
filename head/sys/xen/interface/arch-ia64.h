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

#include "xen.h"

#ifndef __HYPERVISOR_IF_IA64_H__
#define __HYPERVISOR_IF_IA64_H__

#if !defined(__GNUC__) || defined(__STRICT_ANSI__)
#error "Anonymous structs/unions are a GNU extension."
#endif

/* Structural guest handles introduced in 0x00030201. */
#if __XEN_INTERFACE_VERSION__ >= 0x00030201
#define ___DEFINE_XEN_GUEST_HANDLE(name, type) \
    typedef struct { type *p; } __guest_handle_ ## name
#else
#define ___DEFINE_XEN_GUEST_HANDLE(name, type) \
    typedef type * __guest_handle_ ## name
#endif

#define __DEFINE_XEN_GUEST_HANDLE(name, type) \
    ___DEFINE_XEN_GUEST_HANDLE(name, type);   \
    ___DEFINE_XEN_GUEST_HANDLE(const_##name, const type)

#define DEFINE_XEN_GUEST_HANDLE(name)   __DEFINE_XEN_GUEST_HANDLE(name, name)
#define XEN_GUEST_HANDLE(name)          __guest_handle_ ## name
#define XEN_GUEST_HANDLE_64(name)       XEN_GUEST_HANDLE(name)
#define uint64_aligned_t                uint64_t
#define set_xen_guest_handle_raw(hnd, val)  do { (hnd).p = val; } while (0)
#ifdef __XEN_TOOLS__
#define get_xen_guest_handle(val, hnd)  do { val = (hnd).p; } while (0)
#endif
#define set_xen_guest_handle(hnd, val) set_xen_guest_handle_raw(hnd, val)

#ifndef __ASSEMBLY__
typedef unsigned long xen_pfn_t;
#define PRI_xen_pfn "lx"
#endif

/* Arch specific VIRQs definition */
#define VIRQ_ITC        VIRQ_ARCH_0 /* V. Virtual itc timer */
#define VIRQ_MCA_CMC    VIRQ_ARCH_1 /* MCA cmc interrupt */
#define VIRQ_MCA_CPE    VIRQ_ARCH_2 /* MCA cpe interrupt */

/* Maximum number of virtual CPUs in multi-processor guests. */
/* WARNING: before changing this, check that shared_info fits on a page */
#define XEN_LEGACY_MAX_VCPUS 64

/* IO ports location for PV.  */
#define IO_PORTS_PADDR          0x00000ffffc000000UL
#define IO_PORTS_SIZE           0x0000000004000000UL

#ifndef __ASSEMBLY__

typedef unsigned long xen_ulong_t;

#ifdef __XEN_TOOLS__
#define XEN_PAGE_SIZE XC_PAGE_SIZE
#else
#define XEN_PAGE_SIZE PAGE_SIZE
#endif

#define INVALID_MFN       (~0UL)

struct pt_fpreg {
    union {
        unsigned long bits[2];
        long double __dummy;    /* force 16-byte alignment */
    } u;
};

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
            unsigned long tmp[16]; // temp registers (e.g. for hyperprivops)

            /* itc paravirtualization
             * vAR.ITC = mAR.ITC + itc_offset
             * itc_last is one which was lastly passed to
             * the guest OS in order to prevent it from
             * going backwords.
             */
            unsigned long itc_offset;
            unsigned long itc_last;
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

/*
 * This structure is used for magic page in domain pseudo physical address
 * space and the result of XENMEM_machine_memory_map.
 * As the XENMEM_machine_memory_map result,
 * xen_memory_map::nr_entries indicates the size in bytes 
 * including struct xen_ia64_memmap_info. Not the number of entries.
 */
struct xen_ia64_memmap_info {
    uint64_t efi_memmap_size;       /* size of EFI memory map */
    uint64_t efi_memdesc_size;      /* size of an EFI memory map descriptor */
    uint32_t efi_memdesc_version;   /* memory descriptor version */
    void *memdesc[0];               /* array of efi_memory_desc_t */
};
typedef struct xen_ia64_memmap_info xen_ia64_memmap_info_t;

struct arch_shared_info {
    /* PFN of the start_info page.  */
    unsigned long start_info_pfn;

    /* Interrupt vector for event channel.  */
    int evtchn_vector;

    /* PFN of memmap_info page */
    unsigned int memmap_info_num_pages;/* currently only = 1 case is
                                          supported. */
    unsigned long memmap_info_pfn;

    uint64_t pad[31];
};
typedef struct arch_shared_info arch_shared_info_t;

typedef unsigned long xen_callback_t;

struct ia64_tr_entry {
    unsigned long pte;
    unsigned long itir;
    unsigned long vadr;
    unsigned long rid;
};
typedef struct ia64_tr_entry ia64_tr_entry_t;
DEFINE_XEN_GUEST_HANDLE(ia64_tr_entry_t);

struct vcpu_tr_regs {
    struct ia64_tr_entry itrs[12];
    struct ia64_tr_entry dtrs[12];
};

union vcpu_ar_regs {
    unsigned long ar[128];
    struct {
        unsigned long kr[8];
        unsigned long rsv1[8];
        unsigned long rsc;
        unsigned long bsp;
        unsigned long bspstore;
        unsigned long rnat;
        unsigned long rsv2;
        unsigned long fcr;
        unsigned long rsv3[2];
        unsigned long eflag;
        unsigned long csd;
        unsigned long ssd;
        unsigned long cflg;
        unsigned long fsr;
        unsigned long fir;
        unsigned long fdr;
        unsigned long rsv4;
        unsigned long ccv; /* 32 */
        unsigned long rsv5[3];
        unsigned long unat;
        unsigned long rsv6[3];
        unsigned long fpsr;
        unsigned long rsv7[3];
        unsigned long itc;
        unsigned long rsv8[3];
        unsigned long ign1[16];
        unsigned long pfs; /* 64 */
        unsigned long lc;
        unsigned long ec;
        unsigned long rsv9[45];
        unsigned long ign2[16];
    };
};

union vcpu_cr_regs {
    unsigned long cr[128];
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

struct vcpu_guest_context_regs {
        unsigned long r[32];
        unsigned long b[8];
        unsigned long bank[16];
        unsigned long ip;
        unsigned long psr;
        unsigned long cfm;
        unsigned long pr;
        unsigned int nats; /* NaT bits for r1-r31.  */
        unsigned int bnats; /* Nat bits for banked registers.  */
        union vcpu_ar_regs ar;
        union vcpu_cr_regs cr;
        struct pt_fpreg f[128];
        unsigned long dbr[8];
        unsigned long ibr[8];
        unsigned long rr[8];
        unsigned long pkr[16];

        /* FIXME: cpuid,pmd,pmc */

        unsigned long xip;
        unsigned long xpsr;
        unsigned long xfs;
        unsigned long xr[4];

        struct vcpu_tr_regs tr;

        /* Physical registers in case of debug event.  */
        unsigned long excp_iipa;
        unsigned long excp_ifa;
        unsigned long excp_isr;
        unsigned int excp_vector;

        /*
         * The rbs is intended to be the image of the stacked registers still
         * in the cpu (not yet stored in memory).  It is laid out as if it
         * were written in memory at a 512 (64*8) aligned address + offset.
         * rbs_voff is (offset / 8).  rbs_nat contains NaT bits for the
         * remaining rbs registers.  rbs_rnat contains NaT bits for in memory
         * rbs registers.
         * Note: loadrs is 2**14 bytes == 2**11 slots.
         */
        unsigned int rbs_voff;
        unsigned long rbs[2048];
        unsigned long rbs_rnat;

        /*
         * RSE.N_STACKED_PHYS via PAL_RSE_INFO
         * Strictly this isn't cpu context, but this value is necessary
         * for domain save/restore. So is here.
         */
        unsigned long num_phys_stacked;
};

struct vcpu_guest_context {
#define VGCF_EXTRA_REGS (1UL << 1)	/* Set extra regs.  */
#define VGCF_SET_CR_IRR (1UL << 2)	/* Set cr_irr[0:3]. */
#define VGCF_online     (1UL << 3)  /* make this vcpu online */
#define VGCF_SET_AR_ITC (1UL << 4)  /* set pv ar.itc. itc_offset, itc_last */
    unsigned long flags;       /* VGCF_* flags */

    struct vcpu_guest_context_regs regs;

    unsigned long event_callback_ip;

    /* xen doesn't share privregs pages with hvm domain so that this member
     * doesn't make sense for hvm domain.
     * ~0UL is already used for INVALID_P2M_ENTRY. */
#define VGC_PRIVREGS_HVM       (~(-2UL))
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

/* get fpswa revision */
#define IA64_DOM0VP_fpswa_revision      10

/* Add an I/O port space range */
#define IA64_DOM0VP_add_io_space        11

/* expose the foreign domain's p2m table into privileged domain */
#define IA64_DOM0VP_expose_foreign_p2m  12
#define         IA64_DOM0VP_EFP_ALLOC_PTE       0x1 /* allocate p2m table */

/* unexpose the foreign domain's p2m table into privileged domain */
#define IA64_DOM0VP_unexpose_foreign_p2m        13

/* get memmap_info and memmap. It is possible to map the page directly
   by foreign page mapping, but there is a race between writer.
   This hypercall avoids such race. */
#define IA64_DOM0VP_get_memmap          14

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
/* Page is an IO page.  */
#define _ASSIGN_io                      4
#define ASSIGN_io                       (1UL << _ASSIGN_io)

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
#define HYPERPRIVOP_SET_RR0_TO_RR4	(HYPERPRIVOP_START + 0x19)
#define HYPERPRIVOP_MAX			(0x1a)

/* Fast and light hypercalls.  */
#define __HYPERVISOR_ia64_fast_eoi	__HYPERVISOR_arch_1

/* Extra debug features.  */
#define __HYPERVISOR_ia64_debug_op  __HYPERVISOR_arch_2

/* Xencomm macros.  */
#define XENCOMM_INLINE_MASK 0xf800000000000000UL
#define XENCOMM_INLINE_FLAG 0x8000000000000000UL

#ifndef __ASSEMBLY__

/*
 * Optimization features.
 * The hypervisor may do some special optimizations for guests. This hypercall
 * can be used to switch on/of these special optimizations.
 */
#define __HYPERVISOR_opt_feature	0x700UL

#define XEN_IA64_OPTF_OFF	0x0
#define XEN_IA64_OPTF_ON	0x1

/*
 * If this feature is switched on, the hypervisor inserts the
 * tlb entries without calling the guests traphandler.
 * This is useful in guests using region 7 for identity mapping
 * like the linux kernel does.
 */
#define XEN_IA64_OPTF_IDENT_MAP_REG7    1

/* Identity mapping of region 4 addresses in HVM. */
#define XEN_IA64_OPTF_IDENT_MAP_REG4    2

/* Identity mapping of region 5 addresses in HVM. */
#define XEN_IA64_OPTF_IDENT_MAP_REG5    3

#define XEN_IA64_OPTF_IDENT_MAP_NOT_SET  (0)

struct xen_ia64_opt_feature {
	unsigned long cmd;		/* Which feature */
	unsigned char on;		/* Switch feature on/off */
	union {
		struct {
				/* The page protection bit mask of the pte.
			 	 * This will be or'ed with the pte. */
			unsigned long pgprot;
			unsigned long key;	/* A protection key for itir. */
		};
	};
};

#endif /* __ASSEMBLY__ */

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

#ifndef __ASSEMBLY__
#include "arch-ia64/hvm/memmap.h"
#endif

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
