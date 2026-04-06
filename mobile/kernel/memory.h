/*
 * Memory Management
 * uOS(m) - User OS Mobile
 * Virtual memory and page management
 */

#ifndef _MEMORY_H_
#define _MEMORY_H_

#include <stdint.h>
#include <stddef.h>

/* Memory protection flags */
#define PROT_NONE   0x0
#define PROT_READ   0x1
#define PROT_WRITE  0x2
#define PROT_EXEC   0x4

/* Memory map flags */
#define MAP_SHARED   0x1
#define MAP_PRIVATE  0x2
#define MAP_FIXED    0x4
#define MAP_ANON     0x8

/* Virtual address range */
typedef struct {
    uint64_t vaddr;         /* Virtual address */
    size_t size;            /* Size in bytes */
    int prot;               /* Protection flags */
    int flags;              /* Mapping flags */
    uint32_t owner_pid;     /* Owner process ID */
} vm_region_t;

/* Heap management */
typedef struct {
    uint64_t base;
    uint64_t current;
    uint64_t limit;
} heap_t;

/* Memory management operations */
int mem_init(void);
void *mem_alloc(size_t size);
void mem_free(void *ptr);
int mem_mmap(uint64_t addr, size_t len, int prot, int flags, uint32_t pid);
int mem_munmap(uint64_t addr, size_t len, uint32_t pid);
int mem_brk(uint64_t addr);

#endif /* _MEMORY_H_ */