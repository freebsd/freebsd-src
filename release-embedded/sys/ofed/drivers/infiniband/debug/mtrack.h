#ifndef __mtrack_h_
#define __mtrack_h_

#include <memtrack.h>

#include <linux/slab.h>
#include <linux/vmalloc.h>
#include <linux/version.h>

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,14)
#define RDMA_KZALLOC_H
#define kzalloc(size, flags)  ({ \
        void *__memtrack_kz_addr;                 \
                                \
        __memtrack_kz_addr = kmalloc(size, flags); \
        if ( __memtrack_kz_addr ) {                               \
                memset( __memtrack_kz_addr, 0, size) ; \
        }                                                                     \
        __memtrack_kz_addr;                                                                              \
})

#else
#define kzalloc(size, flags) ({ \
        void *__memtrack_addr;                 \
                                \
        __memtrack_addr = kzalloc(size, flags); \
        if ( __memtrack_addr && (size)) {                               \
                memtrack_alloc(MEMTRACK_KMALLOC, (unsigned long)(__memtrack_addr), size, __FILE__, __LINE__, flags); \
        }                                                                     \
        __memtrack_addr;                                                                              \
})

#endif

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,19)
#define kcalloc(n, size, flags) kzalloc((n)*(size), flags)
#else
#define kcalloc(n, size, flags) ({ \
        void *__memtrack_addr;                 \
                                \
        __memtrack_addr = kcalloc(n, size, flags); \
        if ( __memtrack_addr && (size)) {                               \
                memtrack_alloc(MEMTRACK_KMALLOC, (unsigned long)(__memtrack_addr), (n)*(size), __FILE__, __LINE__, flags); \
        }                                                                     \
        __memtrack_addr;                                                                              \
})
#endif



#ifdef ZERO_OR_NULL_PTR
#define kmalloc(sz, flgs) ({ \
        void *__memtrack_addr;                 \
                                \
        __memtrack_addr = kmalloc(sz, flgs); \
        if ( !ZERO_OR_NULL_PTR(__memtrack_addr)) {                               \
                memtrack_alloc(MEMTRACK_KMALLOC, (unsigned long)(__memtrack_addr), sz, __FILE__, __LINE__, flgs); \
        }                                                                     \
        __memtrack_addr;                                                                              \
})
#else
#define kmalloc(sz, flgs) ({ \
        void *__memtrack_addr;                 \
                                \
        __memtrack_addr = kmalloc(sz, flgs); \
        if ( __memtrack_addr ) {                               \
                memtrack_alloc(MEMTRACK_KMALLOC, (unsigned long)(__memtrack_addr), sz, __FILE__, __LINE__, flgs); \
        }                                                                     \
        __memtrack_addr;                                                                              \
})

#endif

#ifdef ZERO_OR_NULL_PTR
#define kfree(addr) ({ \
        void *__memtrack_addr = (void *)addr;                 \
        if ( !ZERO_OR_NULL_PTR(__memtrack_addr) ) {      \
                memtrack_free(MEMTRACK_KMALLOC, (unsigned long)(__memtrack_addr), __FILE__, __LINE__); \
        }                    \
        kfree(__memtrack_addr); \
})
#else
#define kfree(addr) ({ \
        void *__memtrack_addr = (void *)addr;                 \
        if ( __memtrack_addr ) {      \
                memtrack_free(MEMTRACK_KMALLOC, (unsigned long)(__memtrack_addr), __FILE__, __LINE__); \
        }                    \
        kfree(__memtrack_addr); \
})
#endif






#define vmalloc(size) ({ \
        void *__memtrack_addr;                 \
                                \
        __memtrack_addr = vmalloc(size); \
        if ( __memtrack_addr ) {                               \
                memtrack_alloc(MEMTRACK_VMALLOC, (unsigned long)(__memtrack_addr), size, __FILE__, __LINE__, GFP_ATOMIC); \
        }                                                                     \
        __memtrack_addr;                                                                              \
})


#define vfree(addr) ({ \
        void *__memtrack_addr = (void *)addr;   \
        if ( __memtrack_addr ) {      \
                memtrack_free(MEMTRACK_VMALLOC, (unsigned long)(__memtrack_addr), __FILE__, __LINE__); \
        }                    \
        vfree(__memtrack_addr); \
})


#define kmem_cache_alloc(cache, flags) ({ \
        void *__memtrack_addr;         \
                                \
        __memtrack_addr = kmem_cache_alloc(cache, flags); \
        if ( __memtrack_addr ) {                               \
                memtrack_alloc(MEMTRACK_KMEM_OBJ, (unsigned long)(__memtrack_addr), 1, __FILE__, __LINE__, flags); \
        }                                                                     \
        __memtrack_addr;                                                                              \
})


#define kmem_cache_free(cache, addr) ({ \
        void *__memtrack_addr = (void *)addr;                 \
        if ( __memtrack_addr ) {      \
                memtrack_free(MEMTRACK_KMEM_OBJ, (unsigned long)(__memtrack_addr), __FILE__, __LINE__); \
        }                    \
        kmem_cache_free(cache, __memtrack_addr); \
})


#endif /* __mtrack_h_ */

