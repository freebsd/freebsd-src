#ifndef _SECURE_MEMORY_HEAP_H_
#define _SECURE_MEMORY_HEAP_H_
#include <sys/types.h>
#include <vm/uma.h>
#include <sys/kernel.h>
#include <sys/smp.h>
#include <vm/vm.h>
#include <vm/vm_kern.h>
#include <vm/vm_extern.h>
#include <vm/pmap.h>
#include <vm/vm_map.h>
#include <sys/vmem.h>
#include <vm/vm_object.h>

#include <sys/mutex.h>

/** \defgroup ALLOCHOOKS liballoc hooks 
 *
 * These are the OS specific functions which need to 
 * be implemented on any platform that the library
 * is expected to work on.
 */

/** @{ */



// If we are told to not define our own size_t, then we skip the define.
//#define _HAVE_UINTPTR_T
//typedef	unsigned long	uintptr_t;

//This lets you prefix malloc and friends
#define PREFIX(func)		smh_ ## func

#ifdef __cplusplus
extern "C" {
#endif

typedef struct liballoc_major *liballoc_major_t;

typedef struct secure_memory_heap {
    vmem_t *vmem;
    
    struct mtx mtx;
    liballoc_major_t l_memRoot;
    liballoc_major_t l_bestBet;
} * secure_memory_heap_t;

/** This function is supposed to lock the memory data structures. It
 * could be as simple as disabling interrupts or acquiring a spinlock.
 * It's up to you to decide. 
 *
 * \return 0 if the lock was acquired successfully. Anything else is
 * failure.
 */
extern int smh_lock(secure_memory_heap_t);

/** This function unlocks what was previously locked by the liballoc_lock
 * function.  If it disabled interrupts, it enables interrupts. If it
 * had acquiried a spinlock, it releases the spinlock. etc.
 *
 * \return 0 if the lock was successfully released.
 */
extern int smh_unlock(secure_memory_heap_t);

/** This is the hook into the local system which allocates pages. It
 * accepts an integer parameter which is the number of pages
 * required.  The page size was set up in the liballoc_init function.
 *
 * \return NULL if the pages were not allocated.
 * \return A pointer to the allocated memory.
 */
extern void* smh_page_alloc(secure_memory_heap_t, size_t);

/** This frees previously allocated memory. The void* parameter passed
 * to the function is the exact same value returned from a previous
 * liballoc_alloc call.
 *
 * The integer value is the number of pages to free.
 *
 * \return 0 if the memory was successfully freed.
 */
extern int smh_page_free(secure_memory_heap_t, void*,size_t);

extern void    *PREFIX(malloc)(secure_memory_heap_t, size_t);				///< The standard function.
extern void    *PREFIX(realloc)(secure_memory_heap_t, void *, size_t);		///< The standard function.
extern void    *PREFIX(calloc)(secure_memory_heap_t, size_t, size_t);		///< The standard function.
extern void     PREFIX(free)(secure_memory_heap_t, void *);					///< The standard function.

extern void smh_init(secure_memory_heap_t smh, const char *name,
				        	 vm_offset_t base, vm_size_t size);
#ifdef __cplusplus
}
#endif


/** @} */

#endif /* _SUBMAP_MALLOC_H_ */