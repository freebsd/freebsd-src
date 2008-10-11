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

#define XEN_SYSCTL_INTERFACE_VERSION 0x00000006

/*
 * Read console content from Xen buffer ring.
 */
#define XEN_SYSCTL_readconsole       1
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
#define XEN_SYSCTL_tbuf_op           2
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
    uint32_t size;
};
typedef struct xen_sysctl_tbuf_op xen_sysctl_tbuf_op_t;
DEFINE_XEN_GUEST_HANDLE(xen_sysctl_tbuf_op_t);

/*
 * Get physical information about the host machine
 */
#define XEN_SYSCTL_physinfo          3
 /* (x86) The platform supports HVM guests. */
#define _XEN_SYSCTL_PHYSCAP_hvm          0
#define XEN_SYSCTL_PHYSCAP_hvm           (1u<<_XEN_SYSCTL_PHYSCAP_hvm)
 /* (x86) The platform supports HVM-guest direct access to I/O devices. */
#define _XEN_SYSCTL_PHYSCAP_hvm_directio 1
#define XEN_SYSCTL_PHYSCAP_hvm_directio  (1u<<_XEN_SYSCTL_PHYSCAP_hvm_directio)
struct xen_sysctl_physinfo {
    uint32_t threads_per_core;
    uint32_t cores_per_socket;
    uint32_t nr_cpus;
    uint32_t nr_nodes;
    uint32_t cpu_khz;
    uint64_aligned_t total_pages;
    uint64_aligned_t free_pages;
    uint64_aligned_t scrub_pages;
    uint32_t hw_cap[8];

    /*
     * IN: maximum addressable entry in the caller-provided cpu_to_node array.
     * OUT: largest cpu identifier in the system.
     * If OUT is greater than IN then the cpu_to_node array is truncated!
     */
    uint32_t max_cpu_id;
    /*
     * If not NULL, this array is filled with node identifier for each cpu.
     * If a cpu has no node information (e.g., cpu not present) then the
     * sentinel value ~0u is written.
     * The size of this array is specified by the caller in @max_cpu_id.
     * If the actual @max_cpu_id is smaller than the array then the trailing
     * elements of the array will not be written by the sysctl.
     */
    XEN_GUEST_HANDLE_64(uint32) cpu_to_node;

    /* XEN_SYSCTL_PHYSCAP_??? */
    uint32_t capabilities;
};
typedef struct xen_sysctl_physinfo xen_sysctl_physinfo_t;
DEFINE_XEN_GUEST_HANDLE(xen_sysctl_physinfo_t);

/*
 * Get the ID of the current scheduler.
 */
#define XEN_SYSCTL_sched_id          4
struct xen_sysctl_sched_id {
    /* OUT variable */
    uint32_t sched_id;
};
typedef struct xen_sysctl_sched_id xen_sysctl_sched_id_t;
DEFINE_XEN_GUEST_HANDLE(xen_sysctl_sched_id_t);

/* Interface for controlling Xen software performance counters. */
#define XEN_SYSCTL_perfc_op          5
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

#define XEN_SYSCTL_getdomaininfolist 6
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
#define XEN_SYSCTL_debug_keys        7
struct xen_sysctl_debug_keys {
    /* IN variables. */
    XEN_GUEST_HANDLE_64(char) keys;
    uint32_t nr_keys;
};
typedef struct xen_sysctl_debug_keys xen_sysctl_debug_keys_t;
DEFINE_XEN_GUEST_HANDLE(xen_sysctl_debug_keys_t);

/* Get physical CPU information. */
#define XEN_SYSCTL_getcpuinfo        8
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

#define XEN_SYSCTL_availheap         9
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

#define XEN_SYSCTL_get_pmstat        10
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

#define XEN_SYSCTL_cpu_hotplug       11
struct xen_sysctl_cpu_hotplug {
    /* IN variables */
    uint32_t cpu;   /* Physical cpu. */
#define XEN_SYSCTL_CPU_HOTPLUG_ONLINE  0
#define XEN_SYSCTL_CPU_HOTPLUG_OFFLINE 1
    uint32_t op;    /* hotplug opcode */
};
typedef struct xen_sysctl_cpu_hotplug xen_sysctl_cpu_hotplug_t;
DEFINE_XEN_GUEST_HANDLE(xen_sysctl_cpu_hotplug_t);


struct xen_sysctl {
    uint32_t cmd;
    uint32_t interface_version; /* XEN_SYSCTL_INTERFACE_VERSION */
    union {
        struct xen_sysctl_readconsole       readconsole;
        struct xen_sysctl_tbuf_op           tbuf_op;
        struct xen_sysctl_physinfo          physinfo;
        struct xen_sysctl_sched_id          sched_id;
        struct xen_sysctl_perfc_op          perfc_op;
        struct xen_sysctl_getdomaininfolist getdomaininfolist;
        struct xen_sysctl_debug_keys        debug_keys;
        struct xen_sysctl_getcpuinfo        getcpuinfo;
        struct xen_sysctl_availheap         availheap;
        struct xen_sysctl_get_pmstat        get_pmstat;
        struct xen_sysctl_cpu_hotplug       cpu_hotplug;
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
