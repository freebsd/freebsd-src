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
 */

#ifndef __XEN_PUBLIC_HVM_HVM_OP_H__
#define __XEN_PUBLIC_HVM_HVM_OP_H__

/* Get/set subcommands: extra argument == pointer to xen_hvm_param struct. */
#define HVMOP_set_param           0
#define HVMOP_get_param           1
struct xen_hvm_param {
    domid_t  domid;    /* IN */
    uint32_t index;    /* IN */
    uint64_t value;    /* IN/OUT */
};
typedef struct xen_hvm_param xen_hvm_param_t;
DEFINE_XEN_GUEST_HANDLE(xen_hvm_param_t);

/* Set the logical level of one of a domain's PCI INTx wires. */
#define HVMOP_set_pci_intx_level  2
struct xen_hvm_set_pci_intx_level {
    /* Domain to be updated. */
    domid_t  domid;
    /* PCI INTx identification in PCI topology (domain:bus:device:intx). */
    uint8_t  domain, bus, device, intx;
    /* Assertion level (0 = unasserted, 1 = asserted). */
    uint8_t  level;
};
typedef struct xen_hvm_set_pci_intx_level xen_hvm_set_pci_intx_level_t;
DEFINE_XEN_GUEST_HANDLE(xen_hvm_set_pci_intx_level_t);

/* Set the logical level of one of a domain's ISA IRQ wires. */
#define HVMOP_set_isa_irq_level   3
struct xen_hvm_set_isa_irq_level {
    /* Domain to be updated. */
    domid_t  domid;
    /* ISA device identification, by ISA IRQ (0-15). */
    uint8_t  isa_irq;
    /* Assertion level (0 = unasserted, 1 = asserted). */
    uint8_t  level;
};
typedef struct xen_hvm_set_isa_irq_level xen_hvm_set_isa_irq_level_t;
DEFINE_XEN_GUEST_HANDLE(xen_hvm_set_isa_irq_level_t);

#define HVMOP_set_pci_link_route  4
struct xen_hvm_set_pci_link_route {
    /* Domain to be updated. */
    domid_t  domid;
    /* PCI link identifier (0-3). */
    uint8_t  link;
    /* ISA IRQ (1-15), or 0 (disable link). */
    uint8_t  isa_irq;
};
typedef struct xen_hvm_set_pci_link_route xen_hvm_set_pci_link_route_t;
DEFINE_XEN_GUEST_HANDLE(xen_hvm_set_pci_link_route_t);

/* Flushes all VCPU TLBs: @arg must be NULL. */
#define HVMOP_flush_tlbs          5

/* Following tools-only interfaces may change in future. */
#if defined(__XEN__) || defined(__XEN_TOOLS__)

/* Track dirty VRAM. */
#define HVMOP_track_dirty_vram    6
struct xen_hvm_track_dirty_vram {
    /* Domain to be tracked. */
    domid_t  domid;
    /* First pfn to track. */
    uint64_aligned_t first_pfn;
    /* Number of pages to track. */
    uint64_aligned_t nr;
    /* OUT variable. */
    /* Dirty bitmap buffer. */
    XEN_GUEST_HANDLE_64(uint8) dirty_bitmap;
};
typedef struct xen_hvm_track_dirty_vram xen_hvm_track_dirty_vram_t;
DEFINE_XEN_GUEST_HANDLE(xen_hvm_track_dirty_vram_t);

/* Notify that some pages got modified by the Device Model. */
#define HVMOP_modified_memory    7
struct xen_hvm_modified_memory {
    /* Domain to be updated. */
    domid_t  domid;
    /* First pfn. */
    uint64_aligned_t first_pfn;
    /* Number of pages. */
    uint64_aligned_t nr;
};
typedef struct xen_hvm_modified_memory xen_hvm_modified_memory_t;
DEFINE_XEN_GUEST_HANDLE(xen_hvm_modified_memory_t);

#define HVMOP_set_mem_type    8
typedef enum {
    HVMMEM_ram_rw,             /* Normal read/write guest RAM */
    HVMMEM_ram_ro,             /* Read-only; writes are discarded */
    HVMMEM_mmio_dm,            /* Reads and write go to the device model */
} hvmmem_type_t;
/* Notify that a region of memory is to be treated in a specific way. */
struct xen_hvm_set_mem_type {
    /* Domain to be updated. */
    domid_t domid;
    /* Memory type */
    hvmmem_type_t hvmmem_type;
    /* First pfn. */
    uint64_aligned_t first_pfn;
    /* Number of pages. */
    uint64_aligned_t nr;
};
typedef struct xen_hvm_set_mem_type xen_hvm_set_mem_type_t;
DEFINE_XEN_GUEST_HANDLE(xen_hvm_set_mem_type_t);


#endif /* defined(__XEN__) || defined(__XEN_TOOLS__) */

#endif /* __XEN_PUBLIC_HVM_HVM_OP_H__ */
