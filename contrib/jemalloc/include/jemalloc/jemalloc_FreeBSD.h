/*
 * Override settings that were generated in jemalloc_defs.h as necessary.
 */

#undef JEMALLOC_OVERRIDE_VALLOC

#ifndef MALLOC_PRODUCTION
#define	JEMALLOC_DEBUG
#endif

/*
 * The following are architecture-dependent, so conditionally define them for
 * each supported architecture.
 */
#undef CPU_SPINWAIT
#undef JEMALLOC_TLS_MODEL
#undef STATIC_PAGE_SHIFT
#undef LG_SIZEOF_PTR
#undef LG_SIZEOF_INT
#undef LG_SIZEOF_LONG
#undef LG_SIZEOF_INTMAX_T

#ifdef __i386__
#  define LG_SIZEOF_PTR		2
#  define CPU_SPINWAIT		__asm__ volatile("pause")
#  define JEMALLOC_TLS_MODEL	__attribute__((tls_model("initial-exec")))
#endif
#ifdef __ia64__
#  define LG_SIZEOF_PTR		3
#endif
#ifdef __sparc64__
#  define LG_SIZEOF_PTR		3
#  define JEMALLOC_TLS_MODEL	__attribute__((tls_model("initial-exec")))
#endif
#ifdef __amd64__
#  define LG_SIZEOF_PTR		3
#  define CPU_SPINWAIT		__asm__ volatile("pause")
#  define JEMALLOC_TLS_MODEL	__attribute__((tls_model("initial-exec")))
#endif
#ifdef __arm__
#  define LG_SIZEOF_PTR		2
#endif
#ifdef __mips__
#ifdef __mips_n64
#  define LG_SIZEOF_PTR		3
#else
#  define LG_SIZEOF_PTR		2
#endif
#endif
#ifdef __powerpc64__
#  define LG_SIZEOF_PTR		3
#elif defined(__powerpc__)
#  define LG_SIZEOF_PTR		2
#endif

#ifndef JEMALLOC_TLS_MODEL
#  define JEMALLOC_TLS_MODEL	/* Default. */
#endif

#define	STATIC_PAGE_SHIFT	PAGE_SHIFT
#define	LG_SIZEOF_INT		2
#define	LG_SIZEOF_LONG		LG_SIZEOF_PTR
#define	LG_SIZEOF_INTMAX_T	3

/* Disable lazy-lock machinery, mangle isthreaded, and adjust its type. */
#undef JEMALLOC_LAZY_LOCK
extern int __isthreaded;
#define	isthreaded		((bool)__isthreaded)

/* Mangle. */
#undef je_malloc
#undef je_calloc
#undef je_realloc
#undef je_free
#undef je_posix_memalign
#undef je_malloc_usable_size
#undef je_allocm
#undef je_rallocm
#undef je_sallocm
#undef je_dallocm
#undef je_nallocm
#define	je_malloc		__malloc
#define	je_calloc		__calloc
#define	je_realloc		__realloc
#define	je_free			__free
#define	je_posix_memalign	__posix_memalign
#define	je_malloc_usable_size	__malloc_usable_size
#define	je_allocm		__allocm
#define	je_rallocm		__rallocm
#define	je_sallocm		__sallocm
#define	je_dallocm		__dallocm
#define	je_nallocm		__nallocm
#define	open			_open
#define	read			_read
#define	write			_write
#define	close			_close
#define	pthread_mutex_lock	_pthread_mutex_lock
#define	pthread_mutex_unlock	_pthread_mutex_unlock

#ifdef JEMALLOC_C_
/*
 * Define 'weak' symbols so that an application can have its own versions
 * of malloc, calloc, realloc, free, et al.
 */
__weak_reference(__malloc, malloc);
__weak_reference(__calloc, calloc);
__weak_reference(__realloc, realloc);
__weak_reference(__free, free);
__weak_reference(__posix_memalign, posix_memalign);
__weak_reference(__malloc_usable_size, malloc_usable_size);
__weak_reference(__allocm, allocm);
__weak_reference(__rallocm, rallocm);
__weak_reference(__sallocm, sallocm);
__weak_reference(__dallocm, dallocm);
__weak_reference(__nallocm, nallocm);
#endif

