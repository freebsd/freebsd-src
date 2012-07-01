# $FreeBSD$

.PATH: ${.CURDIR}/stdlib/jemalloc

JEMALLOCSRCS:= jemalloc.c arena.c atomic.c base.c bitmap.c chunk.c \
	chunk_dss.c chunk_mmap.c ckh.c ctl.c extent.c hash.c huge.c mb.c \
	mutex.c prof.c quarantine.c rtree.c stats.c tcache.c util.c tsd.c

SYM_MAPS+=${.CURDIR}/stdlib/jemalloc/Symbol.map

CFLAGS+=-I${.CURDIR}/../../contrib/jemalloc/include

.for src in ${JEMALLOCSRCS}
MISRCS+=jemalloc_${src}
CLEANFILES+=jemalloc_${src}
jemalloc_${src}:
	ln -sf ${.CURDIR}/../../contrib/jemalloc/src/${src} ${.TARGET}
.endfor

MAN+=jemalloc.3
CLEANFILES+=jemalloc.3
jemalloc.3:
	ln -sf ${.CURDIR}/../../contrib/jemalloc/doc/jemalloc.3 ${.TARGET}

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
	jemalloc.3 allocm.3 \
	jemalloc.3 rallocm.3 \
	jemalloc.3 sallocm.3 \
	jemalloc.3 dallocm.3 \
	jemalloc.3 nallocm.3 \
	jemalloc.3 malloc.conf.5

.if defined(MALLOC_PRODUCTION)
CFLAGS+=	-DMALLOC_PRODUCTION
.endif
