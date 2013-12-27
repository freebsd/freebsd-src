/******************************************************************************
 * memory.h
 * 
 * Memory reservation and information.
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
 * Copyright (c) 2005, Keir Fraser <keir@xensource.com>
 */

#ifndef __XEN_PUBLIC_MEMORY_H__
#define __XEN_PUBLIC_MEMORY_H__

#include "xen.h"

/*
 * Increase or decrease the specified domain's memory reservation. Returns the
 * number of extents successfully allocated or freed.
 * arg == addr of struct xen_memory_reservation.
 */
#define XENMEM_increase_reservation 0
#define XENMEM_decrease_reservation 1
#define XENMEM_populate_physmap     6

#if __XEN_INTERFACE_VERSION__ >= 0x00030209
/*
 * Maximum # bits addressable by the user of the allocated region (e.g., I/O 
 * devices often have a 32-bit limitation even in 64-bit systems). If zero 
 * then the user has no addressing restriction. This field is not used by 
 * XENMEM_decrease_reservation.
 */
#define XENMEMF_address_bits(x)     (x)
#define XENMEMF_get_address_bits(x) ((x) & 0xffu)
/* NUMA node to allocate from. */
#define XENMEMF_node(x)     (((x) + 1) << 8)
#define XENMEMF_get_node(x) ((((x) >> 8) - 1) & 0xffu)
/* Flag to populate physmap with populate-on-demand entries */
#define XENMEMF_populate_on_demand (1<<16)
/* Flag to request allocation only from the node specified */
#define XENMEMF_exact_node_request  (1<<17)
#define XENMEMF_exact_node(n) (XENMEMF_node(n) | XENMEMF_exact_node_request)
#endif

struct xen_memory_reservation {

    /*
     * XENMEM_increase_reservation:
     *   OUT: MFN (*not* GMFN) bases of extents that were allocated
     * XENMEM_decrease_reservation:
     *   IN:  GMFN bases of extents to free
     * XENMEM_populate_physmap:
     *   IN:  GPFN bases of extents to populate with memory
     *   OUT: GMFN bases of extents that were allocated
     *   (NB. This command also updates the mach_to_phys translation table)
     */
    XEN_GUEST_HANDLE(xen_pfn_t) extent_start;

    /* Number of extents, and size/alignment of each (2^extent_order pages). */
    xen_ulong_t    nr_extents;
    unsigned int   extent_order;

#if __XEN_INTERFACE_VERSION__ >= 0x00030209
    /* XENMEMF flags. */
    unsigned int   mem_flags;
#else
    unsigned int   address_bits;
#endif

    /*
     * Domain whose reservation is being changed.
     * Unprivileged domains can specify only DOMID_SELF.
     */
    domid_t        domid;
};
typedef struct xen_memory_reservation xen_memory_reservation_t;
DEFINE_XEN_GUEST_HANDLE(xen_memory_reservation_t);

/*
 * An atomic exchange of memory pages. If return code is zero then
 * @out.extent_list provides GMFNs of the newly-allocated memory.
 * Returns zero on complete success, otherwise a negative error code.
 * On complete success then always @nr_exchanged == @in.nr_extents.
 * On partial success @nr_exchanged indicates how much work was done.
 */
#define XENMEM_exchange             11
struct xen_memory_exchange {
    /*
     * [IN] Details of memory extents to be exchanged (GMFN bases).
     * Note that @in.address_bits is ignored and unused.
     */
    struct xen_memory_reservation in;

    /*
     * [IN/OUT] Details of new memory extents.
     * We require that:
     *  1. @in.domid == @out.domid
     *  2. @in.nr_extents  << @in.extent_order == 
     *     @out.nr_extents << @out.extent_order
     *  3. @in.extent_start and @out.extent_start lists must not overlap
     *  4. @out.extent_start lists GPFN bases to be populated
     *  5. @out.extent_start is overwritten with allocated GMFN bases
     */
    struct xen_memory_reservation out;

    /*
     * [OUT] Number of input extents that were successfully exchanged:
     *  1. The first @nr_exchanged input extents were successfully
     *     deallocated.
     *  2. The corresponding first entries in the output extent list correctly
     *     indicate the GMFNs that were successfully exchanged.
     *  3. All other input and output extents are untouched.
     *  4. If not all input exents are exchanged then the return code of this
     *     command will be non-zero.
     *  5. THIS FIELD MUST BE INITIALISED TO ZERO BY THE CALLER!
     */
    xen_ulong_t nr_exchanged;
};
typedef struct xen_memory_exchange xen_memory_exchange_t;
DEFINE_XEN_GUEST_HANDLE(xen_memory_exchange_t);

/*
 * Returns the maximum machine frame number of mapped RAM in this system.
 * This command always succeeds (it never returns an error code).
 * arg == NULL.
 */
#define XENMEM_maximum_ram_page     2

/*
 * Returns the current or maximum memory reservation, in pages, of the
 * specified domain (may be DOMID_SELF). Returns -ve errcode on failure.
 * arg == addr of domid_t.
 */
#define XENMEM_current_reservation  3
#define XENMEM_maximum_reservation  4

/*
 * Returns the maximum GPFN in use by the guest, or -ve errcode on failure.
 */
#define XENMEM_maximum_gpfn         14

/*
 * Returns a list of MFN bases of 2MB extents comprising the machine_to_phys
 * mapping table. Architectures which do not have a m2p table do not implement
 * this command.
 * arg == addr of xen_machphys_mfn_list_t.
 */
#define XENMEM_machphys_mfn_list    5
struct xen_machphys_mfn_list {
    /*
     * Size of the 'extent_start' array. Fewer entries will be filled if the
     * machphys table is smaller than max_extents * 2MB.
     */
    unsigned int max_extents;

    /*
     * Pointer to buffer to fill with list of extent starts. If there are
     * any large discontiguities in the machine address space, 2MB gaps in
     * the machphys table will be represented by an MFN base of zero.
     */
    XEN_GUEST_HANDLE(xen_pfn_t) extent_start;

    /*
     * Number of extents written to the above array. This will be smaller
     * than 'max_extents' if the machphys table is smaller than max_e * 2MB.
     */
    unsigned int nr_extents;
};
typedef struct xen_machphys_mfn_list xen_machphys_mfn_list_t;
DEFINE_XEN_GUEST_HANDLE(xen_machphys_mfn_list_t);

/*
 * Returns the location in virtual address space of the machine_to_phys
 * mapping table. Architectures which do not have a m2p table, or which do not
 * map it by default into guest address space, do not implement this command.
 * arg == addr of xen_machphys_mapping_t.
 */
#define XENMEM_machphys_mapping     12
struct xen_machphys_mapping {
    xen_ulong_t v_start, v_end; /* Start and end virtual addresses.   */
    xen_ulong_t max_mfn;        /* Maximum MFN that can be looked up. */
};
typedef struct xen_machphys_mapping xen_machphys_mapping_t;
DEFINE_XEN_GUEST_HANDLE(xen_machphys_mapping_t);

/*
 * Sets the GPFN at which a particular page appears in the specified guest's
 * pseudophysical address space.
 * arg == addr of xen_add_to_physmap_t.
 */
#define XENMEM_add_to_physmap      7
struct xen_add_to_physmap {
    /* Which domain to change the mapping for. */
    domid_t domid;

    /* Number of pages to go through for gmfn_range */
    uint16_t    size;

    /* Source mapping space. */
#define XENMAPSPACE_shared_info 0 /* shared info page */
#define XENMAPSPACE_grant_table 1 /* grant table page */
#define XENMAPSPACE_gmfn        2 /* GMFN */
#define XENMAPSPACE_gmfn_range  3 /* GMFN range */
    unsigned int space;

#define XENMAPIDX_grant_table_status 0x80000000

    /* Index into source mapping space. */
    xen_ulong_t idx;

    /* GPFN where the source mapping page should appear. */
    xen_pfn_t     gpfn;
};
typedef struct xen_add_to_physmap xen_add_to_physmap_t;
DEFINE_XEN_GUEST_HANDLE(xen_add_to_physmap_t);

/*
 * Unmaps the page appearing at a particular GPFN from the specified guest's
 * pseudophysical address space.
 * arg == addr of xen_remove_from_physmap_t.
 */
#define XENMEM_remove_from_physmap      15
struct xen_remove_from_physmap {
    /* Which domain to change the mapping for. */
    domid_t domid;

    /* GPFN of the current mapping of the page. */
    xen_pfn_t     gpfn;
};
typedef struct xen_remove_from_physmap xen_remove_from_physmap_t;
DEFINE_XEN_GUEST_HANDLE(xen_remove_from_physmap_t);

/*** REMOVED ***/
/*#define XENMEM_translate_gpfn_list  8*/

/*
 * Returns the pseudo-physical memory map as it was when the domain
 * was started (specified by XENMEM_set_memory_map).
 * arg == addr of xen_memory_map_t.
 */
#define XENMEM_memory_map           9
struct xen_memory_map {
    /*
     * On call the number of entries which can be stored in buffer. On
     * return the number of entries which have been stored in
     * buffer.
     */
    unsigned int nr_entries;

    /*
     * Entries in the buffer are in the same format as returned by the
     * BIOS INT 0x15 EAX=0xE820 call.
     */
    XEN_GUEST_HANDLE(void) buffer;
};
typedef struct xen_memory_map xen_memory_map_t;
DEFINE_XEN_GUEST_HANDLE(xen_memory_map_t);

/*
 * Returns the real physical memory map. Passes the same structure as
 * XENMEM_memory_map.
 * arg == addr of xen_memory_map_t.
 */
#define XENMEM_machine_memory_map   10

/*
 * Set the pseudo-physical memory map of a domain, as returned by
 * XENMEM_memory_map.
 * arg == addr of xen_foreign_memory_map_t.
 */
#define XENMEM_set_memory_map       13
struct xen_foreign_memory_map {
    domid_t domid;
    struct xen_memory_map map;
};
typedef struct xen_foreign_memory_map xen_foreign_memory_map_t;
DEFINE_XEN_GUEST_HANDLE(xen_foreign_memory_map_t);

#define XENMEM_set_pod_target       16
#define XENMEM_get_pod_target       17
struct xen_pod_target {
    /* IN */
    uint64_t target_pages;
    /* OUT */
    uint64_t tot_pages;
    uint64_t pod_cache_pages;
    uint64_t pod_entries;
    /* IN */
    domid_t domid;
};
typedef struct xen_pod_target xen_pod_target_t;

#if defined(__XEN__) || defined(__XEN_TOOLS__)

#ifndef uint64_aligned_t
#define uint64_aligned_t uint64_t
#endif

/*
 * Get the number of MFNs saved through memory sharing.
 * The call never fails. 
 */
#define XENMEM_get_sharing_freed_pages    18
#define XENMEM_get_sharing_shared_pages   19

#define XENMEM_paging_op                    20
#define XENMEM_paging_op_nominate           0
#define XENMEM_paging_op_evict              1
#define XENMEM_paging_op_prep               2

#define XENMEM_access_op                    21
#define XENMEM_access_op_resume             0

struct xen_mem_event_op {
    uint8_t     op;         /* XENMEM_*_op_* */
    domid_t     domain;
    

    /* PAGING_PREP IN: buffer to immediately fill page in */
    uint64_aligned_t    buffer;
    /* Other OPs */
    uint64_aligned_t    gfn;           /* IN:  gfn of page being operated on */
};
typedef struct xen_mem_event_op xen_mem_event_op_t;
DEFINE_XEN_GUEST_HANDLE(xen_mem_event_op_t);

#define XENMEM_sharing_op                   22
#define XENMEM_sharing_op_nominate_gfn      0
#define XENMEM_sharing_op_nominate_gref     1
#define XENMEM_sharing_op_share             2
#define XENMEM_sharing_op_resume            3
#define XENMEM_sharing_op_debug_gfn         4
#define XENMEM_sharing_op_debug_mfn         5
#define XENMEM_sharing_op_debug_gref        6
#define XENMEM_sharing_op_add_physmap       7
#define XENMEM_sharing_op_audit             8

#define XENMEM_SHARING_OP_S_HANDLE_INVALID  (-10)
#define XENMEM_SHARING_OP_C_HANDLE_INVALID  (-9)

/* The following allows sharing of grant refs. This is useful
 * for sharing utilities sitting as "filters" in IO backends
 * (e.g. memshr + blktap(2)). The IO backend is only exposed 
 * to grant references, and this allows sharing of the grefs */
#define XENMEM_SHARING_OP_FIELD_IS_GREF_FLAG   (1ULL << 62)

#define XENMEM_SHARING_OP_FIELD_MAKE_GREF(field, val)  \
    (field) = (XENMEM_SHARING_OP_FIELD_IS_GREF_FLAG | val)
#define XENMEM_SHARING_OP_FIELD_IS_GREF(field)         \
    ((field) & XENMEM_SHARING_OP_FIELD_IS_GREF_FLAG)
#define XENMEM_SHARING_OP_FIELD_GET_GREF(field)        \
    ((field) & (~XENMEM_SHARING_OP_FIELD_IS_GREF_FLAG))

struct xen_mem_sharing_op {
    uint8_t     op;     /* XENMEM_sharing_op_* */
    domid_t     domain;

    union {
        struct mem_sharing_op_nominate {  /* OP_NOMINATE_xxx           */
            union {
                uint64_aligned_t gfn;     /* IN: gfn to nominate       */
                uint32_t      grant_ref;  /* IN: grant ref to nominate */
            } u;
            uint64_aligned_t  handle;     /* OUT: the handle           */
        } nominate;
        struct mem_sharing_op_share {     /* OP_SHARE/ADD_PHYSMAP */
            uint64_aligned_t source_gfn;    /* IN: the gfn of the source page */
            uint64_aligned_t source_handle; /* IN: handle to the source page */
            uint64_aligned_t client_gfn;    /* IN: the client gfn */
            uint64_aligned_t client_handle; /* IN: handle to the client page */
            domid_t  client_domain; /* IN: the client domain id */
        } share; 
        struct mem_sharing_op_debug {     /* OP_DEBUG_xxx */
            union {
                uint64_aligned_t gfn;      /* IN: gfn to debug          */
                uint64_aligned_t mfn;      /* IN: mfn to debug          */
                uint32_t gref;     /* IN: gref to debug         */
            } u;
        } debug;
    } u;
};
typedef struct xen_mem_sharing_op xen_mem_sharing_op_t;
DEFINE_XEN_GUEST_HANDLE(xen_mem_sharing_op_t);

#endif /* defined(__XEN__) || defined(__XEN_TOOLS__) */

#endif /* __XEN_PUBLIC_MEMORY_H__ */

/*
 * Local variables:
 * mode: C
 * c-set-style: "BSD"
 * c-basic-offset: 4
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
