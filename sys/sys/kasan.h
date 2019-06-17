#ifndef _SYS_KASAN_H
#define _SYS_KASAN_H

#include <sys/cdefs.h>
#include <sys/types.h>
#include <sys/pool.h>

struct kmem_cache;
struct page;
struct vm_struct;
struct task_struct;

typedef uint32_t u32;


/* 
 * Start of necessary Macros 
 */

#define CONFIG_KASAN_SHADOW_OFFSET 0
#define KASAN_SHADOW_OFFSET 0UL
#define KASAN_SHADOW_SCALE_SHIFT 3
#define __VIRTUAL_MASK_SHIFT 47

/*
 * Compiler uses shadow offset assuming that addresses start
 * from 0. Kernel addresses don't start from 0, so shadow
 * for kernel really starts from compiler's shadow offset +
 * 'kernel address space start' >> KASAN_SHADOW_SCALE_SHIFT
 */
#define KASAN_SHADOW_START      (KASAN_SHADOW_OFFSET + VM_MAX_KERNEL_ADDRESS)
#define KASAN_SHADOW_END        (KASAN_SHADOW_START + \
					(1ULL << (__VIRTUAL_MASK_SHIFT - \
						  KASAN_SHADOW_SCALE_SHIFT)))

#define KASAN_SHADOW_SCALE_SIZE (1UL << KASAN_SHADOW_SCALE_SHIFT)
#define KASAN_SHADOW_MASK       (KASAN_SHADOW_SCALE_SIZE - 1)

#define KASAN_FREE_PAGE         0xFF  /* page was freed */
#define KASAN_PAGE_REDZONE      0xFE  /* redzone for kmalloc_large allocations */
#define KASAN_KMALLOC_REDZONE   0xFC  /* redzone inside slub object */
#define KASAN_KMALLOC_FREE      0xFB  /* object was freed (kmem_cache_free/kfree) */
#define KASAN_GLOBAL_REDZONE    0xFA  /* redzone for global variable */

/*
 * Stack redzone shadow values
 * (Those are compiler's ABI, don't change them)
 */
#define KASAN_STACK_LEFT        0xF1
#define KASAN_STACK_MID         0xF2
#define KASAN_STACK_RIGHT       0xF3
#define KASAN_STACK_PARTIAL     0xF4
#define KASAN_USE_AFTER_SCOPE   0xF8

/*
 * alloca redzone shadow values
 */
#define KASAN_ALLOCA_LEFT	0xCA
#define KASAN_ALLOCA_RIGHT	0xCB

#define KASAN_ALLOCA_REDZONE_SIZE	32

/* Don't break randconfig/all*config builds */
#ifndef KASAN_ABI_VERSION
#define KASAN_ABI_VERSION 1
#endif

/*
 * End of Macros
 */

/*
 * Start of Structure definitions
 */

//int kasan_depth;

struct kasan_bug_info {
        /* buffers to store report parts */
        const char *start;
        const char *end;
        char *bug_type;
        char *bug_info;

        /* Varible to store important details */
        const void *access_addr;
	const void *first_bad_addr;
	size_t access_size;
	bool is_write;
	unsigned long ip;
};



/* The layout of struct dictated by compiler */
struct kasan_source_location {
	const char *filename;
	int line_no;
	int column_no;
};

/* The layout of struct dictated by compiler */
struct kasan_global {
	const void *beg;		/* Address of the beginning of the global variable. */
	size_t size;			/* Size of the global variable. */
	size_t size_with_redzone;	/* Size of the variable + size of the red zone. 32 bytes aligned */
	const void *name;
	const void *module_name;	/* Name of the module where the global variable is declared. */
	unsigned long has_dynamic_init;	/* This needed for C++ */
#if KASAN_ABI_VERSION >= 4
	struct kasan_source_location *location;
#endif
#if KASAN_ABI_VERSION >= 5
	char *odr_indicator;
#endif
};

/**
 * Structures to keep alloc and free tracks *
 */

#define KASAN_STACK_DEPTH 64

struct kasan_track {
	u32 pid;
//	depot_stack_handle_t stack;
};

struct kasan_alloc_meta {
	struct kasan_track alloc_track;
	struct kasan_track free_track;
};

struct qlist_node {
	struct qlist_node *next;
};
struct kasan_free_meta {
	/* This field is used while the object is in the quarantine.
	 * Otherwise it might be used for the allocator freelist.
	 */
	struct qlist_node quarantine_link;
};

/*
 * End of Strcuture definitions
 */

/*
 * Start of Shadow translation functions 
 */

static inline const void *kasan_shadow_to_mem(const void *shadow_addr)
{
	return (void *)(((unsigned long)shadow_addr - KASAN_SHADOW_OFFSET)
		<< KASAN_SHADOW_SCALE_SHIFT);
}

static inline void *kasan_mem_to_shadow(const void *addr)
{
	return (void *)(((unsigned long)addr >> KASAN_SHADOW_SCALE_SHIFT)
		+ KASAN_SHADOW_OFFSET);
}

/*
 * End of Shadow translation functions
 */

/*
 * Start of Function prototypes
 */

/* kasan_init.c */

/* All the kasan init functions for page traversal have been removed */
void kasan_early_init(void);
void kasan_init(void);

/* kern_asan.c */
extern void kasan_enable_current(void);
extern void kasan_disable_current(void);
void kasan_unpoison_shadow(const void *address, size_t size);

void kasan_unpoison_task_stack(struct lwp *task);
void kasan_unpoison_stack_above_sp_to(const void *watermark);

void kasan_alloc_pages(struct page *page, unsigned int order);
void kasan_free_pages(struct page *page, unsigned int order);

void kasan_cache_create(struct pool_cache *cache, size_t *size,
			unsigned int *flags);

void kasan_cache_shrink(struct pool_cache *cache);
void kasan_cache_shutdown(struct pool_cache *cache);

void kasan_poison_slab(struct page *page);
void kasan_unpoison_object_data(struct pool_cache *cache, void *object);
void kasan_poison_object_data(struct pool_cache *cache, void *object);
void kasan_init_slab_obj(struct pool_cache *cache, const void *object);

void kasan_kmalloc_large(const void *ptr, size_t size, unsigned int flags);
void kasan_kfree_large(void *ptr, unsigned long ip);
void kasan_poison_kfree(void *ptr, unsigned long ip);
void kasan_kmalloc(struct pool_cache *s, const void *object, size_t size,
		  unsigned int flags);
void kasan_krealloc(const void *object, size_t new_size, unsigned int flags);

void kasan_slab_alloc(struct pool_cache *s, void *object, unsigned int flags);
bool kasan_slab_free(struct pool_cache *s, void *object, unsigned long ip);


int kasan_module_alloc(void *addr, size_t size);
void kasan_free_shadow(const struct vm_struct *vm);

size_t ksize(const void *);
static inline void kasan_unpoison_slab(const void *ptr) { ksize(ptr); }
size_t kasan_metadata_size(struct pool_cache *cache);

bool kasan_save_enable_multi_shot(void);
void kasan_restore_multi_shot(bool enabled);

struct kasan_alloc_meta *get_alloc_info(struct pool_cache *cache,
					const void *object);
struct kasan_free_meta *get_free_info(struct pool_cache *cache,
					const void *object);

void kasan_unpoison_task_stack_below(const void *watermark);
void __asan_register_globals(struct kasan_global *globals, size_t size);
void __asan_unregister_globals(struct kasan_global *globals, size_t size);
void __asan_loadN(unsigned long addr, size_t size);
void __asan_storeN(unsigned long addr, size_t size);
void __asan_handle_no_return(void);
void __asan_poison_stack_memory(const void *addr, size_t size);
void __asan_unpoison_stack_memory(const void *addr, size_t size);
void __asan_alloca_poison(unsigned long addr, size_t size);
void __asan_allocas_unpoison(const void *stack_top, const void *stack_bottom);

void __asan_load1(unsigned long addr);
void __asan_store1(unsigned long addr);
void __asan_load2(unsigned long addr);
void __asan_store2(unsigned long addr);
void __asan_load4(unsigned long addr);
void __asan_store4(unsigned long addr);
void __asan_load8(unsigned long addr);
void __asan_store8(unsigned long addr);
void __asan_load16(unsigned long addr);
void __asan_store16(unsigned long addr);

void __asan_load1_noabort(unsigned long addr);
void __asan_store1_noabort(unsigned long addr);
void __asan_load2_noabort(unsigned long addr);
void __asan_store2_noabort(unsigned long addr);
void __asan_load4_noabort(unsigned long addr);
void __asan_store4_noabort(unsigned long addr);
void __asan_load8_noabort(unsigned long addr);
void __asan_store8_noabort(unsigned long addr);
void __asan_load16_noabort(unsigned long addr);
void __asan_store16_noabort(unsigned long addr);

void __asan_set_shadow_00(void *addr, size_t size);
void __asan_set_shadow_f1(void *addr, size_t size);
void __asan_set_shadow_f2(void *addr, size_t size);
void __asan_set_shadow_f3(void *addr, size_t size);
void __asan_set_shadow_f5(void *addr, size_t size);
void __asan_set_shadow_f8(void *addr, size_t size);

/* kern_asan_report.c */

void kasan_report(unsigned long addr, size_t size,
		bool is_write, unsigned long ip);
void kasan_report_invalid_free(void *object, unsigned long ip);

/* kern_asan_quarantine.c */

void quarantine_put(struct kasan_free_meta *info, struct pool_cache *cache);
void quarantine_reduce(void);
void quarantine_remove_cache(struct pool_cache *cache);

#endif
