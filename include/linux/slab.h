/*
 * linux/mm/slab.h
 * Written by Mark Hemment, 1996.
 * (markhe@nextd.demon.co.uk)
 */

#if	!defined(_LINUX_SLAB_H)
#define	_LINUX_SLAB_H

#if	defined(__KERNEL__)

typedef struct kmem_cache_s kmem_cache_t;

#include	<linux/mm.h>
#include	<linux/cache.h>

/* flags for kmem_cache_alloc() */
#define	SLAB_NOFS		GFP_NOFS
#define	SLAB_NOIO		GFP_NOIO
#define SLAB_NOHIGHIO		GFP_NOHIGHIO
#define	SLAB_ATOMIC		GFP_ATOMIC
#define	SLAB_USER		GFP_USER
#define	SLAB_KERNEL		GFP_KERNEL
#define	SLAB_NFS		GFP_NFS
#define	SLAB_DMA		GFP_DMA

#define SLAB_LEVEL_MASK		(__GFP_WAIT|__GFP_HIGH|__GFP_IO|__GFP_HIGHIO|__GFP_FS)
#define	SLAB_NO_GROW		0x00001000UL	/* don't grow a cache */

/* flags to pass to kmem_cache_create().
 * The first 3 are only valid when the allocator as been build
 * SLAB_DEBUG_SUPPORT.
 */
#define	SLAB_DEBUG_FREE		0x00000100UL	/* Peform (expensive) checks on free */
#define	SLAB_DEBUG_INITIAL	0x00000200UL	/* Call constructor (as verifier) */
#define	SLAB_RED_ZONE		0x00000400UL	/* Red zone objs in a cache */
#define	SLAB_POISON		0x00000800UL	/* Poison objects */
#define	SLAB_NO_REAP		0x00001000UL	/* never reap from the cache */
#define	SLAB_HWCACHE_ALIGN	0x00002000UL	/* align objs on a h/w cache lines */
#define SLAB_CACHE_DMA		0x00004000UL	/* use GFP_DMA memory */
#define SLAB_MUST_HWCACHE_ALIGN	0x00008000UL	/* force alignment */

/* flags passed to a constructor func */
#define	SLAB_CTOR_CONSTRUCTOR	0x001UL		/* if not set, then deconstructor */
#define SLAB_CTOR_ATOMIC	0x002UL		/* tell constructor it can't sleep */
#define	SLAB_CTOR_VERIFY	0x004UL		/* tell constructor it's a verify call */

/* prototypes */
extern void kmem_cache_init(void);
extern void kmem_cache_sizes_init(void);

extern kmem_cache_t *kmem_find_general_cachep(size_t, int gfpflags);
extern kmem_cache_t *kmem_cache_create(const char *, size_t, size_t, unsigned long,
				       void (*)(void *, kmem_cache_t *, unsigned long),
				       void (*)(void *, kmem_cache_t *, unsigned long));
extern int kmem_cache_destroy(kmem_cache_t *);
extern int kmem_cache_shrink(kmem_cache_t *);
extern void *kmem_cache_alloc(kmem_cache_t *, int);
extern void kmem_cache_free(kmem_cache_t *, void *);
extern unsigned int kmem_cache_size(kmem_cache_t *);

extern void *kmalloc(size_t, int);
extern void kfree(const void *);

extern int FASTCALL(kmem_cache_reap(int));

/* System wide caches */
extern kmem_cache_t	*vm_area_cachep;
extern kmem_cache_t	*mm_cachep;
extern kmem_cache_t	*names_cachep;
extern kmem_cache_t	*files_cachep;
extern kmem_cache_t	*filp_cachep;
extern kmem_cache_t	*dquot_cachep;
extern kmem_cache_t	*bh_cachep;
extern kmem_cache_t	*fs_cachep;
extern kmem_cache_t	*sigact_cachep;

#endif	/* __KERNEL__ */

#endif	/* _LINUX_SLAB_H */
