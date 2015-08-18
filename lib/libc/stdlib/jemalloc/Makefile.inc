# $FreeBSD$

.PATH: ${LIBC_SRCTOP}/stdlib/jemalloc

JEMALLOCSRCS:= jemalloc.c arena.c atomic.c base.c bitmap.c chunk.c \
	chunk_dss.c chunk_mmap.c ckh.c ctl.c extent.c hash.c huge.c mb.c \
	mutex.c pages.c prof.c quarantine.c rtree.c stats.c tcache.c tsd.c \
	util.c

SYM_MAPS+=${LIBC_SRCTOP}/stdlib/jemalloc/Symbol.map

CFLAGS+=-I${LIBC_SRCTOP}/../../contrib/jemalloc/include

.for src in ${JEMALLOCSRCS}
MISRCS+=jemalloc_${src}
CLEANFILES+=jemalloc_${src}
jemalloc_${src}:
	ln -sf ${LIBC_SRCTOP}/../../contrib/jemalloc/src/${src} ${.TARGET}
.endfor

MAN+=jemalloc.3
CLEANFILES+=jemalloc.3
jemalloc.3:
	ln -sf ${LIBC_SRCTOP}/../../contrib/jemalloc/doc/jemalloc.3 ${.TARGET}

MLINKS+= \
	jemalloc.3 malloc.3 \
	jemalloc.3 calloc.3 \
	jemalloc.3 posix_memalign.3 \
	jemalloc.3 aligned_alloc.3 \
	jemalloc.3 realloc.3 \
	jemalloc.3 free.3 \
	jemalloc.3 malloc_usable_size.3 \
	jemalloc.3 malloc_stats_print.3 \
	jemalloc.3 mallctl.3 \
	jemalloc.3 mallctlnametomib.3 \
	jemalloc.3 mallctlbymib.3 \
	jemalloc.3 mallocx.3 \
	jemalloc.3 rallocx.3 \
	jemalloc.3 xallocx.3 \
	jemalloc.3 sallocx.3 \
	jemalloc.3 dallocx.3 \
	jemalloc.3 sdallocx.3 \
	jemalloc.3 nallocx.3 \
	jemalloc.3 malloc.conf.5

.if defined(MALLOC_PRODUCTION)
CFLAGS+=	-DMALLOC_PRODUCTION
.endif
