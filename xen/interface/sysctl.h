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

#define XEN_SYSCTL_INTERFACE_VERSION 0x00000009

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
typedef struct xen_sysctl_readconsole xen_sysctl_readconsole_t;
DEFINE_XEN_GUEST_HANDLE(xen_sysctl_readconsole_t);

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
    struct xenctl_cpumap cpu_mask;
    uint32_t             evt_mask;
    /* OUT variables */
    uint64_aligned_t buffer_mfn;
    uint32_t size;  /* Also an IN variable! */
};
typedef struct xen_sysctl_tbuf_op xen_sysctl_tbuf_op_t;
DEFINE_XEN_GUEST_HANDLE(xen_sysctl_tbuf_op_t);

/*
 * Get physical information about the host machine
 */
/* XEN_SYSCTL_physinfo */
 /* (x86) The platform supports HVM guests. */
#define _XEN_SYSCTL_PHYSCAP_hvm          0
#define XEN_SYSCTL_PHYSCAP_hvm           (1u<<_XEN_SYSCTL_PHYSCAP_hvm)
 /* (x86) The platform supports HVM-guest direct access to I/O devices. */
#define _XEN_SYSCTL_PHYSCAP_hvm_directio 1
#define XEN_SYSCTL_PHYSCAP_hvm_directio  (1u<<_XEN_SYSCTL_PHYSCAP_hvm_directio)
struct xen_sysctl_physinfo {
    uint32_t threads_per_core;
    uint32_t cores_per_socket;
    uint32_t nr_cpus;     /* # CPUs currently online */
    uint32_t max_cpu_id;  /* Largest possible CPU ID on this host */
    uint32_t nr_nodes;    /* # nodes currently online */
    uint32_t max_node_id; /* Largest possible node ID on this host */
    uint32_t cpu_khz;
    uint64_aligned_t total_pages;
    uint64_aligned_t free_pages;
    uint64_aligned_t scrub_pages;
    uint32_t hw_cap[8];

    /* XEN_SYSCTL_PHYSCAP_??? */
    uint32_t capabilities;
};
typedef struct xen_sysctl_physinfo xen_sysctl_physinfo_t;
DEFINE_XEN_GUEST_HANDLE(xen_sysctl_physinfo_t);

/*
 * Get the ID of the current scheduler.
 */
/* XEN_SYSCTL_sched_id */
struct xen_sysctl_sched_id {
    /* OUT variable */
    uint32_t sched_id;
};
typedef struct xen_sysctl_sched_id xen_sysctl_sched_id_t;
DEFINE_XEN_GUEST_HANDLE(xen_sysctl_sched_id_t);

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
typedef struct xen_sysctl_perfc_op xen_sysctl_perfc_op_t;
DEFINE_XEN_GUEST_HANDLE(xen_sysctl_perfc_op_t);

/* XEN_SYSCTL_getdomaininfolist */
struct xen_sysctl_getdomaininfolist {
    /* IN variables. */
    domid_t               first_domain;
    uint32_t              max_domains;
    XEN_GUEST_HANDLE_64(xen_domctl_getdomaininfo_t) buffer;
    /* OUT variables. */
    uint32_t              num_domains;
};
typedef struct xen_sysctl_getdomaininfolist xen_sysctl_getdomaininfolist_t;
DEFINE_XEN_GUEST_HANDLE(xen_sysctl_getdomaininfolist_t);

/* Inject debug keys into Xen. */
/* XEN_SYSCTL_debug_keys */
struct xen_sysctl_debug_keys {
    /* IN variables. */
    XEN_GUEST_HANDLE_64(char) keys;
    uint32_t nr_keys;
};
typedef struct xen_sysctl_debug_keys xen_sysctl_debug_keys_t;
DEFINE_XEN_GUEST_HANDLE(xen_sysctl_debug_keys_t);

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
typedef struct xen_sysctl_getcpuinfo xen_sysctl_getcpuinfo_t;
DEFINE_XEN_GUEST_HANDLE(xen_sysctl_getcpuinfo_t); 

/* XEN_SYSCTL_availheap */
struct xen_sysctl_availheap {
    /* IN variables. */
    uint32_t min_bitwidth;  /* Smallest address width (zero if don't care). */
    uint32_t max_bitwidth;  /* Largest address width (zero if don't care). */
    int32_t  node;          /* NUMA node of interest (-1 for all nodes). */
    /* OUT variables. */
    uint64_aligned_t avail_bytes;/* Bytes available in the specified region. */
};
typedef struct xen_sysctl_availheap xen_sysctl_availheap_t;
DEFINE_XEN_GUEST_HANDLE(xen_sysctl_availheap_t);

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
typedef struct pm_px_stat pm_px_stat_t;
DEFINE_XEN_GUEST_HANDLE(pm_px_stat_t);

struct pm_cx_stat {
    uint32_t nr;    /* entry nr in triggers & residencies, including C0 */
    uint32_t last;  /* last Cx state */
    uint64_aligned_t idle_time;                 /* idle time from boot */
    XEN_GUEST_HANDLE_64(uint64) triggers;    /* Cx trigger counts */
    XEN_GUEST_HANDLE_64(uint64) residencies; /* Cx residencies */
    uint64_aligned_t pc2;
    uint64_aligned_t pc3;
    uint64_aligned_t pc6;
    uint64_aligned_t pc7;
    uint64_aligned_t cc3;
    uint64_aligned_t cc6;
    uint64_aligned_t cc7;
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
typedef struct xen_sysctl_get_pmstat xen_sysctl_get_pmstat_t;
DEFINE_XEN_GUEST_HANDLE(xen_sysctl_get_pmstat_t);

/* XEN_SYSCTL_cpu_hotplug */
struct xen_sysctl_cpu_hotplug {
    /* IN variables */
    uint32_t cpu;   /* Physical cpu. */
#define XEN_SYSCTL_CPU_HOTPLUG_ONLINE  0
#define XEN_SYSCTL_CPU_HOTPLUG_OFFLINE 1
    uint32_t op;    /* hotplug opcode */
};
typedef struct xen_sysctl_cpu_hotplug xen_sysctl_cpu_hotplug_t;
DEFINE_XEN_GUEST_HANDLE(xen_sysctl_cpu_hotplug_t);

/*
 * Get/set xen power management, include 
 * 1. cpufreq governors and related parameters
 */
/* XEN_SYSCTL_pm_op */
struct xen_userspace {
    uint32_t scaling_setspeed;
};
typedef struct xen_userspace xen_userspace_t;

struct xen_ondemand {
    uint32_t sampling_rate_max;
    uint32_t sampling_rate_min;

    uint32_t sampling_rate;
    uint32_t up_threshold;
};
typedef struct xen_ondemand xen_ondemand_t;

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

    /* cpuidle max_cstate access command */
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
        uint32_t                    get_max_cstate;
        uint32_t                    set_max_cstate;
        uint32_t                    get_vcpu_migration_delay;
        uint32_t                    set_vcpu_migration_delay;
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
typedef struct xen_sysctl_lockprof_op xen_sysctl_lockprof_op_t;
DEFINE_XEN_GUEST_HANDLE(xen_sysctl_lockprof_op_t);

/* XEN_SYSCTL_topologyinfo */
#define INVALID_TOPOLOGY_ID  (~0U)
struct xen_sysctl_topologyinfo {
    /*
     * IN: maximum addressable entry in the caller-provided arrays.
     * OUT: largest cpu identifier in the system.
     * If OUT is greater than IN then the arrays are truncated!
     * If OUT is leass than IN then the array tails are not written by sysctl.
     */
    uint32_t max_cpu_index;

    /*
     * If not NULL, these arrays are filled with core/socket/node identifier
     * for each cpu.
     * If a cpu has no core/socket/node information (e.g., cpu not present) 
     * then the sentinel value ~0u is written to each array.
     * The number of array elements written by the sysctl is:
     *   min(@max_cpu_index_IN,@max_cpu_index_OUT)+1
     */
    XEN_GUEST_HANDLE_64(uint32) cpu_to_core;
    XEN_GUEST_HANDLE_64(uint32) cpu_to_socket;
    XEN_GUEST_HANDLE_64(uint32) cpu_to_node;
};
typedef struct xen_sysctl_topologyinfo xen_sysctl_topologyinfo_t;
DEFINE_XEN_GUEST_HANDLE(xen_sysctl_topologyinfo_t);

/* XEN_SYSCTL_numainfo */
#define INVALID_NUMAINFO_ID (~0U)
struct xen_sysctl_numainfo {
    /*
     * IN: maximum addressable entry in the caller-provided arrays.
     * OUT: largest node identifier in the system.
     * If OUT is greater than IN then the arrays are truncated!
     */
    uint32_t max_node_index;

    /* NB. Entries are 0 if node is not present. */
    XEN_GUEST_HANDLE_64(uint64) node_to_memsize;
    XEN_GUEST_HANDLE_64(uint64) node_to_memfree;

    /*
     * Array, of size (max_node_index+1)^2, listing memory access distances
     * between nodes. If an entry has no node distance information (e.g., node 
     * not present) then the value ~0u is written.
     * 
     * Note that the array rows must be indexed by multiplying by the minimum 
     * of the caller-provided max_node_index and the returned value of
     * max_node_index. That is, if the largest node index in the system is
     * smaller than the caller can handle, a smaller 2-d array is constructed
     * within the space provided by the caller. When this occurs, trailing
     * space provided by the caller is not modified. If the largest node index
     * in the system is larger than the caller can handle, then a 2-d array of
     * the maximum size handleable by the caller is constructed.
     */
    XEN_GUEST_HANDLE_64(uint32) node_to_node_distance;
};
typedef struct xen_sysctl_numainfo xen_sysctl_numainfo_t;
DEFINE_XEN_GUEST_HANDLE(xen_sysctl_numainfo_t);

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
    struct xenctl_cpumap cpumap; /*     OUT: IF */
};
typedef struct xen_sysctl_cpupool_op xen_sysctl_cpupool_op_t;
DEFINE_XEN_GUEST_HANDLE(xen_sysctl_cpupool_op_t);

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

struct xen_sysctl_credit_schedule {
    /* Length of timeslice in milliseconds */
#define XEN_SYSCTL_CSCHED_TSLICE_MAX 1000
#define XEN_SYSCTL_CSCHED_TSLICE_MIN 1
    unsigned tslice_ms;
    /* Rate limit (minimum timeslice) in microseconds */
#define XEN_SYSCTL_SCHED_RATELIMIT_MAX 500000
#define XEN_SYSCTL_SCHED_RATELIMIT_MIN 100
    unsigned ratelimit_us;
};
typedef struct xen_sysctl_credit_schedule xen_sysctl_credit_schedule_t;
DEFINE_XEN_GUEST_HANDLE(xen_sysctl_credit_schedule_t);

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
    } u;
};
typedef struct xen_sysctl_scheduler_op xen_sysctl_scheduler_op_t;
DEFINE_XEN_GUEST_HANDLE(xen_sysctl_scheduler_op_t);

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
#define XEN_SYSCTL_topologyinfo                  16 
#define XEN_SYSCTL_numainfo                      17
#define XEN_SYSCTL_cpupool_op                    18
#define XEN_SYSCTL_scheduler_op                  19
    uint32_t interface_version; /* XEN_SYSCTL_INTERFACE_VERSION */
    union {
        struct xen_sysctl_readconsole       readconsole;
        struct xen_sysctl_tbuf_op           tbuf_op;
        struct xen_sysctl_physinfo          physinfo;
        struct xen_sysctl_topologyinfo      topologyinfo;
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
        uint8_t                             pad[128];
    } u;
};
typedef struct xen_sysctl xen_sysctl_t;
DEFINE_XEN_GUEST_HANDLE(xen_sysctl_t);

#endif /* __XEN_PUBLIC_SYSCTL_H__ */

/*
 * Local variables:
 * mode: C
 * c-set-style: "BSD"
 * c-basic-offset: 4
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
