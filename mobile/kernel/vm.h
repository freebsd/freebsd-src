/*
 * Virtual Memory Management
 * uOS(m) - User OS Mobile
 * RISC-V SV39 page table support
 */

#ifndef _VM_H_
#define _VM_H_

#include <stdint.h>

/* SV39 page table structure */
#define VA_BITS 39
#define PAGE_SHIFT 12
#define PAGE_SIZE (1UL << PAGE_SHIFT)
#define PTRS_PER_PAGE (PAGE_SIZE / 8)

/* Protection bits */
#define PTE_V 0x001  /* Valid */
#define PTE_R 0x002  /* Readable */
#define PTE_W 0x004  /* Writable */
#define PTE_X 0x008  /* Executable */
#define PTE_U 0x010  /* User */
#define PTE_G 0x020  /* Global */
#define PTE_A 0x040  /* Accessed */
#define PTE_D 0x080  /* Dirty */

/* Page table entry */
typedef uint64_t pte_t;

/* Address extraction macros */
#define VPN_BITS 9
#define VPN_MASK ((1UL << VPN_BITS) - 1)
#define VA_VPN2(va) (((va) >> 30) & VPN_MASK)
#define VA_VPN1(va) (((va) >> 21) & VPN_MASK)
#define VA_VPN0(va) (((va) >> 12) & VPN_MASK)
#define VA_OFFSET(va) ((va) & (PAGE_SIZE - 1))
#define PA_PPN(pa) ((pa) >> PAGE_SHIFT)

/* Page allocation */
typedef struct {
    uint64_t num_pages;
    uint8_t *bitmap;
    uint64_t next_free;
} page_allocator_t;

/* Virtual memory context */
typedef struct {
    uint64_t satp;          /* Supervisor Address Translation and Protection */
    pte_t *root_page_table;
    uint32_t owner_pid;
} vm_context_t;

/* VM operations */
int vm_init(void);
vm_context_t *vm_create_context(uint32_t pid);
void vm_destroy_context(vm_context_t *ctx);
int vm_map_page(vm_context_t *ctx, uint64_t vaddr, uint64_t paddr, int flags);
int vm_unmap_page(vm_context_t *ctx, uint64_t vaddr);
pte_t *vm_translate(vm_context_t *ctx, uint64_t vaddr);
uint64_t vm_get_physical_addr(vm_context_t *ctx, uint64_t vaddr);

/* Page table operations */
pte_t *vm_get_pte(pte_t *root, uint64_t vaddr);
int vm_set_pte(pte_t *root, uint64_t vaddr, uint64_t paddr, int flags);

#endif /* _VM_H_ */