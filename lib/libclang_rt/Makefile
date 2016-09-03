# $FreeBSD$

.include <src.opts.mk>

.if ${MACHINE_CPUARCH} == "i386" || ${MACHINE_CPUARCH} == "amd64"
SUBDIR+=	include
SUBDIR+=	asan
SUBDIR+=	asan-preinit
SUBDIR+=	asan_dynamic
SUBDIR+=	asan_cxx
SUBDIR+=	safestack
SUBDIR+=	stats
SUBDIR+=	stats_client
SUBDIR+=	ubsan_standalone
SUBDIR+=	ubsan_standalone_cxx
.endif

.if ${MACHINE_CPUARCH} == "i386" || ${MACHINE_CPUARCH} == "amd64" || \
    (${MACHINE_CPUARCH} == "arm" && ${MACHINE_ARCH} != "armeb")
SUBDIR+=	profile
.endif

.include <bsd.subdir.mk>
