#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD$");

#include <sys/param.h>
#include <sys/device.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/param.h>
#include <sys/conf.h>
#include <sys/cprng.h>
#include <sys/kmem.h>
#include <sys/systm.h>
#include <sys/types.h>
#include <sys/filedesc.h>
#include <sys/lwp.h>
#include <sys/pool.h>

#include <amd64/pcb.h>
#include <sys/kasan.h>

#define _RET_IP_      (unsigned long)__builtin_return_address(0)

//Typedefs for current version
typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;

typedef uint8_t __u8;
typedef uint16_t __u16;
typedef uint32_t __u32;
typedef uint64_t __u64;

typedef int8_t s8;
typedef int16_t s16;
typedef int32_t s32;
typedef int64_t s64;

typedef int8_t __s8;
typedef int16_t __s16;
typedef int32_t __s32;
typedef int64_t __s64;

typedef uint16_t __le16;
typedef uint32_t __le32;
typedef uint64_t __le64;

typedef uint16_t __be16;
typedef uint32_t __be32;
typedef uint64_t __be64;
//End of typedefs


#define IS_ALIGNED(x, a)(((x) & ((typeof(x))(a) - 1)) == 0)
#define __round_mask(x, y) ((__typeof__(x))((y)-1))
#define round_up(x, y) ((((x)-1) | __round_mask(x, y))+1)
#define round_down(x, y) ((x) & ~__round_mask(x, y))
#define THREAD_SIZE 4086

/* I don't see a __memory_barrier in linux
#ifndef barrier
# define barrier() __memory_barrier()
#endif
*/
/* Barriers were removed  */
#define __READ_ONCE_SIZE						\
({									\
	switch (size) {							\
	case 1: *(__u8 *)res = *(__u8 *)p; break;		\
	case 2: *(__u16 *)res = *(__u16 *)p; break;		\
	case 4: *(__u32 *)res = *(__u32 *)p; break;		\
	case 8: *(__u64 *)res = *(__u64 *)p; break;		\
	default:							\
		__builtin_memcpy((void *)res, (void *)p, size);	\
	}								\
})

static __always_inline
void __read_once_size(void *p, void *res, int size)
{
	__READ_ONCE_SIZE;
}

//static __no_kasan_or_inline
static __always_inline
void __read_once_size_nocheck(void *p, void *res, int size)
{
	__READ_ONCE_SIZE;
}

//	smp_read_barrier_depends();
//   Above line is not added.
#define __READ_ONCE(x, check)						\
({									\
	union { typeof(x) __val; char __c[1]; } __u;			\
	if (check)							\
		__read_once_size(&(x), __u.__c, sizeof(x));		\
	else								\
		__read_once_size_nocheck(&(x), __u.__c, sizeof(x));	\
	__u.__val;							\
})
#define READ_ONCE(x) __READ_ONCE(x, 1)


#define SLAB_KASAN 100 //temp
#define SLAB_TYPESAFE_BY_RCU 100 //temp
#define KMALLOC_MAX_SIZE 100 //temp
#define ZERO_SIZE_PTR (void *)100 //temp
#define GFP_NOWAIT 100 //temp
#define VM_KASAN 100 //temp


/* Function declarations for KASAN Functions  */
void kasan_check_read(const volatile void *, unsigned int);
void * task_stack_page(struct lwp * );
void kasan_check_write(const volatile void *, unsigned int);


/* End of Function declarations for KASAN Functions */

/*
void kasan_enable_current(void)
{
	kasan_depth++;
}

void kasan_disable_current(void)
{
	kasan_depth--;
}
*/

/*
 * Dummy functions for timebeing
 */

bool PageHighMem(struct page *);
bool PageHighMem(struct page *Page) {
        return true;
}

bool PageSlab(struct page *);
bool PageSlab(struct page *Page) {
        return true;
}

void * page_address(struct page *);
void * page_address(struct page *Page) {
        return (void *)0;
}

struct page * virt_to_page(const void *);
struct page * virt_to_page(const void *test) {
        return (struct page *)0;
}

struct page * virt_to_head_page(const void *);
struct page * virt_to_head_page(const void *test) {
        return (struct page *)0;
}

int compound_order(struct page *);
int compound_order(struct page *Page) {
        return 0;
}

void * nearest_obj(struct pool_cache *, struct page *, void *);
void * nearest_obj(struct pool_cache *cache, struct page *Page, void *obj)
{
        return (void *)0;
}
/*
 * End of Dummy functions
 */

/*
 * Start of NetBSD kernel alternatives
 */
/*
 * Used to return the page mapping the stack of a lwp
 */
void * task_stack_page(struct lwp *task) {
        struct pcb *pb = lwp_getpcb(task);
        return (void *)pb->pcb_rbp;
}
/*
 * End of NetBSD kernel alternatives 
 */


/*
 * Poisons the shadow memory for 'size' bytes starting from 'addr'.
 * Memory addresses should be aligned to KASAN_SHADOW_SCALE_SIZE.
 */
static void
kasan_poison_shadow(const void *address, size_t size, u8 value)
{
	void *shadow_start, *shadow_end;

        /*
         * Find the shadow offsets of the start and end address
         */
	shadow_start = kasan_mem_to_shadow(address);
	shadow_end = kasan_mem_to_shadow((void *)((uintptr_t)address + 
                    size));

        /*
         * Use memset to populate the region with the given value
         */
	__builtin_memset(shadow_start, value, (char *)shadow_end - 
                (char *)shadow_start);
}

/*
 * unpoisons the shadow memory for 'size' bytes starting from 'addr'.
 * Memory addresses should be aligned to KASAN_SHADOW_SCALE_SIZE.
 */
void
kasan_unpoison_shadow(const void *address, size_t size)
{
	kasan_poison_shadow(address, size, 0);

	if (size & KASAN_SHADOW_MASK) {
		u8 *shadow = (u8 *)kasan_mem_to_shadow((void *)
                        ((uintptr_t)address + size));
		*shadow = size & KASAN_SHADOW_MASK;
	}
}

/* Unpoison the stack from the page with the stack base */
static void
__kasan_unpoison_stack(struct lwp *task, const void *sp)
{
	void *base = task_stack_page(task);
	size_t size = (const char *)sp - (const char *)base;

	kasan_unpoison_shadow(base, size);
}

/* Unpoison the entire stack for a task. */
void
kasan_unpoison_task_stack(struct lwp *task)
{
	__kasan_unpoison_stack(task,(void *) 
                ((uintptr_t)task_stack_page(task) + THREAD_SIZE));
}

/* Unpoison the stack for the current task beyond a watermark sp value. */
void
kasan_unpoison_task_stack_below(const void *watermark)
{

	/* Calculate the task stack base address. */
	void *base = (void *)((unsigned long)watermark & 
                ~(THREAD_SIZE - 1));

	kasan_unpoison_shadow(base, (const char *)watermark - 
                (char *)base);
}

/*
 * Clear all poison for the region between the current SP and a provided
 * watermark value, as is sometimes required prior to hand-crafted 
 * asm function returns in the middle of functions.
 */
void
kasan_unpoison_stack_above_sp_to(const void *watermark)
{
	const void *sp = __builtin_frame_address(0);
	size_t size = (const char *)watermark - (const char *)sp;

        /* Make sure that sp is below the watermark */
//        if (KASSERT(sp <= watermark))
//        if (KASSERT((int64_t)size < 0 ))
//                return;
	kasan_unpoison_shadow(sp, size);
}

/*
 * All functions below always inlined so compiler could
 * perform better optimizations in each of __asan_loadX/__assn_storeX
 * depending on memory access size X.
 */

static __always_inline bool
memory_is_poisoned_1(unsigned long addr)
{
	s8 shadow_value = *(s8 *)kasan_mem_to_shadow((void *)addr);

	if (__predict_false(shadow_value)) {
		s8 last_accessible_byte = addr & KASAN_SHADOW_MASK;
		return __predict_false(last_accessible_byte >=
                        shadow_value);
	}

	return false;
}

static __always_inline bool
memory_is_poisoned_2_4_8(unsigned long addr,
    unsigned long size)
{
	u8 *shadow_addr = (u8 *)kasan_mem_to_shadow((void *)addr);

	/*
	 * Access crosses 8(shadow size)-byte boundary. Such access maps
	 * into 2 shadow bytes, so we need to check them both.
	 */
	if (__predict_false(((addr + size - 1) & KASAN_SHADOW_MASK)
                    < size - 1))
		return *shadow_addr || memory_is_poisoned_1(addr + 
                        size - 1);

	return memory_is_poisoned_1(addr + size - 1);
}

static __always_inline bool
memory_is_poisoned_16(unsigned long addr)
{
	u16 *shadow_addr = (u16 *)kasan_mem_to_shadow((void *)addr);

	/* Unaligned 16-bytes access maps into 3 shadow bytes. */
	if (__predict_false(!IS_ALIGNED(addr, KASAN_SHADOW_SCALE_SIZE)))
		return *shadow_addr || memory_is_poisoned_1(addr + 15);

	return *shadow_addr;
}

/* Function to check whether a set of bytes is not zero */
static __always_inline unsigned long
bytes_is_nonzero(const u8 *start, size_t size)
{
	while (size) {
                /* If the byte is not zero return the value */
		if (__predict_false(*start))
			return (unsigned long)start;
		start++;
		size--;
	}

	return 0;
}

/*
 * Function to make sure whether a range of memory in the shadow region is 
 * non zero to check whether the action being perfomed is legal.
 */
static __always_inline unsigned long
memory_is_nonzero(const void *start, const void *end)
{
	unsigned int words;
	unsigned long ret;
	unsigned int prefix = (const unsigned long)start % 8;

        /* 
         * If the size is less that 16 bytes then use bytes_is_nonzero 
         * since we don't need to care about the allignment at all. 
         */
	if ((const char *)end - (const char *)start <= 16)
		return bytes_is_nonzero(start,(unsigned long)
                        ((const char *)end - (const char *)start));

        /* Check the non aligned bytes and check if they are non zero. */
	if (prefix) {
		prefix = 8 - prefix;
		ret = bytes_is_nonzero(start, prefix);
		if (__predict_false(ret))
                  return ret;
	        start =(void *)((uintptr_t)start + (uintptr_t)prefix);
        }

        /* Check the memory region by taking chunks of 8 bytes each time */
	words = ((const char *)end - (const char *)start) / 8;
	while (words) {
		if (__predict_false(*(const u64 *)start))
			return bytes_is_nonzero(start, 8);
		start =(void *)((uintptr_t)start + (uintptr_t)8);
                words--;
	}

	return bytes_is_nonzero(start, (unsigned long)((const char *)end - 
                    (const char *)start) % 8);
}

/*
 * Function to make sure that n bytes of memory in the shadow region are 
 * poisoned according to the request.
 */
static __always_inline bool
memory_is_poisoned_n(unsigned long addr, size_t size)
{
	unsigned long ret;
   
        /* Check if the memory region is non zero  */
	ret = memory_is_nonzero(kasan_mem_to_shadow((void *)addr),
			(char *)kasan_mem_to_shadow((void *)(addr 
                                + size - 1)) + 1);

        /* If the memory seems to be poisoned (non zero) */
	if (__predict_false(ret)) {
                /* take the last byte and its corresponding shadow offset */
		unsigned long last_byte = addr + size - 1;
		s8 *last_shadow = (s8 *)kasan_mem_to_shadow((void *)last_byte);

                /* if */
		if (__predict_false(ret != (unsigned long)last_shadow ||
			((long)(last_byte & KASAN_SHADOW_MASK) 
                         >= *last_shadow)))
			return true;
	}
	return false;
}

/* 
 * Function to decide and call the corresponding function to check the 
 * poisoning based on the size that was given 
 */
static __always_inline bool
memory_is_poisoned(unsigned long addr, size_t size)
{
	if (__builtin_constant_p(size)) {
		switch (size) {
		case 1:
			return memory_is_poisoned_1(addr);
		case 2:
		case 4:
		case 8:
			return memory_is_poisoned_2_4_8(addr, size);
		case 16:
			return memory_is_poisoned_16(addr);
		default:
			KASSERT(0 && "Not reached");
		}
	}

	return memory_is_poisoned_n(addr, size);
}

/*
 * Inline function used to check whether the given memory access is 
 * proper. 
 */
static __always_inline void
check_memory_region_inline(unsigned long addr, size_t size, bool write,
	unsigned long ret_ip)
{

	if (__predict_false(size == 0))
		return;

        /* Check if the address is a valid kernel address */
	if (__predict_false((void *)addr <
		kasan_shadow_to_mem((void *)KASAN_SHADOW_START))) {
		kasan_report(addr, size, write, ret_ip);
		return;
	}

        /* If the memory is not poisoned then return normally */
	if (__predict_true(!memory_is_poisoned(addr, size)))
		return;

        /* Bug found, proceed to report the bug */
	kasan_report(addr, size, write, ret_ip);

}


static void
check_memory_region(unsigned long addr, size_t size, bool write,
	unsigned long ret_ip)
{
	check_memory_region_inline(addr, size, write, ret_ip);
}

void
kasan_check_read(const volatile void *p, unsigned int size)
{
	check_memory_region((unsigned long)p, size, false, _RET_IP_);
}

void
kasan_check_write(const volatile void *p, unsigned int size)
{
	check_memory_region((unsigned long)p, size, true, _RET_IP_);
}
/*
MULTIPLE DEFINITION errors while linking here
#undef memset
void *memset(void *addr, int c, size_t len)
{
	check_memory_region((unsigned long)addr, len, true, _RET_IP_);

	return __builtin_memset(addr, c, len);
}
#undef memmove
void *memmove(void *dest, const void *src, size_t len)
{
	check_memory_region((unsigned long)src, len, false, _RET_IP_);
	check_memory_region((unsigned long)dest, len, true, _RET_IP_);

	return __memmove(dest, src, len);
}

#undef memcpy
void *memcpy(void *dest, const void *src, size_t len)
{
	check_memory_region((unsigned long)src, len, false, _RET_IP_);
	check_memory_region((unsigned long)dest, len, true, _RET_IP_);

	return __builtin_memcpy(dest, src, len);
}
*/

/*
 * Function to unpoison the shadow offset of a page which is being allocated
 * only if it is not a Highmem page (Not applicable for amd64)
 */
void
kasan_alloc_pages(struct page *page, unsigned int order)
{
	if (__predict_true(!PageHighMem(page)))
		kasan_unpoison_shadow(page_address(page), PAGE_SIZE << order);
}

/*
 * Function to poison the shadow offset of a page which is being freed 
 * only if it is not a Highmem page (Not applicable for amd64)
 */
void
kasan_free_pages(struct page *page, unsigned int order)
{
	if (__predict_true(!PageHighMem(page)))
		kasan_poison_shadow(page_address(page), PAGE_SIZE << order,
		    KASAN_FREE_PAGE);
}

/*
 * Adaptive redzone policy taken from the userspace AddressSanitizer runtime.
 * For larger allocations larger redzones are used.
 */
static unsigned int
optimal_redzone(unsigned int object_size)
{
	return
		object_size <= 64        - 16   ? 16 :
		object_size <= 128       - 32   ? 32 :
		object_size <= 512       - 64   ? 64 :
		object_size <= 4096      - 128  ? 128 :
		object_size <= (1 << 14) - 256  ? 256 :
		object_size <= (1 << 15) - 512  ? 512 :
		object_size <= (1 << 16) - 1024 ? 1024 : 2048;
}

/*
 * Function to initialize the kasan_info struct inside the pool_cache
 * struct used by kmem(8) and pool_cache(8) allocators with the details of
 * the allocation.
 */
void
kasan_cache_create(struct pool_cache *cache, size_t *size,
			unsigned int *flags)
{
	unsigned int orig_size = *size;
	int redzone_adjust;

	/* Add alloc meta. */
	cache->kasan_info.alloc_meta_offset = *size;
	*size += sizeof(struct kasan_alloc_meta);

	/* Add free meta. */
	if (cache->pc_pool.pr_flags & SLAB_TYPESAFE_BY_RCU || //cache->ctor ||
	    cache->pc_reqsize < sizeof(struct kasan_free_meta)) {
		cache->kasan_info.free_meta_offset = *size;
		*size += sizeof(struct kasan_free_meta);
	}
	redzone_adjust = optimal_redzone(cache->pc_reqsize) -
		(*size - cache->pc_reqsize);

	if (redzone_adjust > 0)
		*size += redzone_adjust;

	*size = min(KMALLOC_MAX_SIZE,
			max(*size, cache->pc_reqsize +
					optimal_redzone(cache->pc_reqsize)));

	/*
	 * If the metadata doesn't fit, don't enable KASAN at all.
	 */
	if (*size <= cache->kasan_info.alloc_meta_offset ||
			*size <= cache->kasan_info.free_meta_offset) {
		cache->kasan_info.alloc_meta_offset = 0;
		cache->kasan_info.free_meta_offset = 0;
		*size = orig_size;
		return;
	}

	*flags |= SLAB_KASAN;
}

/*
 * Functions to be called by the Pagedaemon since there are no shrink and
 * shutdown functions in the cache allocator. Will add after the quarantine
 * list feature is ready
 */

/*
void
kasan_cache_shrink(struct pool_cache *cache)
{
	quarantine_remove_cache(cache);
}

void
kasan_cache_shutdown(struct pool_cache *cache)
{
	if (!__pool_cache_empty(cache))
		quarantine_remove_cache(cache);
}
*/

/* 
 * Function to return the total size of the alloc and free meta structure
 * Returns 0 if the structres don't exist 
 */
size_t
kasan_metadata_size(struct pool_cache *cache)
{
	return (cache->kasan_info.alloc_meta_offset ?
		sizeof(struct kasan_alloc_meta) : 0) +
		(cache->kasan_info.free_meta_offset ?
		sizeof(struct kasan_free_meta) : 0);
}

/*
 * Function to poison a pool of cache memory. The function needs renaming.
 */
void
kasan_poison_slab(struct page *page)
{
	kasan_poison_shadow(page_address(page),
			PAGE_SIZE << compound_order(page),
			KASAN_KMALLOC_REDZONE);
}

/* 
 * Function to unpoison a object of memory that is allocated by the allocator -
 * in this case the pool_cache or the kmem allocator
 */
void
kasan_unpoison_object_data(struct pool_cache *cache, void *object)
{
	kasan_unpoison_shadow(object, cache->pc_reqsize);
}

/* 
 * Function to poison a object of memory that is allocated by the allocator -
 * in this case the pool_cache or the kmem allocator
 */
void 
kasan_poison_object_data(struct pool_cache *cache, void *object)
{
	kasan_poison_shadow(object,
			round_up(cache->pc_reqsize, KASAN_SHADOW_SCALE_SIZE),
			KASAN_KMALLOC_REDZONE);
}

/*
 * Set of Functions to handle irq stacks - will be ported later
 */
/*
static inline int 
in_irqentry_text(unsigned long ptr)
{
	return (ptr >= (unsigned long)&__irqentry_text_start &&
		ptr < (unsigned long)&__irqentry_text_end) ||
		(ptr >= (unsigned long)&__softirqentry_text_start &&
		 ptr < (unsigned long)&__softirqentry_text_end);
}

static inline void 
filter_irq_stacks(struct stack_trace *trace)
{
	int i;

	if (!trace->nr_entries)
		return;
	for (i = 0; i < trace->nr_entries; i++)
		if (in_irqentry_text(trace->entries[i])) {
*/			/* Include the irqentry function into the stack. */
/*			trace->nr_entries = i + 1;
			break;
		}
}

static inline depot_stack_handle_t 
save_stack(unsigned int flags)
{
	unsigned long entries[KASAN_STACK_DEPTH];
	struct stack_trace trace = {
		.nr_entries = 0,
		.entries = entries,
		.max_entries = KASAN_STACK_DEPTH,
		.skip = 0
	};

	save_stack_trace(&trace);
	filter_irq_stacks(&trace);
	if (trace.nr_entries != 0 &&
	    trace.entries[trace.nr_entries-1] == ULONG_MAX)
		trace.nr_entries--;

	return depot_save_stack(&trace, flags);
}
*/

/* Function to set the pid and stack in the kasan_track structure */
static inline void 
set_track(struct kasan_track *track, unsigned int flags)
{
/*  
 *      Haven't decided on how to proceed with this yet.
 *
 *	track->pid = current->pid;
 *	track->stack = save_stack(flags);
 */
}

/* 
 * Function to retrieve address of the structure which contains the details of
 * allocation.
 */
struct kasan_alloc_meta 
*get_alloc_info(struct pool_cache *cache, const void *object)
{
	KASSERT(sizeof(struct kasan_alloc_meta) > 32);
	return (void *)((char *)__UNCONST(object) +
                cache->kasan_info.alloc_meta_offset);
}

/* 
 * Function to retrieve address of the structure which contains the details of
 * the memory which was freed.
 */
struct kasan_free_meta 
*get_free_info(struct pool_cache *cache, const void *object)
{
	KASSERT(sizeof(struct kasan_free_meta) > 32);
	return (void *)((char *)__UNCONST(object) + 
                cache->kasan_info.free_meta_offset);
}

/*
 * Function allocates a structure of alloc info in the address pointed by the
 * get_alloc_info function. Requires renaming and possible flag change.
 */
void 
kasan_init_slab_obj(struct pool_cache *cache, const void *object)
{
	struct kasan_alloc_meta *alloc_info;

        /* If the cache already has a alloc info struct? */
	if (!(cache->pc_pool.pr_flags & SLAB_KASAN))
		return;

	alloc_info = get_alloc_info(cache, object);
	__builtin_memset(alloc_info, 0, sizeof(*alloc_info));
}

/* Allocate memory for the slab using kasan_kmalloc */
void 
kasan_slab_alloc(struct pool_cache *cache, void *object, unsigned int flags)
{
	kasan_kmalloc(cache, object, cache->pc_reqsize, flags);
}

static bool 
__kasan_slab_free(struct pool_cache *cache, void *object,
    unsigned long ip, bool quarantine)
{
	s8 shadow_byte;
	unsigned long rounded_up_size;

        /* 
         * Check if it was a invalid free by checking whether the object was 
         * a part of the cache. Will need to rethink this.
         */
	if (__predict_false(nearest_obj(cache, virt_to_head_page(object), 
            object) !=object)) {
//		kasan_report_invalid_free(object, ip);
		return true;
	}

	/* RCU slabs could be legally used after free within the RCU period */
	if (__predict_false(cache->pc_pool.pr_flags & SLAB_TYPESAFE_BY_RCU))
		return false;

        /*
         * If the memory wasn't posioned then it means that it is a invalid
         * free of memory since the memory has never been allocated.
         */
	shadow_byte = READ_ONCE(*(s8 *)kasan_mem_to_shadow(object));
	if (shadow_byte < 0 || shadow_byte >= KASAN_SHADOW_SCALE_SIZE) {
//		kasan_report_invalid_free(object, ip);
		return true;
	}
        
        /*
         * Poison the object since it is not usable anymore as it has been
         * freed. Poison is done according to the size rounded up to the kasan
         * scale (2 << 8)
         */
	rounded_up_size = round_up(cache->pc_reqsize, KASAN_SHADOW_SCALE_SIZE);
	kasan_poison_shadow(object, rounded_up_size, KASAN_KMALLOC_FREE);

	if (!quarantine || __predict_false(!(cache->pc_pool.pr_flags & 
            SLAB_KASAN)))
		return false;

	/*
         * Set the kasan_track structure and proceed to put the object in the
         * quarantine list.
         */
        set_track(&get_alloc_info(cache, object)->free_track, GFP_NOWAIT);
//	quarantine_put(get_free_info(cache, object), cache);
	return true;
}

/* Wrapper function for slab free - subject to change*/
bool 
kasan_slab_free(struct pool_cache *cache, void *object, unsigned long ip)
{
	return __kasan_slab_free(cache, object, ip, true);
}

/* Kasan implementation of kmalloc */
void 
kasan_kmalloc(struct pool_cache *cache, const void *object, size_t size,
		   unsigned int flags)
{
	unsigned long redzone_start;
	unsigned long redzone_end;
/* look at quarantine later
	if (gfpflags_allow_blocking(flags))
		quarantine_reduce();
*/
	if (__predict_false(object == NULL))
		return;

        /* Caluclate redzone to catch oveflows */
	redzone_start = round_up(((unsigned long)object + size),
				KASAN_SHADOW_SCALE_SIZE);
	redzone_end = round_up((unsigned long)object + cache->pc_reqsize,
				KASAN_SHADOW_SCALE_SIZE);

        /* Unpoison the memory of the object and posion the redzone region */
	kasan_unpoison_shadow(object, size);
	kasan_poison_shadow((void *)redzone_start, redzone_end - redzone_start,
		KASAN_KMALLOC_REDZONE);

        /* Need to reimplement the flag here */
	if (cache->pc_pool.pr_flags & SLAB_KASAN)
		set_track(&get_alloc_info(cache, object)->alloc_track, flags);
}

/*
 * kmalloc for large allocations - Might need to rethink this since the pool
 * cache allocator seems to go for a direct allocation with the pool allocator
 * for large memories. 
 */
void 
kasan_kmalloc_large(const void *ptr, size_t size, unsigned int flags)
{
	struct page *page;
	unsigned long redzone_start;
	unsigned long redzone_end;
/*
	if (gfpflags_allow_blocking(flags))
		quarantine_reduce();
*/
	if (__predict_false(ptr == NULL))
		return;

        /* Calculate the redzone offsets to catch overflows */
	page = virt_to_page(ptr);
	redzone_start = round_up(((unsigned long)ptr + size),
				KASAN_SHADOW_SCALE_SIZE);
	redzone_end = (unsigned long)ptr + (PAGE_SIZE << compound_order(page));

        /* Unpoison the object and poison the redzone */
	kasan_unpoison_shadow(ptr, size);
	kasan_poison_shadow((void *)redzone_start, redzone_end - redzone_start,
		KASAN_PAGE_REDZONE);
}

/* kasan implementation of krealloc */
void 
kasan_krealloc(const void *object, size_t size, unsigned int flags)
{
	struct page *page;

	if (__predict_false(object == ZERO_SIZE_PTR))
		return;

	page = virt_to_head_page(object);

        /* Need to decide when to call kmalloc and kmalloc large */
	if (__predict_false(!PageSlab(page)))
		kasan_kmalloc_large(object, size, flags);
//	else
//		kasan_kmalloc(page->slab_cache, object, size, flags);
}

/* 
 * NetBSD alternative - free has been depreciated and made a wrapper around
 * kmem_free. Will remove if no functions need this.
 */
void 
kasan_poison_kfree(void *ptr, unsigned long ip)
{
	struct page *page;

	page = virt_to_head_page(ptr);

	if (__predict_false(!PageSlab(page))) {
		if (ptr != page_address(page)) {
//			kasan_report_invalid_free(ptr, ip);
			return;
		}
		kasan_poison_shadow(ptr, PAGE_SIZE << compound_order(page),
				KASAN_FREE_PAGE);
	} else {
//		__kasan_slab_free(page->slab_cache, ptr, ip, false);
	}
}

/* 
 * NetBSD alternative - free has been deprecited and made a wrapper around
 * kmem_free. Will remove if no functions need this.
 */
void 
kasan_kfree_large(void *ptr, unsigned long ip)
{
	if (ptr != page_address(virt_to_head_page(ptr)))
                return ;
//		kasan_report_invalid_free(ptr, ip);
	/* The object will be poisoned by page_alloc. */

}

/* Module part will be dealt with later
int kasan_module_alloc(void *addr, size_t size)
{
	void *ret;
	size_t shadow_size;
	unsigned long shadow_start;

	shadow_start = (unsigned long)kasan_mem_to_shadow(addr);
	shadow_size = round_up(size >> KASAN_SHADOW_SCALE_SHIFT,
			PAGE_SIZE);

	if (WARN_ON(!PAGE_ALIGNED(shadow_start)))
		return -EINVAL;

	ret = __vmalloc_node_range(shadow_size, 1, shadow_start,
			shadow_start + shadow_size,
			GFP_KERNEL | __GFP_ZERO,
			PAGE_KERNEL, VM_NO_GUARD, NUMA_NO_NODE,
			__builtin_return_address(0));

	if (ret) {
		find_vm_area(addr)->flags |= VM_KASAN;
		kmemleak_ignore(ret);
		return 0;
	}

	return -ENOMEM;
}
*/

/* TODO */
void 
kasan_free_shadow(const struct vm_struct *vm)
{
/*
	if (vm->flags & VM_KASAN)
		vfree(kasan_mem_to_shadow(vm->addr));
*/
}

/* */
static void 
register_global(struct kasan_global *global)
{
	size_t aligned_size = round_up(global->size, KASAN_SHADOW_SCALE_SIZE);

        /* unposion the global area */
	kasan_unpoison_shadow(global->beg, global->size);

	kasan_poison_shadow((void *)((uintptr_t)global->beg + aligned_size),
		global->size_with_redzone - aligned_size,
		KASAN_GLOBAL_REDZONE);
}

void 
__asan_register_globals(struct kasan_global *globals, size_t size)
{
	int i;

	for (i = 0; i < size; i++)
		register_global(&globals[i]);
}

void 
__asan_unregister_globals(struct kasan_global *globals, size_t size)
{
}

#define DEFINE_ASAN_LOAD_STORE(size)				\
	void __asan_load##size(unsigned long addr)		\
	{                                                       \
        check_memory_region_inline(addr, size, false, _RET_IP_);\
        } \
	void __asan_load##size##_noabort(unsigned long);	\
	void __asan_load##size##_noabort(unsigned long addr)	\
	{\
        check_memory_region_inline(addr, size, false, _RET_IP_);\
        }							\
	void __asan_store##size(unsigned long addr)		\
	{\
        check_memory_region_inline(addr, size, true, _RET_IP_);\
        }							\
	void __asan_store##size##_noabort(unsigned long);	\
	void __asan_store##size##_noabort(unsigned long addr)	\
	{\
        check_memory_region_inline(addr, size, true, _RET_IP_);\
        }							\


DEFINE_ASAN_LOAD_STORE(1);
DEFINE_ASAN_LOAD_STORE(2);
DEFINE_ASAN_LOAD_STORE(4);
DEFINE_ASAN_LOAD_STORE(8);
DEFINE_ASAN_LOAD_STORE(16);

void 
__asan_loadN(unsigned long addr, size_t size)
{
        check_memory_region(addr, size, false, _RET_IP_);
}

void __asan_loadN_noabort(unsigned long, size_t);
void 
__asan_loadN_noabort(unsigned long addr, size_t size)
{
        check_memory_region(addr, size, false, _RET_IP_);
}

void __asan_storeN(unsigned long addr, size_t size)
{
        check_memory_region(addr, size, true, _RET_IP_);
}

void __asan_storeN_noabort(unsigned long, size_t);
void 
__asan_storeN_noabort(unsigned long addr, size_t size)
{
        check_memory_region(addr, size, true, _RET_IP_);
}

/* to shut up compiler complaints */

void __asan_handle_no_return(void) {}

/* Emitted by compiler to poison large objects when they go out of scope. */
void 
__asan_poison_stack_memory(const void *addr, size_t size)
{

	/*
	 * Addr is KASAN_SHADOW_SCALE_SIZE-aligned and the object is surrounded
	 * by redzones, so we simply round up size to simplify logic.
	 */
	kasan_poison_shadow(addr, round_up(size, KASAN_SHADOW_SCALE_SIZE),
			    KASAN_USE_AFTER_SCOPE);
}

/* Emitted by compiler to unpoison large objects when they go into scope. */

void 
__asan_unpoison_stack_memory(const void *addr, size_t size)
{
	kasan_unpoison_shadow(addr, size);
}

/* Emitted by compiler to poison alloca()ed objects. */
void 
__asan_alloca_poison(unsigned long addr, size_t size)
{
	size_t rounded_up_size = round_up(size, KASAN_SHADOW_SCALE_SIZE);
	size_t padding_size = round_up(size, KASAN_ALLOCA_REDZONE_SIZE) -
			rounded_up_size;
	size_t rounded_down_size = round_down(size, KASAN_SHADOW_SCALE_SIZE);

	const void *left_redzone = (const void *)(addr -
			KASAN_ALLOCA_REDZONE_SIZE);
	const void *right_redzone = (const void *)(addr + rounded_up_size);

	//WARN_ON(!IS_ALIGNED(addr, KASAN_ALLOCA_REDZONE_SIZE));

	kasan_unpoison_shadow((const void *)(addr + rounded_down_size),
			      size - rounded_down_size);
	kasan_poison_shadow(left_redzone, KASAN_ALLOCA_REDZONE_SIZE,
			KASAN_ALLOCA_LEFT);
	kasan_poison_shadow(right_redzone,
			padding_size + KASAN_ALLOCA_REDZONE_SIZE,
			KASAN_ALLOCA_RIGHT);
}

/* Emitted by compiler to unpoison alloca()ed areas when the stack unwinds. */
void 
__asan_allocas_unpoison(const void *stack_top, const void *stack_bottom)
{
	if (__predict_false(!stack_top || stack_top > stack_bottom))
		return;

	kasan_unpoison_shadow(stack_top, (const char *)stack_bottom - (const char *)stack_top);
}

/* Emitted by the compiler to [un]poison local variables. */
#define DEFINE_ASAN_SET_SHADOW(byte) \
	void __asan_set_shadow_##byte(void *addr, size_t size)	\
	{								\
	        __builtin_memset((void *)addr, 0x##byte, size);		\
	}								\

DEFINE_ASAN_SET_SHADOW(00);
DEFINE_ASAN_SET_SHADOW(f1);
DEFINE_ASAN_SET_SHADOW(f2);
DEFINE_ASAN_SET_SHADOW(f3);
DEFINE_ASAN_SET_SHADOW(f5);
DEFINE_ASAN_SET_SHADOW(f8);
/*
#ifdef CONFIG_MEMORY_HOTPLUG
static bool shadow_mapped(unsigned long addr)
{
	pgd_t *pgd = pgd_offset_k(addr);
	p4d_t *p4d;
	pud_t *pud;
	pmd_t *pmd;
	pte_t *pte;

	if (pgd_none(*pgd))
		return false;
	p4d = p4d_offset(pgd, addr);
	if (p4d_none(*p4d))
		return false;
	pud = pud_offset(p4d, addr);
	if (pud_none(*pud))
		return false;
*/
	/*
	 * We can't use pud_large() or pud_huge(), the first one is
	 * arch-specific, the last one depends on HUGETLB_PAGE.  So let's abuse
	 * pud_bad(), if pud is bad then it's bad because it's huge.
	 */
/*	if (pud_bad(*pud))
		return true;
	pmd = pmd_offset(pud, addr);
	if (pmd_none(*pmd))
		return false;

	if (pmd_bad(*pmd))
		return true;
	pte = pte_offset_kernel(pmd, addr);
	return !pte_none(*pte);
}

static int __meminit kasan_mem_notifier(struct notifier_block *nb,
			unsigned long action, void *data)
{
	struct memory_notify *mem_data = data;
	unsigned long nr_shadow_pages, start_kaddr, shadow_start;
	unsigned long shadow_end, shadow_size;

	nr_shadow_pages = mem_data->nr_pages >> KASAN_SHADOW_SCALE_SHIFT;
	start_kaddr = (unsigned long)pfn_to_kaddr(mem_data->start_pfn);
	shadow_start = (unsigned long)kasan_mem_to_shadow((void *)start_kaddr);
	shadow_size = nr_shadow_pages << PAGE_SHIFT;
	shadow_end = shadow_start + shadow_size;

	if (WARN_ON(mem_data->nr_pages % KASAN_SHADOW_SCALE_SIZE) ||
		WARN_ON(start_kaddr % (KASAN_SHADOW_SCALE_SIZE << PAGE_SHIFT)))
		return NOTIFY_BAD;

	switch (action) {
	case MEM_GOING_ONLINE: {
		void *ret;
*/
		/*
		 * If shadow is mapped already than it must have been mapped
		 * during the boot. This could happen if we onlining previously
		 * offlined memory.
		 */
/*		if (shadow_mapped(shadow_start))
			return NOTIFY_OK;

		ret = __vmalloc_node_range(shadow_size, PAGE_SIZE, shadow_start,
					shadow_end, GFP_KERNEL,
					PAGE_KERNEL, VM_NO_GUARD,
					pfn_to_nid(mem_data->start_pfn),
					__builtin_return_address(0));
		if (!ret)
			return NOTIFY_BAD;

		kmemleak_ignore(ret);
		return NOTIFY_OK;
	}
	case MEM_CANCEL_ONLINE:
	case MEM_OFFLINE: {
		struct vm_struct *vm;
*/
		/*
		 * shadow_start was either mapped during boot by kasan_init()
		 * or during memory online by __vmalloc_node_range().
		 * In the latter case we can use vfree() to free shadow.
		 * Non-NULL result of the find_vm_area() will tell us if
		 * that was the second case.
		 *
		 * Currently it's not possible to free shadow mapped
		 * during boot by kasan_init(). It's because the code
		 * to do that hasn't been written yet. So we'll just
		 * leak the memory.
		 */
/*		vm = find_vm_area((void *)shadow_start);
		if (vm)
			vfree((void *)shadow_start);
	}
	}

	return NOTIFY_OK;
}

static int __init kasan_memhotplug_init(void)
{
	hotplug_memory_notifier(kasan_mem_notifier, 0);

	return 0;
}

core_initcall(kasan_memhotplug_init);
#endif*/
