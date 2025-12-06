.PATH: ${LIBC_SRCTOP}/stdlib/malloc/jemalloc

JEMALLOCSRCS:= jemalloc.c arena.c background_thread.c base.c bin.c bitmap.c \
	ckh.c ctl.c div.c extent.c extent_dss.c extent_mmap.c hook.c \
	large.c log.c malloc_io.c mutex.c nstime.c pages.c \
	prof.c rtree.c safety_check.c sc.c stats.c sz.c tcache.c \
	test_hooks.c ticker.c tsd.c witness.c \
	bin_info.c san.c san_bump.c counter.c prof_data.c prof_log.c prof_recent.c prof_stats.c prof_sys.c \
	emap.c edata.c edata_cache.c pa.c pa_extra.c pac.c decay.c hpa.c hpa_hooks.c fxp.c hpdata.c pai.c \
	ecache.c ehooks.c eset.c sec.c cache_bin.c peak_event.c psset.c inspect.c exp_grow.c thread_event.c \
	buf_writer.c

CFLAGS+=-I${SRCTOP}/contrib/jemalloc/include -I${LIBC_SRCTOP}/stdlib/malloc/jemalloc/include
.if ${MK_JEMALLOC_LG_VADDR_WIDE} != no
CFLAGS+=-D_USE_LG_VADDR_WIDE
.endif
CFLAGS.gcc+=-Wno-error=missing-braces


.for src in ${JEMALLOCSRCS}
MISRCS+=jemalloc_${src}
CLEANFILES+=jemalloc_${src}
jemalloc_${src}: ${SRCTOP}/contrib/jemalloc/src/${src} .NOMETA
	ln -sf ${.ALLSRC} ${.TARGET}
.endfor

MAN+=	jemalloc.3

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

.if ${MK_MALLOC_PRODUCTION} != "no" || defined(MALLOC_PRODUCTION)
CFLAGS+=	-DMALLOC_PRODUCTION
.endif
