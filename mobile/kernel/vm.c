/*
 * Virtual Memory Implementation
 * uOS(m) - User OS Mobile
 * SV39 page table management
 */

#include "vm.h"
#include "memory.h"

#define MAX_PAGES 65536
static page_allocator_t page_allocator;

extern void uart_puts(const char *s);
extern void uart_putc(char c);

/* Initialize virtual memory */
int vm_init(void) {
    uart_puts("Virtual memory initializing (SV39)...\n");
    
    page_allocator.num_pages = MAX_PAGES;
    page_allocator.bitmap = (uint8_t *)mem_alloc((MAX_PAGES + 7) / 8);
    page_allocator.next_free = 0;
    
    if (!page_allocator.bitmap) {
        uart_puts("Failed to allocate page bitmap\n");
        return -1;
    }
    
    uart_puts("Virtual memory ready\n");
    return 0;
}

/* Allocate a physical page */
static uint64_t vm_alloc_page(void) {
    for (uint64_t i = page_allocator.next_free; i < page_allocator.num_pages; i++) {
        uint8_t byte = page_allocator.bitmap[i / 8];
        int bit = i % 8;
        if ((byte & (1 << bit)) == 0) {
            page_allocator.bitmap[i / 8] |= (1 << bit);
            page_allocator.next_free = i + 1;
            return i * PAGE_SIZE;
        }
    }
    return 0;
}

/* Free a physical page */
static void vm_free_page(uint64_t paddr) {
    uint64_t page_num = paddr / PAGE_SIZE;
    if (page_num < page_allocator.num_pages) {
        page_allocator.bitmap[page_num / 8] &= ~(1 << (page_num % 8));
    }
}

/* Get page table entry from SV39 page table */
pte_t *vm_get_pte(pte_t *root, uint64_t vaddr) {
    pte_t *table = root;
    
    for (int level = 2; level >= 0; level--) {
        uint64_t vpn;
        if (level == 2) vpn = VA_VPN2(vaddr);
        else if (level == 1) vpn = VA_VPN1(vaddr);
        else vpn = VA_VPN0(vaddr);
        
        pte_t pte = table[vpn];
        
        /* Not valid */
        if (!(pte & PTE_V)) {
            return NULL;
        }
        
        /* Leaf page */
        if ((pte & (PTE_R | PTE_W | PTE_X)) != 0) {
            return &table[vpn];
        }
        
        /* Non-leaf, get next level table */
        uint64_t next_pa = (pte >> 10) << PAGE_SHIFT;
        table = (pte_t *)next_pa;
    }
    
    return NULL;
}

/* Set a page table entry */
int vm_set_pte(pte_t *root, uint64_t vaddr, uint64_t paddr, int flags) {
    pte_t *table = root;
    
    for (int level = 2; level >= 0; level--) {
        uint64_t vpn;
        if (level == 2) vpn = VA_VPN2(vaddr);
        else if (level == 1) vpn = VA_VPN1(vaddr);
        else vpn = VA_VPN0(vaddr);
        
        if (level > 0) {
            /* Intermediate level */
            pte_t *pte = &table[vpn];
            
            if (!(*pte & PTE_V)) {
                /* Allocate new page table */
                uint64_t new_table_pa = vm_alloc_page();
                if (!new_table_pa) return -1;
                
                /* Create PTE pointing to new table */
                *pte = ((new_table_pa >> PAGE_SHIFT) << 10) | PTE_V;
            }
            
            /* Get next level table */
            uint64_t next_pa = ((*pte) >> 10) << PAGE_SHIFT;
            table = (pte_t *)next_pa;
        } else {
            /* Leaf level */
            uint64_t ppn = PA_PPN(paddr);
            pte_t pte_val = (ppn << 10) | flags | PTE_V;
            table[vpn] = pte_val;
        }
    }
    
    return 0;
}

/* Create a new VM context */
vm_context_t *vm_create_context(uint32_t pid) {
    vm_context_t *ctx = (vm_context_t *)mem_alloc(sizeof(vm_context_t));
    if (!ctx) return NULL;
    
    /* Allocate root page table */
    ctx->root_page_table = (pte_t *)mem_alloc(PAGE_SIZE);
    if (!ctx->root_page_table) {
        mem_free(ctx);
        return NULL;
    }
    
    ctx->owner_pid = pid;
    
    /* Set SATP (SV39, PPN of root table, ASID) */
    uint64_t root_ppn = (uint64_t)ctx->root_page_table >> PAGE_SHIFT;
    ctx->satp = (8UL << 60) | (0 << 44) | root_ppn;
    
    return ctx;
}

/* Destroy a VM context */
void vm_destroy_context(vm_context_t *ctx) {
    if (!ctx) return;
    
    if (ctx->root_page_table) {
        mem_free(ctx->root_page_table);
    }
    mem_free(ctx);
}

/* Map a page in VM context */
int vm_map_page(vm_context_t *ctx, uint64_t vaddr, uint64_t paddr, int flags) {
    if (!ctx || !ctx->root_page_table) return -1;
    
    return vm_set_pte(ctx->root_page_table, vaddr, paddr, flags);
}

/* Unmap a page */
int vm_unmap_page(vm_context_t *ctx, uint64_t vaddr) {
    if (!ctx) return -1;
    
    pte_t *pte = vm_get_pte(ctx->root_page_table, vaddr);
    if (pte) {
        *pte = 0;
        return 0;
    }
    return -1;
}

/* Translate virtual address to physical */
uint64_t vm_get_physical_addr(vm_context_t *ctx, uint64_t vaddr) {
    pte_t *pte = vm_translate(ctx, vaddr);
    if (!pte) return 0;
    
    uint64_t ppn = ((*pte) >> 10);
    return (ppn << PAGE_SHIFT) | VA_OFFSET(vaddr);
}

/* Translate virtual address */
pte_t *vm_translate(vm_context_t *ctx, uint64_t vaddr) {
    if (!ctx) return NULL;
    
    return vm_get_pte(ctx->root_page_table, vaddr);
}