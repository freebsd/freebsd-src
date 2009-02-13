/******************************************************************************
 * domctl.h
 * 
 * Domain management operations. For use by node control stack.
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
 * Copyright (c) 2002-2003, B Dragovic
 * Copyright (c) 2002-2006, K Fraser
 */

#ifndef __XEN_PUBLIC_DOMCTL_H__
#define __XEN_PUBLIC_DOMCTL_H__

#if !defined(__XEN__) && !defined(__XEN_TOOLS__)
#error "domctl operations are intended for use by node control tools only"
#endif

#include "xen.h"

#define XEN_DOMCTL_INTERFACE_VERSION 0x00000005

struct xenctl_cpumap {
    XEN_GUEST_HANDLE_64(uint8_t) bitmap;
    uint32_t nr_cpus;
};

/*
 * NB. xen_domctl.domain is an IN/OUT parameter for this operation.
 * If it is specified as zero, an id is auto-allocated and returned.
 */
#define XEN_DOMCTL_createdomain       1
struct xen_domctl_createdomain {
    /* IN parameters */
    uint32_t ssidref;
    xen_domain_handle_t handle;
 /* Is this an HVM guest (as opposed to a PV guest)? */
#define _XEN_DOMCTL_CDF_hvm_guest 0
#define XEN_DOMCTL_CDF_hvm_guest  (1U<<_XEN_DOMCTL_CDF_hvm_guest)
 /* Use hardware-assisted paging if available? */
#define _XEN_DOMCTL_CDF_hap       1
#define XEN_DOMCTL_CDF_hap        (1U<<_XEN_DOMCTL_CDF_hap)
    uint32_t flags;
};
typedef struct xen_domctl_createdomain xen_domctl_createdomain_t;
DEFINE_XEN_GUEST_HANDLE(xen_domctl_createdomain_t);

#define XEN_DOMCTL_destroydomain      2
#define XEN_DOMCTL_pausedomain        3
#define XEN_DOMCTL_unpausedomain      4
#define XEN_DOMCTL_resumedomain      27

#define XEN_DOMCTL_getdomaininfo      5
struct xen_domctl_getdomaininfo {
    /* OUT variables. */
    domid_t  domain;              /* Also echoed in domctl.domain */
 /* Domain is scheduled to die. */
#define _XEN_DOMINF_dying     0
#define XEN_DOMINF_dying      (1U<<_XEN_DOMINF_dying)
 /* Domain is an HVM guest (as opposed to a PV guest). */
#define _XEN_DOMINF_hvm_guest 1
#define XEN_DOMINF_hvm_guest  (1U<<_XEN_DOMINF_hvm_guest)
 /* The guest OS has shut down. */
#define _XEN_DOMINF_shutdown  2
#define XEN_DOMINF_shutdown   (1U<<_XEN_DOMINF_shutdown)
 /* Currently paused by control software. */
#define _XEN_DOMINF_paused    3
#define XEN_DOMINF_paused     (1U<<_XEN_DOMINF_paused)
 /* Currently blocked pending an event.     */
#define _XEN_DOMINF_blocked   4
#define XEN_DOMINF_blocked    (1U<<_XEN_DOMINF_blocked)
 /* Domain is currently running.            */
#define _XEN_DOMINF_running   5
#define XEN_DOMINF_running    (1U<<_XEN_DOMINF_running)
 /* Being debugged.  */
#define _XEN_DOMINF_debugged  6
#define XEN_DOMINF_debugged   (1U<<_XEN_DOMINF_debugged)
 /* CPU to which this domain is bound.      */
#define XEN_DOMINF_cpumask      255
#define XEN_DOMINF_cpushift       8
 /* XEN_DOMINF_shutdown guest-supplied code.  */
#define XEN_DOMINF_shutdownmask 255
#define XEN_DOMINF_shutdownshift 16
    uint32_t flags;              /* XEN_DOMINF_* */
    uint64_aligned_t tot_pages;
    uint64_aligned_t max_pages;
    uint64_aligned_t shared_info_frame; /* GMFN of shared_info struct */
    uint64_aligned_t cpu_time;
    uint32_t nr_online_vcpus;    /* Number of VCPUs currently online. */
    uint32_t max_vcpu_id;        /* Maximum VCPUID in use by this domain. */
    uint32_t ssidref;
    xen_domain_handle_t handle;
};
typedef struct xen_domctl_getdomaininfo xen_domctl_getdomaininfo_t;
DEFINE_XEN_GUEST_HANDLE(xen_domctl_getdomaininfo_t);


#define XEN_DOMCTL_getmemlist         6
struct xen_domctl_getmemlist {
    /* IN variables. */
    /* Max entries to write to output buffer. */
    uint64_aligned_t max_pfns;
    /* Start index in guest's page list. */
    uint64_aligned_t start_pfn;
    XEN_GUEST_HANDLE_64(uint64_t) buffer;
    /* OUT variables. */
    uint64_aligned_t num_pfns;
};
typedef struct xen_domctl_getmemlist xen_domctl_getmemlist_t;
DEFINE_XEN_GUEST_HANDLE(xen_domctl_getmemlist_t);


#define XEN_DOMCTL_getpageframeinfo   7

#define XEN_DOMCTL_PFINFO_LTAB_SHIFT 28
#define XEN_DOMCTL_PFINFO_NOTAB   (0x0U<<28)
#define XEN_DOMCTL_PFINFO_L1TAB   (0x1U<<28)
#define XEN_DOMCTL_PFINFO_L2TAB   (0x2U<<28)
#define XEN_DOMCTL_PFINFO_L3TAB   (0x3U<<28)
#define XEN_DOMCTL_PFINFO_L4TAB   (0x4U<<28)
#define XEN_DOMCTL_PFINFO_LTABTYPE_MASK (0x7U<<28)
#define XEN_DOMCTL_PFINFO_LPINTAB (0x1U<<31)
#define XEN_DOMCTL_PFINFO_XTAB    (0xfU<<28) /* invalid page */
#define XEN_DOMCTL_PFINFO_LTAB_MASK (0xfU<<28)

struct xen_domctl_getpageframeinfo {
    /* IN variables. */
    uint64_aligned_t gmfn; /* GMFN to query */
    /* OUT variables. */
    /* Is the page PINNED to a type? */
    uint32_t type;         /* see above type defs */
};
typedef struct xen_domctl_getpageframeinfo xen_domctl_getpageframeinfo_t;
DEFINE_XEN_GUEST_HANDLE(xen_domctl_getpageframeinfo_t);


#define XEN_DOMCTL_getpageframeinfo2  8
struct xen_domctl_getpageframeinfo2 {
    /* IN variables. */
    uint64_aligned_t num;
    /* IN/OUT variables. */
    XEN_GUEST_HANDLE_64(uint32_t) array;
};
typedef struct xen_domctl_getpageframeinfo2 xen_domctl_getpageframeinfo2_t;
DEFINE_XEN_GUEST_HANDLE(xen_domctl_getpageframeinfo2_t);


/*
 * Control shadow pagetables operation
 */
#define XEN_DOMCTL_shadow_op         10

/* Disable shadow mode. */
#define XEN_DOMCTL_SHADOW_OP_OFF         0

/* Enable shadow mode (mode contains ORed XEN_DOMCTL_SHADOW_ENABLE_* flags). */
#define XEN_DOMCTL_SHADOW_OP_ENABLE      32

/* Log-dirty bitmap operations. */
 /* Return the bitmap and clean internal copy for next round. */
#define XEN_DOMCTL_SHADOW_OP_CLEAN       11
 /* Return the bitmap but do not modify internal copy. */
#define XEN_DOMCTL_SHADOW_OP_PEEK        12

/* Memory allocation accessors. */
#define XEN_DOMCTL_SHADOW_OP_GET_ALLOCATION   30
#define XEN_DOMCTL_SHADOW_OP_SET_ALLOCATION   31

/* Legacy enable operations. */
 /* Equiv. to ENABLE with no mode flags. */
#define XEN_DOMCTL_SHADOW_OP_ENABLE_TEST       1
 /* Equiv. to ENABLE with mode flag ENABLE_LOG_DIRTY. */
#define XEN_DOMCTL_SHADOW_OP_ENABLE_LOGDIRTY   2
 /* Equiv. to ENABLE with mode flags ENABLE_REFCOUNT and ENABLE_TRANSLATE. */
#define XEN_DOMCTL_SHADOW_OP_ENABLE_TRANSLATE  3

/* Mode flags for XEN_DOMCTL_SHADOW_OP_ENABLE. */
 /*
  * Shadow pagetables are refcounted: guest does not use explicit mmu
  * operations nor write-protect its pagetables.
  */
#define XEN_DOMCTL_SHADOW_ENABLE_REFCOUNT  (1 << 1)
 /*
  * Log pages in a bitmap as they are dirtied.
  * Used for live relocation to determine which pages must be re-sent.
  */
#define XEN_DOMCTL_SHADOW_ENABLE_LOG_DIRTY (1 << 2)
 /*
  * Automatically translate GPFNs into MFNs.
  */
#define XEN_DOMCTL_SHADOW_ENABLE_TRANSLATE (1 << 3)
 /*
  * Xen does not steal virtual address space from the guest.
  * Requires HVM support.
  */
#define XEN_DOMCTL_SHADOW_ENABLE_EXTERNAL  (1 << 4)

struct xen_domctl_shadow_op_stats {
    uint32_t fault_count;
    uint32_t dirty_count;
};
typedef struct xen_domctl_shadow_op_stats xen_domctl_shadow_op_stats_t;
DEFINE_XEN_GUEST_HANDLE(xen_domctl_shadow_op_stats_t);

struct xen_domctl_shadow_op {
    /* IN variables. */
    uint32_t       op;       /* XEN_DOMCTL_SHADOW_OP_* */

    /* OP_ENABLE */
    uint32_t       mode;     /* XEN_DOMCTL_SHADOW_ENABLE_* */

    /* OP_GET_ALLOCATION / OP_SET_ALLOCATION */
    uint32_t       mb;       /* Shadow memory allocation in MB */

    /* OP_PEEK / OP_CLEAN */
    XEN_GUEST_HANDLE_64(uint8_t) dirty_bitmap;
    uint64_aligned_t pages; /* Size of buffer. Updated with actual size. */
    struct xen_domctl_shadow_op_stats stats;
};
typedef struct xen_domctl_shadow_op xen_domctl_shadow_op_t;
DEFINE_XEN_GUEST_HANDLE(xen_domctl_shadow_op_t);


#define XEN_DOMCTL_max_mem           11
struct xen_domctl_max_mem {
    /* IN variables. */
    uint64_aligned_t max_memkb;
};
typedef struct xen_domctl_max_mem xen_domctl_max_mem_t;
DEFINE_XEN_GUEST_HANDLE(xen_domctl_max_mem_t);


#define XEN_DOMCTL_setvcpucontext    12
#define XEN_DOMCTL_getvcpucontext    13
struct xen_domctl_vcpucontext {
    uint32_t              vcpu;                  /* IN */
    XEN_GUEST_HANDLE_64(vcpu_guest_context_t) ctxt; /* IN/OUT */
};
typedef struct xen_domctl_vcpucontext xen_domctl_vcpucontext_t;
DEFINE_XEN_GUEST_HANDLE(xen_domctl_vcpucontext_t);


#define XEN_DOMCTL_getvcpuinfo       14
struct xen_domctl_getvcpuinfo {
    /* IN variables. */
    uint32_t vcpu;
    /* OUT variables. */
    uint8_t  online;                  /* currently online (not hotplugged)? */
    uint8_t  blocked;                 /* blocked waiting for an event? */
    uint8_t  running;                 /* currently scheduled on its CPU? */
    uint64_aligned_t cpu_time;        /* total cpu time consumed (ns) */
    uint32_t cpu;                     /* current mapping   */
};
typedef struct xen_domctl_getvcpuinfo xen_domctl_getvcpuinfo_t;
DEFINE_XEN_GUEST_HANDLE(xen_domctl_getvcpuinfo_t);


/* Get/set which physical cpus a vcpu can execute on. */
#define XEN_DOMCTL_setvcpuaffinity    9
#define XEN_DOMCTL_getvcpuaffinity   25
struct xen_domctl_vcpuaffinity {
    uint32_t  vcpu;              /* IN */
    struct xenctl_cpumap cpumap; /* IN/OUT */
};
typedef struct xen_domctl_vcpuaffinity xen_domctl_vcpuaffinity_t;
DEFINE_XEN_GUEST_HANDLE(xen_domctl_vcpuaffinity_t);


#define XEN_DOMCTL_max_vcpus         15
struct xen_domctl_max_vcpus {
    uint32_t max;           /* maximum number of vcpus */
};
typedef struct xen_domctl_max_vcpus xen_domctl_max_vcpus_t;
DEFINE_XEN_GUEST_HANDLE(xen_domctl_max_vcpus_t);


#define XEN_DOMCTL_scheduler_op      16
/* Scheduler types. */
#define XEN_SCHEDULER_SEDF     4
#define XEN_SCHEDULER_CREDIT   5
/* Set or get info? */
#define XEN_DOMCTL_SCHEDOP_putinfo 0
#define XEN_DOMCTL_SCHEDOP_getinfo 1
struct xen_domctl_scheduler_op {
    uint32_t sched_id;  /* XEN_SCHEDULER_* */
    uint32_t cmd;       /* XEN_DOMCTL_SCHEDOP_* */
    union {
        struct xen_domctl_sched_sedf {
            uint64_aligned_t period;
            uint64_aligned_t slice;
            uint64_aligned_t latency;
            uint32_t extratime;
            uint32_t weight;
        } sedf;
        struct xen_domctl_sched_credit {
            uint16_t weight;
            uint16_t cap;
        } credit;
    } u;
};
typedef struct xen_domctl_scheduler_op xen_domctl_scheduler_op_t;
DEFINE_XEN_GUEST_HANDLE(xen_domctl_scheduler_op_t);


#define XEN_DOMCTL_setdomainhandle   17
struct xen_domctl_setdomainhandle {
    xen_domain_handle_t handle;
};
typedef struct xen_domctl_setdomainhandle xen_domctl_setdomainhandle_t;
DEFINE_XEN_GUEST_HANDLE(xen_domctl_setdomainhandle_t);


#define XEN_DOMCTL_setdebugging      18
struct xen_domctl_setdebugging {
    uint8_t enable;
};
typedef struct xen_domctl_setdebugging xen_domctl_setdebugging_t;
DEFINE_XEN_GUEST_HANDLE(xen_domctl_setdebugging_t);


#define XEN_DOMCTL_irq_permission    19
struct xen_domctl_irq_permission {
    uint8_t pirq;
    uint8_t allow_access;    /* flag to specify enable/disable of IRQ access */
};
typedef struct xen_domctl_irq_permission xen_domctl_irq_permission_t;
DEFINE_XEN_GUEST_HANDLE(xen_domctl_irq_permission_t);


#define XEN_DOMCTL_iomem_permission  20
struct xen_domctl_iomem_permission {
    uint64_aligned_t first_mfn;/* first page (physical page number) in range */
    uint64_aligned_t nr_mfns;  /* number of pages in range (>0) */
    uint8_t  allow_access;     /* allow (!0) or deny (0) access to range? */
};
typedef struct xen_domctl_iomem_permission xen_domctl_iomem_permission_t;
DEFINE_XEN_GUEST_HANDLE(xen_domctl_iomem_permission_t);


#define XEN_DOMCTL_ioport_permission 21
struct xen_domctl_ioport_permission {
    uint32_t first_port;              /* first port int range */
    uint32_t nr_ports;                /* size of port range */
    uint8_t  allow_access;            /* allow or deny access to range? */
};
typedef struct xen_domctl_ioport_permission xen_domctl_ioport_permission_t;
DEFINE_XEN_GUEST_HANDLE(xen_domctl_ioport_permission_t);


#define XEN_DOMCTL_hypercall_init    22
struct xen_domctl_hypercall_init {
    uint64_aligned_t  gmfn;           /* GMFN to be initialised */
};
typedef struct xen_domctl_hypercall_init xen_domctl_hypercall_init_t;
DEFINE_XEN_GUEST_HANDLE(xen_domctl_hypercall_init_t);


#define XEN_DOMCTL_arch_setup        23
#define _XEN_DOMAINSETUP_hvm_guest 0
#define XEN_DOMAINSETUP_hvm_guest  (1UL<<_XEN_DOMAINSETUP_hvm_guest)
#define _XEN_DOMAINSETUP_query 1 /* Get parameters (for save)  */
#define XEN_DOMAINSETUP_query  (1UL<<_XEN_DOMAINSETUP_query)
#define _XEN_DOMAINSETUP_sioemu_guest 2
#define XEN_DOMAINSETUP_sioemu_guest  (1UL<<_XEN_DOMAINSETUP_sioemu_guest)
typedef struct xen_domctl_arch_setup {
    uint64_aligned_t flags;  /* XEN_DOMAINSETUP_* */
#ifdef __ia64__
    uint64_aligned_t bp;     /* mpaddr of boot param area */
    uint64_aligned_t maxmem; /* Highest memory address for MDT.  */
    uint64_aligned_t xsi_va; /* Xen shared_info area virtual address.  */
    uint32_t hypercall_imm;  /* Break imm for Xen hypercalls.  */
    int8_t vhpt_size_log2;   /* Log2 of VHPT size. */
#endif
} xen_domctl_arch_setup_t;
DEFINE_XEN_GUEST_HANDLE(xen_domctl_arch_setup_t);


#define XEN_DOMCTL_settimeoffset     24
struct xen_domctl_settimeoffset {
    int32_t  time_offset_seconds; /* applied to domain wallclock time */
};
typedef struct xen_domctl_settimeoffset xen_domctl_settimeoffset_t;
DEFINE_XEN_GUEST_HANDLE(xen_domctl_settimeoffset_t);

 
#define XEN_DOMCTL_gethvmcontext     33
#define XEN_DOMCTL_sethvmcontext     34
typedef struct xen_domctl_hvmcontext {
    uint32_t size; /* IN/OUT: size of buffer / bytes filled */
    XEN_GUEST_HANDLE_64(uint8_t) buffer; /* IN/OUT: data, or call
                                          * gethvmcontext with NULL
                                          * buffer to get size
                                          * req'd */
} xen_domctl_hvmcontext_t;
DEFINE_XEN_GUEST_HANDLE(xen_domctl_hvmcontext_t);


#define XEN_DOMCTL_set_address_size  35
#define XEN_DOMCTL_get_address_size  36
typedef struct xen_domctl_address_size {
    uint32_t size;
} xen_domctl_address_size_t;
DEFINE_XEN_GUEST_HANDLE(xen_domctl_address_size_t);


#define XEN_DOMCTL_real_mode_area    26
struct xen_domctl_real_mode_area {
    uint32_t log; /* log2 of Real Mode Area size */
};
typedef struct xen_domctl_real_mode_area xen_domctl_real_mode_area_t;
DEFINE_XEN_GUEST_HANDLE(xen_domctl_real_mode_area_t);


#define XEN_DOMCTL_sendtrigger       28
#define XEN_DOMCTL_SENDTRIGGER_NMI    0
#define XEN_DOMCTL_SENDTRIGGER_RESET  1
#define XEN_DOMCTL_SENDTRIGGER_INIT   2
struct xen_domctl_sendtrigger {
    uint32_t  trigger;  /* IN */
    uint32_t  vcpu;     /* IN */
};
typedef struct xen_domctl_sendtrigger xen_domctl_sendtrigger_t;
DEFINE_XEN_GUEST_HANDLE(xen_domctl_sendtrigger_t);


/* Assign PCI device to HVM guest. Sets up IOMMU structures. */
#define XEN_DOMCTL_assign_device      37
#define XEN_DOMCTL_test_assign_device 45
#define XEN_DOMCTL_deassign_device 47
struct xen_domctl_assign_device {
    uint32_t  machine_bdf;   /* machine PCI ID of assigned device */
};
typedef struct xen_domctl_assign_device xen_domctl_assign_device_t;
DEFINE_XEN_GUEST_HANDLE(xen_domctl_assign_device_t);

/* Retrieve sibling devices infomation of machine_bdf */
#define XEN_DOMCTL_get_device_group 50
struct xen_domctl_get_device_group {
    uint32_t  machine_bdf;      /* IN */
    uint32_t  max_sdevs;        /* IN */
    uint32_t  num_sdevs;        /* OUT */
    XEN_GUEST_HANDLE_64(uint32)  sdev_array;   /* OUT */
};
typedef struct xen_domctl_get_device_group xen_domctl_get_device_group_t;
DEFINE_XEN_GUEST_HANDLE(xen_domctl_get_device_group_t);

/* Pass-through interrupts: bind real irq -> hvm devfn. */
#define XEN_DOMCTL_bind_pt_irq       38
#define XEN_DOMCTL_unbind_pt_irq     48
typedef enum pt_irq_type_e {
    PT_IRQ_TYPE_PCI,
    PT_IRQ_TYPE_ISA,
    PT_IRQ_TYPE_MSI,
} pt_irq_type_t;
struct xen_domctl_bind_pt_irq {
    uint32_t machine_irq;
    pt_irq_type_t irq_type;
    uint32_t hvm_domid;

    union {
        struct {
            uint8_t isa_irq;
        } isa;
        struct {
            uint8_t bus;
            uint8_t device;
            uint8_t intx;
        } pci;
        struct {
            uint8_t gvec;
            uint32_t gflags;
        } msi;
    } u;
};
typedef struct xen_domctl_bind_pt_irq xen_domctl_bind_pt_irq_t;
DEFINE_XEN_GUEST_HANDLE(xen_domctl_bind_pt_irq_t);


/* Bind machine I/O address range -> HVM address range. */
#define XEN_DOMCTL_memory_mapping    39
#define DPCI_ADD_MAPPING         1
#define DPCI_REMOVE_MAPPING      0
struct xen_domctl_memory_mapping {
    uint64_aligned_t first_gfn; /* first page (hvm guest phys page) in range */
    uint64_aligned_t first_mfn; /* first page (machine page) in range */
    uint64_aligned_t nr_mfns;   /* number of pages in range (>0) */
    uint32_t add_mapping;       /* add or remove mapping */
    uint32_t padding;           /* padding for 64-bit aligned structure */
};
typedef struct xen_domctl_memory_mapping xen_domctl_memory_mapping_t;
DEFINE_XEN_GUEST_HANDLE(xen_domctl_memory_mapping_t);


/* Bind machine I/O port range -> HVM I/O port range. */
#define XEN_DOMCTL_ioport_mapping    40
struct xen_domctl_ioport_mapping {
    uint32_t first_gport;     /* first guest IO port*/
    uint32_t first_mport;     /* first machine IO port */
    uint32_t nr_ports;        /* size of port range */
    uint32_t add_mapping;     /* add or remove mapping */
};
typedef struct xen_domctl_ioport_mapping xen_domctl_ioport_mapping_t;
DEFINE_XEN_GUEST_HANDLE(xen_domctl_ioport_mapping_t);


/*
 * Pin caching type of RAM space for x86 HVM domU.
 */
#define XEN_DOMCTL_pin_mem_cacheattr 41
/* Caching types: these happen to be the same as x86 MTRR/PAT type codes. */
#define XEN_DOMCTL_MEM_CACHEATTR_UC  0
#define XEN_DOMCTL_MEM_CACHEATTR_WC  1
#define XEN_DOMCTL_MEM_CACHEATTR_WT  4
#define XEN_DOMCTL_MEM_CACHEATTR_WP  5
#define XEN_DOMCTL_MEM_CACHEATTR_WB  6
#define XEN_DOMCTL_MEM_CACHEATTR_UCM 7
struct xen_domctl_pin_mem_cacheattr {
    uint64_aligned_t start, end;
    unsigned int type; /* XEN_DOMCTL_MEM_CACHEATTR_* */
};
typedef struct xen_domctl_pin_mem_cacheattr xen_domctl_pin_mem_cacheattr_t;
DEFINE_XEN_GUEST_HANDLE(xen_domctl_pin_mem_cacheattr_t);


#define XEN_DOMCTL_set_ext_vcpucontext 42
#define XEN_DOMCTL_get_ext_vcpucontext 43
struct xen_domctl_ext_vcpucontext {
    /* IN: VCPU that this call applies to. */
    uint32_t         vcpu;
    /*
     * SET: Size of struct (IN)
     * GET: Size of struct (OUT)
     */
    uint32_t         size;
#if defined(__i386__) || defined(__x86_64__)
    /* SYSCALL from 32-bit mode and SYSENTER callback information. */
    /* NB. SYSCALL from 64-bit mode is contained in vcpu_guest_context_t */
    uint64_aligned_t syscall32_callback_eip;
    uint64_aligned_t sysenter_callback_eip;
    uint16_t         syscall32_callback_cs;
    uint16_t         sysenter_callback_cs;
    uint8_t          syscall32_disables_events;
    uint8_t          sysenter_disables_events;
#endif
};
typedef struct xen_domctl_ext_vcpucontext xen_domctl_ext_vcpucontext_t;
DEFINE_XEN_GUEST_HANDLE(xen_domctl_ext_vcpucontext_t);

/*
 * Set optimizaton features for a domain
 */
#define XEN_DOMCTL_set_opt_feature    44
struct xen_domctl_set_opt_feature {
#if defined(__ia64__)
    struct xen_ia64_opt_feature optf;
#else
    /* Make struct non-empty: do not depend on this field name! */
    uint64_t dummy;
#endif
};
typedef struct xen_domctl_set_opt_feature xen_domctl_set_opt_feature_t;
DEFINE_XEN_GUEST_HANDLE(xen_domctl_set_opt_feature_t);

/*
 * Set the target domain for a domain
 */
#define XEN_DOMCTL_set_target    46
struct xen_domctl_set_target {
    domid_t target;
};
typedef struct xen_domctl_set_target xen_domctl_set_target_t;
DEFINE_XEN_GUEST_HANDLE(xen_domctl_set_target_t);

#if defined(__i386__) || defined(__x86_64__)
# define XEN_CPUID_INPUT_UNUSED  0xFFFFFFFF
# define XEN_DOMCTL_set_cpuid 49
struct xen_domctl_cpuid {
  unsigned int  input[2];
  unsigned int  eax;
  unsigned int  ebx;
  unsigned int  ecx;
  unsigned int  edx;
};
typedef struct xen_domctl_cpuid xen_domctl_cpuid_t;
DEFINE_XEN_GUEST_HANDLE(xen_domctl_cpuid_t);
#endif

#define XEN_DOMCTL_subscribe          29
struct xen_domctl_subscribe {
    uint32_t port; /* IN */
};
typedef struct xen_domctl_subscribe xen_domctl_subscribe_t;
DEFINE_XEN_GUEST_HANDLE(xen_domctl_subscribe_t);

/*
 * Define the maximum machine address size which should be allocated
 * to a guest.
 */
#define XEN_DOMCTL_set_machine_address_size  51
#define XEN_DOMCTL_get_machine_address_size  52


struct xen_domctl {
    uint32_t cmd;
    uint32_t interface_version; /* XEN_DOMCTL_INTERFACE_VERSION */
    domid_t  domain;
    union {
        struct xen_domctl_createdomain      createdomain;
        struct xen_domctl_getdomaininfo     getdomaininfo;
        struct xen_domctl_getmemlist        getmemlist;
        struct xen_domctl_getpageframeinfo  getpageframeinfo;
        struct xen_domctl_getpageframeinfo2 getpageframeinfo2;
        struct xen_domctl_vcpuaffinity      vcpuaffinity;
        struct xen_domctl_shadow_op         shadow_op;
        struct xen_domctl_max_mem           max_mem;
        struct xen_domctl_vcpucontext       vcpucontext;
        struct xen_domctl_getvcpuinfo       getvcpuinfo;
        struct xen_domctl_max_vcpus         max_vcpus;
        struct xen_domctl_scheduler_op      scheduler_op;
        struct xen_domctl_setdomainhandle   setdomainhandle;
        struct xen_domctl_setdebugging      setdebugging;
        struct xen_domctl_irq_permission    irq_permission;
        struct xen_domctl_iomem_permission  iomem_permission;
        struct xen_domctl_ioport_permission ioport_permission;
        struct xen_domctl_hypercall_init    hypercall_init;
        struct xen_domctl_arch_setup        arch_setup;
        struct xen_domctl_settimeoffset     settimeoffset;
        struct xen_domctl_real_mode_area    real_mode_area;
        struct xen_domctl_hvmcontext        hvmcontext;
        struct xen_domctl_address_size      address_size;
        struct xen_domctl_sendtrigger       sendtrigger;
        struct xen_domctl_get_device_group  get_device_group;
        struct xen_domctl_assign_device     assign_device;
        struct xen_domctl_bind_pt_irq       bind_pt_irq;
        struct xen_domctl_memory_mapping    memory_mapping;
        struct xen_domctl_ioport_mapping    ioport_mapping;
        struct xen_domctl_pin_mem_cacheattr pin_mem_cacheattr;
        struct xen_domctl_ext_vcpucontext   ext_vcpucontext;
        struct xen_domctl_set_opt_feature   set_opt_feature;
        struct xen_domctl_set_target        set_target;
        struct xen_domctl_subscribe         subscribe;
#if defined(__i386__) || defined(__x86_64__)
        struct xen_domctl_cpuid             cpuid;
#endif
        uint8_t                             pad[128];
    } u;
};
typedef struct xen_domctl xen_domctl_t;
DEFINE_XEN_GUEST_HANDLE(xen_domctl_t);

#endif /* __XEN_PUBLIC_DOMCTL_H__ */

/*
 * Local variables:
 * mode: C
 * c-set-style: "BSD"
 * c-basic-offset: 4
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
