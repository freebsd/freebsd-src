/******************************************************************************
 * sysctl.h
 *
 * System management operations. For use by node control stack.
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
 * Copyright (c) 2002-2006, K Fraser
 */

#ifndef __XEN_PUBLIC_SYSCTL_H__
#define __XEN_PUBLIC_SYSCTL_H__

#if !defined(__XEN__) && !defined(__XEN_TOOLS__)
#error "sysctl operations are intended for use by node control tools only"
#endif

#include "xen.h"
#include "domctl.h"
#include "physdev.h"

#define XEN_SYSCTL_INTERFACE_VERSION 0x00000014

/*
 * Read console content from Xen buffer ring.
 */
/* XEN_SYSCTL_readconsole */
struct xen_sysctl_readconsole {
    /* IN: Non-zero -> clear after reading. */
    uint8_t clear;
    /* IN: Non-zero -> start index specified by @index field. */
    uint8_t incremental;
    uint8_t pad0, pad1;
    /*
     * IN:  Start index for consuming from ring buffer (if @incremental);
     * OUT: End index after consuming from ring buffer.
     */
    uint32_t index;
    /* IN: Virtual address to write console data. */
    XEN_GUEST_HANDLE_64(char) buffer;
    /* IN: Size of buffer; OUT: Bytes written to buffer. */
    uint32_t count;
};

/* Get trace buffers machine base address */
/* XEN_SYSCTL_tbuf_op */
struct xen_sysctl_tbuf_op {
    /* IN variables */
#define XEN_SYSCTL_TBUFOP_get_info     0
#define XEN_SYSCTL_TBUFOP_set_cpu_mask 1
#define XEN_SYSCTL_TBUFOP_set_evt_mask 2
#define XEN_SYSCTL_TBUFOP_set_size     3
#define XEN_SYSCTL_TBUFOP_enable       4
#define XEN_SYSCTL_TBUFOP_disable      5
    uint32_t cmd;
    /* IN/OUT variables */
    struct xenctl_bitmap cpu_mask;
    uint32_t             evt_mask;
    /* OUT variables */
    uint64_aligned_t buffer_mfn;
    uint32_t size;  /* Also an IN variable! */
};

/*
 * Get physical information about the host machine
 */
/* XEN_SYSCTL_physinfo */
 /* The platform supports HVM guests. */
#define _XEN_SYSCTL_PHYSCAP_hvm          0
#define XEN_SYSCTL_PHYSCAP_hvm           (1u<<_XEN_SYSCTL_PHYSCAP_hvm)
 /* The platform supports PV guests. */
#define _XEN_SYSCTL_PHYSCAP_pv           1
#define XEN_SYSCTL_PHYSCAP_pv            (1u<<_XEN_SYSCTL_PHYSCAP_pv)
 /* The platform supports direct access to I/O devices with IOMMU. */
#define _XEN_SYSCTL_PHYSCAP_directio     2
#define XEN_SYSCTL_PHYSCAP_directio  (1u<<_XEN_SYSCTL_PHYSCAP_directio)
/* The platform supports Hardware Assisted Paging. */
#define _XEN_SYSCTL_PHYSCAP_hap          3
#define XEN_SYSCTL_PHYSCAP_hap           (1u<<_XEN_SYSCTL_PHYSCAP_hap)
/* The platform supports software paging. */
#define _XEN_SYSCTL_PHYSCAP_shadow       4
#define XEN_SYSCTL_PHYSCAP_shadow        (1u<<_XEN_SYSCTL_PHYSCAP_shadow)
/* The platform supports sharing of HAP page tables with the IOMMU. */
#define _XEN_SYSCTL_PHYSCAP_iommu_hap_pt_share 5
#define XEN_SYSCTL_PHYSCAP_iommu_hap_pt_share  \
    (1u << _XEN_SYSCTL_PHYSCAP_iommu_hap_pt_share)
#define XEN_SYSCTL_PHYSCAP_vmtrace       (1u << 6)
/* The platform supports vPMU. */
#define XEN_SYSCTL_PHYSCAP_vpmu          (1u << 7)

/* Xen supports the Grant v1 and/or v2 ABIs. */
#define XEN_SYSCTL_PHYSCAP_gnttab_v1     (1u << 8)
#define XEN_SYSCTL_PHYSCAP_gnttab_v2     (1u << 9)

/* Max XEN_SYSCTL_PHYSCAP_* constant.  Used for ABI checking. */
#define XEN_SYSCTL_PHYSCAP_MAX XEN_SYSCTL_PHYSCAP_gnttab_v2

struct xen_sysctl_physinfo {
    uint32_t threads_per_core;
    uint32_t cores_per_socket;
    uint32_t nr_cpus;     /* # CPUs currently online */
    uint32_t max_cpu_id;  /* Largest possible CPU ID on this host */
    uint32_t nr_nodes;    /* # nodes currently online */
    uint32_t max_node_id; /* Largest possible node ID on this host */
    uint32_t cpu_khz;
    uint32_t capabilities;/* XEN_SYSCTL_PHYSCAP_??? */
    uint64_aligned_t total_pages;
    uint64_aligned_t free_pages;
    uint64_aligned_t scrub_pages;
    uint64_aligned_t outstanding_pages;
    uint64_aligned_t max_mfn; /* Largest possible MFN on this host */
    uint32_t hw_cap[8];
};

/*
 * Get the ID of the current scheduler.
 */
/* XEN_SYSCTL_sched_id */
struct xen_sysctl_sched_id {
    /* OUT variable */
    uint32_t sched_id;
};

/* Interface for controlling Xen software performance counters. */
/* XEN_SYSCTL_perfc_op */
/* Sub-operations: */
#define XEN_SYSCTL_PERFCOP_reset 1   /* Reset all counters to zero. */
#define XEN_SYSCTL_PERFCOP_query 2   /* Get perfctr information. */
struct xen_sysctl_perfc_desc {
    char         name[80];             /* name of perf counter */
    uint32_t     nr_vals;              /* number of values for this counter */
};
typedef struct xen_sysctl_perfc_desc xen_sysctl_perfc_desc_t;
DEFINE_XEN_GUEST_HANDLE(xen_sysctl_perfc_desc_t);
typedef uint32_t xen_sysctl_perfc_val_t;
DEFINE_XEN_GUEST_HANDLE(xen_sysctl_perfc_val_t);

struct xen_sysctl_perfc_op {
    /* IN variables. */
    uint32_t       cmd;                /*  XEN_SYSCTL_PERFCOP_??? */
    /* OUT variables. */
    uint32_t       nr_counters;       /*  number of counters description  */
    uint32_t       nr_vals;           /*  number of values  */
    /* counter information (or NULL) */
    XEN_GUEST_HANDLE_64(xen_sysctl_perfc_desc_t) desc;
    /* counter values (or NULL) */
    XEN_GUEST_HANDLE_64(xen_sysctl_perfc_val_t) val;
};

/* XEN_SYSCTL_getdomaininfolist */
struct xen_sysctl_getdomaininfolist {
    /* IN variables. */
    domid_t               first_domain;
    uint32_t              max_domains;
    XEN_GUEST_HANDLE_64(xen_domctl_getdomaininfo_t) buffer;
    /* OUT variables. */
    uint32_t              num_domains;
};

/* Inject debug keys into Xen. */
/* XEN_SYSCTL_debug_keys */
struct xen_sysctl_debug_keys {
    /* IN variables. */
    XEN_GUEST_HANDLE_64(const_char) keys;
    uint32_t nr_keys;
};

/* Get physical CPU information. */
/* XEN_SYSCTL_getcpuinfo */
struct xen_sysctl_cpuinfo {
    uint64_aligned_t idletime;
};
typedef struct xen_sysctl_cpuinfo xen_sysctl_cpuinfo_t;
DEFINE_XEN_GUEST_HANDLE(xen_sysctl_cpuinfo_t);
struct xen_sysctl_getcpuinfo {
    /* IN variables. */
    uint32_t max_cpus;
    XEN_GUEST_HANDLE_64(xen_sysctl_cpuinfo_t) info;
    /* OUT variables. */
    uint32_t nr_cpus;
};

/* XEN_SYSCTL_availheap */
struct xen_sysctl_availheap {
    /* IN variables. */
    uint32_t min_bitwidth;  /* Smallest address width (zero if don't care). */
    uint32_t max_bitwidth;  /* Largest address width (zero if don't care). */
    int32_t  node;          /* NUMA node of interest (-1 for all nodes). */
    /* OUT variables. */
    uint64_aligned_t avail_bytes;/* Bytes available in the specified region. */
};

/* XEN_SYSCTL_get_pmstat */
struct pm_px_val {
    uint64_aligned_t freq;        /* Px core frequency */
    uint64_aligned_t residency;   /* Px residency time */
    uint64_aligned_t count;       /* Px transition count */
};
typedef struct pm_px_val pm_px_val_t;
DEFINE_XEN_GUEST_HANDLE(pm_px_val_t);

struct pm_px_stat {
    uint8_t total;        /* total Px states */
    uint8_t usable;       /* usable Px states */
    uint8_t last;         /* last Px state */
    uint8_t cur;          /* current Px state */
    XEN_GUEST_HANDLE_64(uint64) trans_pt;   /* Px transition table */
    XEN_GUEST_HANDLE_64(pm_px_val_t) pt;
};

struct pm_cx_stat {
    uint32_t nr;    /* entry nr in triggers & residencies, including C0 */
    uint32_t last;  /* last Cx state */
    uint64_aligned_t idle_time;                 /* idle time from boot */
    XEN_GUEST_HANDLE_64(uint64) triggers;    /* Cx trigger counts */
    XEN_GUEST_HANDLE_64(uint64) residencies; /* Cx residencies */
    uint32_t nr_pc;                          /* entry nr in pc[] */
    uint32_t nr_cc;                          /* entry nr in cc[] */
    /*
     * These two arrays may (and generally will) have unused slots; slots not
     * having a corresponding hardware register will not be written by the
     * hypervisor. It is therefore up to the caller to put a suitable sentinel
     * into all slots before invoking the function.
     * Indexing is 1-biased (PC1/CC1 being at index 0).
     */
    XEN_GUEST_HANDLE_64(uint64) pc;
    XEN_GUEST_HANDLE_64(uint64) cc;
};

struct xen_sysctl_get_pmstat {
#define PMSTAT_CATEGORY_MASK 0xf0
#define PMSTAT_PX            0x10
#define PMSTAT_CX            0x20
#define PMSTAT_get_max_px    (PMSTAT_PX | 0x1)
#define PMSTAT_get_pxstat    (PMSTAT_PX | 0x2)
#define PMSTAT_reset_pxstat  (PMSTAT_PX | 0x3)
#define PMSTAT_get_max_cx    (PMSTAT_CX | 0x1)
#define PMSTAT_get_cxstat    (PMSTAT_CX | 0x2)
#define PMSTAT_reset_cxstat  (PMSTAT_CX | 0x3)
    uint32_t type;
    uint32_t cpuid;
    union {
        struct pm_px_stat getpx;
        struct pm_cx_stat getcx;
        /* other struct for tx, etc */
    } u;
};

/* XEN_SYSCTL_cpu_hotplug */
struct xen_sysctl_cpu_hotplug {
    /* IN variables */
    uint32_t cpu;   /* Physical cpu. */

    /* Single CPU enable/disable. */
#define XEN_SYSCTL_CPU_HOTPLUG_ONLINE  0
#define XEN_SYSCTL_CPU_HOTPLUG_OFFLINE 1

    /*
     * SMT enable/disable.
     *
     * These two ops loop over all present CPUs, and either online or offline
     * every non-primary sibling thread (those with a thread id which is not
     * 0).  This behaviour is chosen to simplify the implementation.
     *
     * They are intended as a shorthand for identifying and feeding the cpu
     * numbers individually to HOTPLUG_{ON,OFF}LINE.
     *
     * These are not expected to be used in conjunction with debugging options
     * such as `maxcpus=` or when other manual configuration of offline cpus
     * is in use.
     */
#define XEN_SYSCTL_CPU_HOTPLUG_SMT_ENABLE  2
#define XEN_SYSCTL_CPU_HOTPLUG_SMT_DISABLE 3
    uint32_t op;    /* hotplug opcode */
};

/*
 * Get/set xen power management, include
 * 1. cpufreq governors and related parameters
 */
/* XEN_SYSCTL_pm_op */
struct xen_userspace {
    uint32_t scaling_setspeed;
};

struct xen_ondemand {
    uint32_t sampling_rate_max;
    uint32_t sampling_rate_min;

    uint32_t sampling_rate;
    uint32_t up_threshold;
};

/*
 * cpufreq para name of this structure named
 * same as sysfs file name of native linux
 */
#define CPUFREQ_NAME_LEN 16
struct xen_get_cpufreq_para {
    /* IN/OUT variable */
    uint32_t cpu_num;
    uint32_t freq_num;
    uint32_t gov_num;

    /* for all governors */
    /* OUT variable */
    XEN_GUEST_HANDLE_64(uint32) affected_cpus;
    XEN_GUEST_HANDLE_64(uint32) scaling_available_frequencies;
    XEN_GUEST_HANDLE_64(char)   scaling_available_governors;
    char scaling_driver[CPUFREQ_NAME_LEN];

    uint32_t cpuinfo_cur_freq;
    uint32_t cpuinfo_max_freq;
    uint32_t cpuinfo_min_freq;
    uint32_t scaling_cur_freq;

    char scaling_governor[CPUFREQ_NAME_LEN];
    uint32_t scaling_max_freq;
    uint32_t scaling_min_freq;

    /* for specific governor */
    union {
        struct  xen_userspace userspace;
        struct  xen_ondemand ondemand;
    } u;

    int32_t turbo_enabled;
};

struct xen_set_cpufreq_gov {
    char scaling_governor[CPUFREQ_NAME_LEN];
};

struct xen_set_cpufreq_para {
    #define SCALING_MAX_FREQ           1
    #define SCALING_MIN_FREQ           2
    #define SCALING_SETSPEED           3
    #define SAMPLING_RATE              4
    #define UP_THRESHOLD               5

    uint32_t ctrl_type;
    uint32_t ctrl_value;
};

struct xen_sysctl_pm_op {
    #define PM_PARA_CATEGORY_MASK      0xf0
    #define CPUFREQ_PARA               0x10

    /* cpufreq command type */
    #define GET_CPUFREQ_PARA           (CPUFREQ_PARA | 0x01)
    #define SET_CPUFREQ_GOV            (CPUFREQ_PARA | 0x02)
    #define SET_CPUFREQ_PARA           (CPUFREQ_PARA | 0x03)
    #define GET_CPUFREQ_AVGFREQ        (CPUFREQ_PARA | 0x04)

    /* set/reset scheduler power saving option */
    #define XEN_SYSCTL_pm_op_set_sched_opt_smt    0x21

    /*
     * cpuidle max C-state and max C-sub-state access command:
     * Set cpuid to 0 for max C-state.
     * Set cpuid to 1 for max C-sub-state.
     */
    #define XEN_SYSCTL_pm_op_get_max_cstate       0x22
    #define XEN_SYSCTL_pm_op_set_max_cstate       0x23

    /* set scheduler migration cost value */
    #define XEN_SYSCTL_pm_op_set_vcpu_migration_delay   0x24
    #define XEN_SYSCTL_pm_op_get_vcpu_migration_delay   0x25

    /* enable/disable turbo mode when in dbs governor */
    #define XEN_SYSCTL_pm_op_enable_turbo               0x26
    #define XEN_SYSCTL_pm_op_disable_turbo              0x27

    uint32_t cmd;
    uint32_t cpuid;
    union {
        struct xen_get_cpufreq_para get_para;
        struct xen_set_cpufreq_gov  set_gov;
        struct xen_set_cpufreq_para set_para;
        uint64_aligned_t get_avgfreq;
        uint32_t                    set_sched_opt_smt;
#define XEN_SYSCTL_CX_UNLIMITED 0xffffffff
        uint32_t                    get_max_cstate;
        uint32_t                    set_max_cstate;
    } u;
};

/* XEN_SYSCTL_page_offline_op */
struct xen_sysctl_page_offline_op {
    /* IN: range of page to be offlined */
#define sysctl_page_offline     1
#define sysctl_page_online      2
#define sysctl_query_page_offline  3
    uint32_t cmd;
    uint32_t start;
    uint32_t end;
    /* OUT: result of page offline request */
    /*
     * bit 0~15: result flags
     * bit 16~31: owner
     */
    XEN_GUEST_HANDLE(uint32) status;
};

#define PG_OFFLINE_STATUS_MASK    (0xFFUL)

/* The result is invalid, i.e. HV does not handle it */
#define PG_OFFLINE_INVALID   (0x1UL << 0)

#define PG_OFFLINE_OFFLINED  (0x1UL << 1)
#define PG_OFFLINE_PENDING   (0x1UL << 2)
#define PG_OFFLINE_FAILED    (0x1UL << 3)
#define PG_OFFLINE_AGAIN     (0x1UL << 4)

#define PG_ONLINE_FAILED     PG_OFFLINE_FAILED
#define PG_ONLINE_ONLINED    PG_OFFLINE_OFFLINED

#define PG_OFFLINE_STATUS_OFFLINED              (0x1UL << 1)
#define PG_OFFLINE_STATUS_ONLINE                (0x1UL << 2)
#define PG_OFFLINE_STATUS_OFFLINE_PENDING       (0x1UL << 3)
#define PG_OFFLINE_STATUS_BROKEN                (0x1UL << 4)

#define PG_OFFLINE_MISC_MASK    (0xFFUL << 4)

/* valid when PG_OFFLINE_FAILED or PG_OFFLINE_PENDING */
#define PG_OFFLINE_XENPAGE   (0x1UL << 8)
#define PG_OFFLINE_DOM0PAGE  (0x1UL << 9)
#define PG_OFFLINE_ANONYMOUS (0x1UL << 10)
#define PG_OFFLINE_NOT_CONV_RAM   (0x1UL << 11)
#define PG_OFFLINE_OWNED     (0x1UL << 12)

#define PG_OFFLINE_BROKEN    (0x1UL << 13)
#define PG_ONLINE_BROKEN     PG_OFFLINE_BROKEN

#define PG_OFFLINE_OWNER_SHIFT 16

/* XEN_SYSCTL_lockprof_op */
/* Sub-operations: */
#define XEN_SYSCTL_LOCKPROF_reset 1   /* Reset all profile data to zero. */
#define XEN_SYSCTL_LOCKPROF_query 2   /* Get lock profile information. */
/* Record-type: */
#define LOCKPROF_TYPE_GLOBAL      0   /* global lock, idx meaningless */
#define LOCKPROF_TYPE_PERDOM      1   /* per-domain lock, idx is domid */
#define LOCKPROF_TYPE_N           2   /* number of types */
struct xen_sysctl_lockprof_data {
    char     name[40];     /* lock name (may include up to 2 %d specifiers) */
    int32_t  type;         /* LOCKPROF_TYPE_??? */
    int32_t  idx;          /* index (e.g. domain id) */
    uint64_aligned_t lock_cnt;     /* # of locking succeeded */
    uint64_aligned_t block_cnt;    /* # of wait for lock */
    uint64_aligned_t lock_time;    /* nsecs lock held */
    uint64_aligned_t block_time;   /* nsecs waited for lock */
};
typedef struct xen_sysctl_lockprof_data xen_sysctl_lockprof_data_t;
DEFINE_XEN_GUEST_HANDLE(xen_sysctl_lockprof_data_t);
struct xen_sysctl_lockprof_op {
    /* IN variables. */
    uint32_t       cmd;               /* XEN_SYSCTL_LOCKPROF_??? */
    uint32_t       max_elem;          /* size of output buffer */
    /* OUT variables (query only). */
    uint32_t       nr_elem;           /* number of elements available */
    uint64_aligned_t time;            /* nsecs of profile measurement */
    /* profile information (or NULL) */
    XEN_GUEST_HANDLE_64(xen_sysctl_lockprof_data_t) data;
};

/* XEN_SYSCTL_cputopoinfo */
#define XEN_INVALID_CORE_ID     (~0U)
#define XEN_INVALID_SOCKET_ID   (~0U)
#define XEN_INVALID_NODE_ID     (~0U)

struct xen_sysctl_cputopo {
    uint32_t core;
    uint32_t socket;
    uint32_t node;
};
typedef struct xen_sysctl_cputopo xen_sysctl_cputopo_t;
DEFINE_XEN_GUEST_HANDLE(xen_sysctl_cputopo_t);

/*
 * IN:
 *  - a NULL 'cputopo' handle is a request for maximun 'num_cpus'.
 *  - otherwise it's the number of entries in 'cputopo'
 *
 * OUT:
 *  - If 'num_cpus' is less than the number Xen wants to write but the handle
 *    handle is not a NULL one, partial data gets returned and 'num_cpus' gets
 *    updated to reflect the intended number.
 *  - Otherwise, 'num_cpus' shall indicate the number of entries written, which
 *    may be less than the input value.
 */
struct xen_sysctl_cputopoinfo {
    uint32_t num_cpus;
    XEN_GUEST_HANDLE_64(xen_sysctl_cputopo_t) cputopo;
};

/* XEN_SYSCTL_numainfo */
#define XEN_INVALID_MEM_SZ     (~0U)
#define XEN_INVALID_NODE_DIST  (~0U)

struct xen_sysctl_meminfo {
    uint64_t memsize;
    uint64_t memfree;
};
typedef struct xen_sysctl_meminfo xen_sysctl_meminfo_t;
DEFINE_XEN_GUEST_HANDLE(xen_sysctl_meminfo_t);

/*
 * IN:
 *  - Both 'meminfo' and 'distance' handles being null is a request
 *    for maximum value of 'num_nodes'.
 *  - Otherwise it's the number of entries in 'meminfo' and square root
 *    of number of entries in 'distance' (when corresponding handle is
 *    non-null)
 *
 * OUT:
 *  - If 'num_nodes' is less than the number Xen wants to write but either
 *    handle is not a NULL one, partial data gets returned and 'num_nodes'
 *    gets updated to reflect the intended number.
 *  - Otherwise, 'num_nodes' shall indicate the number of entries written, which
 *    may be less than the input value.
 */

struct xen_sysctl_numainfo {
    uint32_t num_nodes;

    XEN_GUEST_HANDLE_64(xen_sysctl_meminfo_t) meminfo;

    /*
     * Distance between nodes 'i' and 'j' is stored in index 'i*N + j',
     * where N is the number of nodes that will be returned in 'num_nodes'
     * (i.e. not 'num_nodes' provided by the caller)
     */
    XEN_GUEST_HANDLE_64(uint32) distance;
};

/* XEN_SYSCTL_cpupool_op */
#define XEN_SYSCTL_CPUPOOL_OP_CREATE                1  /* C */
#define XEN_SYSCTL_CPUPOOL_OP_DESTROY               2  /* D */
#define XEN_SYSCTL_CPUPOOL_OP_INFO                  3  /* I */
#define XEN_SYSCTL_CPUPOOL_OP_ADDCPU                4  /* A */
#define XEN_SYSCTL_CPUPOOL_OP_RMCPU                 5  /* R */
#define XEN_SYSCTL_CPUPOOL_OP_MOVEDOMAIN            6  /* M */
#define XEN_SYSCTL_CPUPOOL_OP_FREEINFO              7  /* F */
#define XEN_SYSCTL_CPUPOOL_PAR_ANY     0xFFFFFFFF
struct xen_sysctl_cpupool_op {
    uint32_t op;          /* IN */
    uint32_t cpupool_id;  /* IN: CDIARM OUT: CI */
    uint32_t sched_id;    /* IN: C      OUT: I  */
    uint32_t domid;       /* IN: M              */
    uint32_t cpu;         /* IN: AR             */
    uint32_t n_dom;       /*            OUT: I  */
    struct xenctl_bitmap cpumap; /*     OUT: IF */
};

/*
 * Error return values of cpupool operations:
 *
 * -EADDRINUSE:
 *  XEN_SYSCTL_CPUPOOL_OP_RMCPU: A vcpu is temporarily pinned to the cpu
 *    which is to be removed from a cpupool.
 * -EADDRNOTAVAIL:
 *  XEN_SYSCTL_CPUPOOL_OP_ADDCPU, XEN_SYSCTL_CPUPOOL_OP_RMCPU: A previous
 *    request to remove a cpu from a cpupool was terminated with -EAGAIN
 *    and has not been retried using the same parameters.
 * -EAGAIN:
 *  XEN_SYSCTL_CPUPOOL_OP_RMCPU: The cpu can't be removed from the cpupool
 *    as it is active in the hypervisor. A retry will succeed soon.
 * -EBUSY:
 *  XEN_SYSCTL_CPUPOOL_OP_DESTROY, XEN_SYSCTL_CPUPOOL_OP_RMCPU: A cpupool
 *    can't be destroyed or the last cpu can't be removed as there is still
 *    a running domain in that cpupool.
 * -EEXIST:
 *  XEN_SYSCTL_CPUPOOL_OP_CREATE: A cpupool_id was specified and is already
 *    existing.
 * -EINVAL:
 *  XEN_SYSCTL_CPUPOOL_OP_ADDCPU, XEN_SYSCTL_CPUPOOL_OP_RMCPU: An illegal
 *    cpu was specified (cpu does not exist).
 *  XEN_SYSCTL_CPUPOOL_OP_MOVEDOMAIN: An illegal domain was specified
 *    (domain id illegal or not suitable for operation).
 * -ENODEV:
 *  XEN_SYSCTL_CPUPOOL_OP_ADDCPU, XEN_SYSCTL_CPUPOOL_OP_RMCPU: The specified
 *    cpu is either not free (add) or not member of the specified cpupool
 *    (remove).
 * -ENOENT:
 *  all: The cpupool with the specified cpupool_id doesn't exist.
 *
 * Some common error return values like -ENOMEM and -EFAULT are possible for
 * all the operations.
 */

#define ARINC653_MAX_DOMAINS_PER_SCHEDULE   64
/*
 * This structure is used to pass a new ARINC653 schedule from a
 * privileged domain (ie dom0) to Xen.
 */
struct xen_sysctl_arinc653_schedule {
    /* major_frame holds the time for the new schedule's major frame
     * in nanoseconds. */
    uint64_aligned_t     major_frame;
    /* num_sched_entries holds how many of the entries in the
     * sched_entries[] array are valid. */
    uint8_t     num_sched_entries;
    /* The sched_entries array holds the actual schedule entries. */
    struct {
        /* dom_handle must match a domain's UUID */
        xen_domain_handle_t dom_handle;
        /* If a domain has multiple VCPUs, vcpu_id specifies which one
         * this schedule entry applies to. It should be set to 0 if
         * there is only one VCPU for the domain. */
        unsigned int vcpu_id;
        /* runtime specifies the amount of time that should be allocated
         * to this VCPU per major frame. It is specified in nanoseconds */
        uint64_aligned_t runtime;
    } sched_entries[ARINC653_MAX_DOMAINS_PER_SCHEDULE];
};
typedef struct xen_sysctl_arinc653_schedule xen_sysctl_arinc653_schedule_t;
DEFINE_XEN_GUEST_HANDLE(xen_sysctl_arinc653_schedule_t);

/*
 * Valid range for context switch rate limit (in microseconds).
 * Applicable to Credit and Credit2 schedulers.
 */
#define XEN_SYSCTL_SCHED_RATELIMIT_MAX 500000
#define XEN_SYSCTL_SCHED_RATELIMIT_MIN 100

struct xen_sysctl_credit_schedule {
    /* Length of timeslice in milliseconds */
#define XEN_SYSCTL_CSCHED_TSLICE_MAX 1000
#define XEN_SYSCTL_CSCHED_TSLICE_MIN 1
    unsigned tslice_ms;
    unsigned ratelimit_us;
    /*
     * How long we consider a vCPU to be cache-hot on the
     * CPU where it has run (max 100ms, in microseconds)
    */
#define XEN_SYSCTL_CSCHED_MGR_DLY_MAX_US (100 * 1000)
    unsigned vcpu_migr_delay_us;
};

struct xen_sysctl_credit2_schedule {
    unsigned ratelimit_us;
};

/* XEN_SYSCTL_scheduler_op */
/* Set or get info? */
#define XEN_SYSCTL_SCHEDOP_putinfo 0
#define XEN_SYSCTL_SCHEDOP_getinfo 1
struct xen_sysctl_scheduler_op {
    uint32_t cpupool_id; /* Cpupool whose scheduler is to be targetted. */
    uint32_t sched_id;   /* XEN_SCHEDULER_* (domctl.h) */
    uint32_t cmd;        /* XEN_SYSCTL_SCHEDOP_* */
    union {
        struct xen_sysctl_sched_arinc653 {
            XEN_GUEST_HANDLE_64(xen_sysctl_arinc653_schedule_t) schedule;
        } sched_arinc653;
        struct xen_sysctl_credit_schedule sched_credit;
        struct xen_sysctl_credit2_schedule sched_credit2;
    } u;
};

/*
 * Output format of gcov data:
 *
 * XEN_GCOV_FORMAT_MAGIC XEN_GCOV_RECORD ... XEN_GCOV_RECORD
 *
 * That is, one magic number followed by 0 or more record.
 *
 * The magic number is stored as an uint32_t field.
 *
 * The record is packed and variable in length. It has the form:
 *
 *  filename: a NULL terminated path name extracted from gcov, used to
 *            create the name of gcda file.
 *  size:     a uint32_t field indicating the size of the payload, the
 *            unit is byte.
 *  payload:  the actual payload, length is `size' bytes.
 *
 * Userspace tool will split the record to different files.
 */

#define XEN_GCOV_FORMAT_MAGIC    0x58434f56 /* XCOV */

/*
 * Ouput format of LLVM coverage data is just a raw stream, as would be
 * written by the compiler_rt run time library into a .profraw file. There
 * are no special Xen tags or delimiters because none are needed.
 */

#define XEN_SYSCTL_COVERAGE_get_size 0 /* Get total size of output data */
#define XEN_SYSCTL_COVERAGE_read     1 /* Read output data */
#define XEN_SYSCTL_COVERAGE_reset    2 /* Reset all counters */

struct xen_sysctl_coverage_op {
    uint32_t cmd;
    uint32_t size; /* IN/OUT: size of the buffer  */
    XEN_GUEST_HANDLE_64(char) buffer; /* OUT */
};

#define XEN_SYSCTL_PSR_CMT_get_total_rmid            0
#define XEN_SYSCTL_PSR_CMT_get_l3_upscaling_factor   1
/* The L3 cache size is returned in KB unit */
#define XEN_SYSCTL_PSR_CMT_get_l3_cache_size         2
#define XEN_SYSCTL_PSR_CMT_enabled                   3
#define XEN_SYSCTL_PSR_CMT_get_l3_event_mask         4
struct xen_sysctl_psr_cmt_op {
    uint32_t cmd;       /* IN: XEN_SYSCTL_PSR_CMT_* */
    uint32_t flags;     /* padding variable, may be extended for future use */
    union {
        uint64_t data;  /* OUT */
        struct {
            uint32_t cpu;   /* IN */
            uint32_t rsvd;
        } l3_cache;
    } u;
};

/* XEN_SYSCTL_pcitopoinfo */
#define XEN_INVALID_DEV (XEN_INVALID_NODE_ID - 1)
struct xen_sysctl_pcitopoinfo {
    /*
     * IN: Number of elements in 'devs' and 'nodes' arrays.
     * OUT: Number of processed elements of those arrays.
     */
    uint32_t num_devs;

    /* IN: list of devices for which node IDs are requested. */
    XEN_GUEST_HANDLE_64(physdev_pci_device_t) devs;

    /*
     * OUT: node identifier for each device.
     * If information for a particular device is not available then
     * corresponding entry will be set to XEN_INVALID_NODE_ID. If
     * device is not known to the hypervisor then XEN_INVALID_DEV
     * will be provided.
     */
    XEN_GUEST_HANDLE_64(uint32) nodes;
};

#define XEN_SYSCTL_PSR_get_l3_info               0
#define XEN_SYSCTL_PSR_get_l2_info               1
#define XEN_SYSCTL_PSR_get_mba_info              2
struct xen_sysctl_psr_alloc {
    uint32_t cmd;       /* IN: XEN_SYSCTL_PSR_* */
    uint32_t target;    /* IN */
    union {
        struct {
            uint32_t cbm_len;   /* OUT: CBM length */
            uint32_t cos_max;   /* OUT: Maximum COS */
#define XEN_SYSCTL_PSR_CAT_L3_CDP       (1u << 0)
            uint32_t flags;     /* OUT: CAT flags */
        } cat_info;

        struct {
            uint32_t thrtl_max; /* OUT: Maximum throttle */
            uint32_t cos_max;   /* OUT: Maximum COS */
#define XEN_SYSCTL_PSR_MBA_LINEAR      (1u << 0)
            uint32_t flags;     /* OUT: MBA flags */
        } mba_info;
    } u;
};

/*
 * XEN_SYSCTL_get_cpu_levelling_caps (x86 specific)
 *
 * Return hardware capabilities concerning masking or faulting of the cpuid
 * instruction for PV guests.
 */
struct xen_sysctl_cpu_levelling_caps {
#define XEN_SYSCTL_CPU_LEVELCAP_faulting    (1ul <<  0) /* CPUID faulting    */
#define XEN_SYSCTL_CPU_LEVELCAP_ecx         (1ul <<  1) /* 0x00000001.ecx    */
#define XEN_SYSCTL_CPU_LEVELCAP_edx         (1ul <<  2) /* 0x00000001.edx    */
#define XEN_SYSCTL_CPU_LEVELCAP_extd_ecx    (1ul <<  3) /* 0x80000001.ecx    */
#define XEN_SYSCTL_CPU_LEVELCAP_extd_edx    (1ul <<  4) /* 0x80000001.edx    */
#define XEN_SYSCTL_CPU_LEVELCAP_xsave_eax   (1ul <<  5) /* 0x0000000D:1.eax  */
#define XEN_SYSCTL_CPU_LEVELCAP_thermal_ecx (1ul <<  6) /* 0x00000006.ecx    */
#define XEN_SYSCTL_CPU_LEVELCAP_l7s0_eax    (1ul <<  7) /* 0x00000007:0.eax  */
#define XEN_SYSCTL_CPU_LEVELCAP_l7s0_ebx    (1ul <<  8) /* 0x00000007:0.ebx  */
    uint32_t caps;
};

/*
 * XEN_SYSCTL_get_cpu_featureset (x86 specific)
 *
 * Return information about featuresets available on this host.
 *  -  Raw: The real cpuid values.
 *  - Host: The values Xen is using, (after command line overrides, etc).
 *  -   PV: Maximum set of features which can be given to a PV guest.
 *  -  HVM: Maximum set of features which can be given to a HVM guest.
 * May fail with -EOPNOTSUPP if querying for PV or HVM data when support is
 * compiled out of Xen.
 */
struct xen_sysctl_cpu_featureset {
#define XEN_SYSCTL_cpu_featureset_raw      0
#define XEN_SYSCTL_cpu_featureset_host     1
#define XEN_SYSCTL_cpu_featureset_pv       2
#define XEN_SYSCTL_cpu_featureset_hvm      3
    uint32_t index;       /* IN: Which featureset to query? */
    uint32_t nr_features; /* IN/OUT: Number of entries in/written to
                           * 'features', or the maximum number of features if
                           * the guest handle is NULL.  NB. All featuresets
                           * come from the same numberspace, so have the same
                           * maximum length. */
    XEN_GUEST_HANDLE_64(uint32) features; /* OUT: */
};

/*
 * XEN_SYSCTL_LIVEPATCH_op
 *
 * Refer to the docs/unstable/misc/livepatch.markdown
 * for the design details of this hypercall.
 *
 * There are four sub-ops:
 *  XEN_SYSCTL_LIVEPATCH_UPLOAD (0)
 *  XEN_SYSCTL_LIVEPATCH_GET (1)
 *  XEN_SYSCTL_LIVEPATCH_LIST (2)
 *  XEN_SYSCTL_LIVEPATCH_ACTION (3)
 *
 * The normal sequence of sub-ops is to:
 *  1) XEN_SYSCTL_LIVEPATCH_UPLOAD to upload the payload. If errors STOP.
 *  2) XEN_SYSCTL_LIVEPATCH_GET to check the `->rc`. If -XEN_EAGAIN spin.
 *     If zero go to next step.
 *  3) XEN_SYSCTL_LIVEPATCH_ACTION with LIVEPATCH_ACTION_APPLY to apply the patch.
 *  4) XEN_SYSCTL_LIVEPATCH_GET to check the `->rc`. If in -XEN_EAGAIN spin.
 *     If zero exit with success.
 */

#define LIVEPATCH_PAYLOAD_VERSION 2
/*
 * .livepatch.funcs structure layout defined in the `Payload format`
 * section in the Live Patch design document.
 *
 * We guard this with __XEN__ as toolstacks SHOULD not use it.
 */
#ifdef __XEN__
#define LIVEPATCH_OPAQUE_SIZE 31

struct livepatch_expectation {
    uint8_t enabled : 1;
    uint8_t len : 5;        /* Length of data up to LIVEPATCH_OPAQUE_SIZE
                               (5 bits is enough for now) */
    uint8_t rsv : 2;        /* Reserved. Zero value */
    uint8_t data[LIVEPATCH_OPAQUE_SIZE]; /* Same size as opaque[] buffer of
                                            struct livepatch_func. This is the
                                            max number of bytes to be patched */
};
typedef struct livepatch_expectation livepatch_expectation_t;

typedef enum livepatch_func_state {
    LIVEPATCH_FUNC_NOT_APPLIED,
    LIVEPATCH_FUNC_APPLIED
} livepatch_func_state_t;

struct livepatch_func {
    const char *name;       /* Name of function to be patched. */
    void *new_addr;
    void *old_addr;
    uint32_t new_size;
    uint32_t old_size;
    uint8_t version;        /* MUST be LIVEPATCH_PAYLOAD_VERSION. */
    uint8_t opaque[LIVEPATCH_OPAQUE_SIZE];
    uint8_t applied;
    uint8_t _pad[7];
    livepatch_expectation_t expect;
};
typedef struct livepatch_func livepatch_func_t;
#endif

/*
 * Structure describing an ELF payload. Uniquely identifies the
 * payload. Should be human readable.
 * Recommended length is upto XEN_LIVEPATCH_NAME_SIZE.
 * Includes the NUL terminator.
 */
#define XEN_LIVEPATCH_NAME_SIZE 128
struct xen_livepatch_name {
    XEN_GUEST_HANDLE_64(char) name;         /* IN: pointer to name. */
    uint16_t size;                          /* IN: size of name. May be upto
                                               XEN_LIVEPATCH_NAME_SIZE. */
    uint16_t pad[3];                        /* IN: MUST be zero. */
};

/*
 * Upload a payload to the hypervisor. The payload is verified
 * against basic checks and if there are any issues the proper return code
 * will be returned. The payload is not applied at this time - that is
 * controlled by XEN_SYSCTL_LIVEPATCH_ACTION.
 *
 * The return value is zero if the payload was succesfully uploaded.
 * Otherwise an EXX return value is provided. Duplicate `name` are not
 * supported.
 *
 * The payload at this point is verified against basic checks.
 *
 * The `payload` is the ELF payload as mentioned in the `Payload format`
 * section in the Live Patch design document.
 */
#define XEN_SYSCTL_LIVEPATCH_UPLOAD 0
struct xen_sysctl_livepatch_upload {
    struct xen_livepatch_name name;         /* IN, name of the patch. */
    uint64_t size;                          /* IN, size of the ELF file. */
    XEN_GUEST_HANDLE_64(uint8) payload;     /* IN, the ELF file. */
};

/*
 * Retrieve an status of an specific payload.
 *
 * Upon completion the `struct xen_livepatch_status` is updated.
 *
 * The return value is zero on success and XEN_EXX on failure. This operation
 * is synchronous and does not require preemption.
 */
#define XEN_SYSCTL_LIVEPATCH_GET 1

struct xen_livepatch_status {
#define LIVEPATCH_STATE_CHECKED      1
#define LIVEPATCH_STATE_APPLIED      2
    uint32_t state;                /* OUT: LIVEPATCH_STATE_*. */
    int32_t rc;                    /* OUT: 0 if no error, otherwise -XEN_EXX. */
};
typedef struct xen_livepatch_status xen_livepatch_status_t;
DEFINE_XEN_GUEST_HANDLE(xen_livepatch_status_t);

struct xen_sysctl_livepatch_get {
    struct xen_livepatch_name name;         /* IN, name of the payload. */
    struct xen_livepatch_status status;     /* IN/OUT, state of it. */
};

/*
 * Retrieve an array of abbreviated status, names and metadata of payloads that
 * are loaded in the hypervisor.
 *
 * If the hypercall returns an positive number, it is the number (up to `nr`)
 * of the payloads returned, along with `nr` updated with the number of remaining
 * payloads, `version` updated (it may be the same across hypercalls. If it varies
 * the data is stale and further calls could fail), `name_total_size` and
 * `metadata_total_size` containing total sizes of transferred data for both the
 * arrays.
 * The `status`, `name`, `len`, `metadata` and `metadata_len` are updated at their
 * designed index value (`idx`) with the returned value of data.
 *
 * If the hypercall returns E2BIG the `nr` is too big and should be
 * lowered. The upper limit of `nr` is left to the implemention.
 *
 * Note that due to the asynchronous nature of hypercalls the domain might have
 * added or removed the number of payloads making this information stale. It is
 * the responsibility of the toolstack to use the `version` field to check
 * between each invocation. if the version differs it should discard the stale
 * data and start from scratch. It is OK for the toolstack to use the new
 * `version` field.
 */
#define XEN_SYSCTL_LIVEPATCH_LIST 2
struct xen_sysctl_livepatch_list {
    uint32_t version;                       /* OUT: Hypervisor stamps value.
                                               If varies between calls, we are
                                             * getting stale data. */
    uint32_t idx;                           /* IN: Index into hypervisor list. */
    uint32_t nr;                            /* IN: How many status, name, and len
                                               should fill out. Can be zero to get
                                               amount of payloads and version.
                                               OUT: How many payloads left. */
    uint32_t pad;                           /* IN: Must be zero. */
    uint32_t name_total_size;               /* OUT: Total size of all transfer names */
    uint32_t metadata_total_size;           /* OUT: Total size of all transfer metadata */
    XEN_GUEST_HANDLE_64(xen_livepatch_status_t) status;  /* OUT. Must have enough
                                               space allocate for nr of them. */
    XEN_GUEST_HANDLE_64(char) name;         /* OUT: Array of names. Each member
                                               may have an arbitrary length up to
                                               XEN_LIVEPATCH_NAME_SIZE bytes. Must have
                                               nr of them. */
    XEN_GUEST_HANDLE_64(uint32) len;        /* OUT: Array of lengths of name's.
                                               Must have nr of them. */
    XEN_GUEST_HANDLE_64(char) metadata;     /* OUT: Array of metadata strings. Each
                                               member may have an arbitrary length.
                                               Must have nr of them. */
    XEN_GUEST_HANDLE_64(uint32) metadata_len;  /* OUT: Array of lengths of metadata's.
                                                  Must have nr of them. */
};

/*
 * Perform an operation on the payload structure referenced by the `name` field.
 * The operation request is asynchronous and the status should be retrieved
 * by using either XEN_SYSCTL_LIVEPATCH_GET or XEN_SYSCTL_LIVEPATCH_LIST hypercall.
 */
#define XEN_SYSCTL_LIVEPATCH_ACTION 3
struct xen_sysctl_livepatch_action {
    struct xen_livepatch_name name;         /* IN, name of the patch. */
#define LIVEPATCH_ACTION_UNLOAD       1
#define LIVEPATCH_ACTION_REVERT       2
#define LIVEPATCH_ACTION_APPLY        3
#define LIVEPATCH_ACTION_REPLACE      4
    uint32_t cmd;                           /* IN: LIVEPATCH_ACTION_*. */
    uint32_t timeout;                       /* IN: If zero then uses */
                                            /* hypervisor default. */
                                            /* Or upper bound of time (ns) */
                                            /* for operation to take. */

/*
 * Override default inter-module buildid dependency chain enforcement.
 * Check only if module is built for given hypervisor by comparing buildid.
 */
#define LIVEPATCH_ACTION_APPLY_NODEPS (1 << 0)
    uint32_t flags;                         /* IN: action flags. */
                                            /* Provide additional parameters */
                                            /* for an action. */
    uint32_t pad;                           /* IN: Always zero. */
};

struct xen_sysctl_livepatch_op {
    uint32_t cmd;                           /* IN: XEN_SYSCTL_LIVEPATCH_*. */
    uint32_t pad;                           /* IN: Always zero. */
    union {
        struct xen_sysctl_livepatch_upload upload;
        struct xen_sysctl_livepatch_list list;
        struct xen_sysctl_livepatch_get get;
        struct xen_sysctl_livepatch_action action;
    } u;
};

#if defined(__i386__) || defined(__x86_64__)
/*
 * XEN_SYSCTL_get_cpu_policy (x86 specific)
 *
 * Return information about CPUID and MSR policies available on this host.
 *  -       Raw: The real H/W values.
 *  -      Host: The values Xen is using, (after command line overrides, etc).
 *  -     Max_*: Maximum set of features a PV or HVM guest can use.  Includes
 *               experimental features outside of security support.
 *  - Default_*: Default set of features a PV or HVM guest can use.  This is
 *               the security supported set.
 * May fail with -EOPNOTSUPP if querying for PV or HVM data when support is
 * compiled out of Xen.
 */
struct xen_sysctl_cpu_policy {
#define XEN_SYSCTL_cpu_policy_raw          0
#define XEN_SYSCTL_cpu_policy_host         1
#define XEN_SYSCTL_cpu_policy_pv_max       2
#define XEN_SYSCTL_cpu_policy_hvm_max      3
#define XEN_SYSCTL_cpu_policy_pv_default   4
#define XEN_SYSCTL_cpu_policy_hvm_default  5
    uint32_t index;       /* IN: Which policy to query? */
    uint32_t nr_leaves;   /* IN/OUT: Number of leaves in/written to
                           * 'cpuid_policy', or the maximum number of leaves
                           * if the guest handle is NULL. */
    uint32_t nr_msrs;     /* IN/OUT: Number of MSRs in/written to
                           * 'msr_policy', or the maximum number of MSRs if
                           * the guest handle is NULL. */
    uint32_t _rsvd;       /* Must be zero. */
    XEN_GUEST_HANDLE_64(xen_cpuid_leaf_t) cpuid_policy; /* OUT */
    XEN_GUEST_HANDLE_64(xen_msr_entry_t) msr_policy;    /* OUT */
};
typedef struct xen_sysctl_cpu_policy xen_sysctl_cpu_policy_t;
DEFINE_XEN_GUEST_HANDLE(xen_sysctl_cpu_policy_t);
#endif

struct xen_sysctl {
    uint32_t cmd;
#define XEN_SYSCTL_readconsole                    1
#define XEN_SYSCTL_tbuf_op                        2
#define XEN_SYSCTL_physinfo                       3
#define XEN_SYSCTL_sched_id                       4
#define XEN_SYSCTL_perfc_op                       5
#define XEN_SYSCTL_getdomaininfolist              6
#define XEN_SYSCTL_debug_keys                     7
#define XEN_SYSCTL_getcpuinfo                     8
#define XEN_SYSCTL_availheap                      9
#define XEN_SYSCTL_get_pmstat                    10
#define XEN_SYSCTL_cpu_hotplug                   11
#define XEN_SYSCTL_pm_op                         12
#define XEN_SYSCTL_page_offline_op               14
#define XEN_SYSCTL_lockprof_op                   15
#define XEN_SYSCTL_cputopoinfo                   16
#define XEN_SYSCTL_numainfo                      17
#define XEN_SYSCTL_cpupool_op                    18
#define XEN_SYSCTL_scheduler_op                  19
#define XEN_SYSCTL_coverage_op                   20
#define XEN_SYSCTL_psr_cmt_op                    21
#define XEN_SYSCTL_pcitopoinfo                   22
#define XEN_SYSCTL_psr_alloc                     23
/* #define XEN_SYSCTL_tmem_op                       24 */
#define XEN_SYSCTL_get_cpu_levelling_caps        25
#define XEN_SYSCTL_get_cpu_featureset            26
#define XEN_SYSCTL_livepatch_op                  27
/* #define XEN_SYSCTL_set_parameter              28 */
#define XEN_SYSCTL_get_cpu_policy                29
    uint32_t interface_version; /* XEN_SYSCTL_INTERFACE_VERSION */
    union {
        struct xen_sysctl_readconsole       readconsole;
        struct xen_sysctl_tbuf_op           tbuf_op;
        struct xen_sysctl_physinfo          physinfo;
        struct xen_sysctl_cputopoinfo       cputopoinfo;
        struct xen_sysctl_pcitopoinfo       pcitopoinfo;
        struct xen_sysctl_numainfo          numainfo;
        struct xen_sysctl_sched_id          sched_id;
        struct xen_sysctl_perfc_op          perfc_op;
        struct xen_sysctl_getdomaininfolist getdomaininfolist;
        struct xen_sysctl_debug_keys        debug_keys;
        struct xen_sysctl_getcpuinfo        getcpuinfo;
        struct xen_sysctl_availheap         availheap;
        struct xen_sysctl_get_pmstat        get_pmstat;
        struct xen_sysctl_cpu_hotplug       cpu_hotplug;
        struct xen_sysctl_pm_op             pm_op;
        struct xen_sysctl_page_offline_op   page_offline;
        struct xen_sysctl_lockprof_op       lockprof_op;
        struct xen_sysctl_cpupool_op        cpupool_op;
        struct xen_sysctl_scheduler_op      scheduler_op;
        struct xen_sysctl_coverage_op       coverage_op;
        struct xen_sysctl_psr_cmt_op        psr_cmt_op;
        struct xen_sysctl_psr_alloc         psr_alloc;
        struct xen_sysctl_cpu_levelling_caps cpu_levelling_caps;
        struct xen_sysctl_cpu_featureset    cpu_featureset;
        struct xen_sysctl_livepatch_op      livepatch;
#if defined(__i386__) || defined(__x86_64__)
        struct xen_sysctl_cpu_policy        cpu_policy;
#endif
        uint8_t                             pad[128];
    } u;
};
typedef struct xen_sysctl xen_sysctl_t;
DEFINE_XEN_GUEST_HANDLE(xen_sysctl_t);

#endif /* __XEN_PUBLIC_SYSCTL_H__ */

/*
 * Local variables:
 * mode: C
 * c-file-style: "BSD"
 * c-basic-offset: 4
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
