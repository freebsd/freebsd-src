# $FreeBSD$

SUBDIR_ALL+=	include
SUBDIR_ALL+=	profile
SUBDIR_ALL+=	stats
SUBDIR_ALL+=	stats_client
SUBDIR_ALL+=	ubsan_minimal
SUBDIR_ALL+=	ubsan_standalone
SUBDIR_ALL+=	ubsan_standalone_cxx

.if ${MACHINE_CPUARCH} == "aarch64" || ${MACHINE_CPUARCH} == "amd64" || \
    ${MACHINE_CPUARCH} == "arm" || ${MACHINE_CPUARCH} == "i386"
SUBDIR_ALL+=	asan
SUBDIR_ALL+=	asan-preinit
SUBDIR_ALL+=	asan_cxx
SUBDIR_ALL+=	asan_dynamic
SUBDIR_ALL+=	cfi
SUBDIR_ALL+=	cfi_diag
.endif # aarch64 || amd64 || arm || i386

.if ${MACHINE_CPUARCH} == "aarch64" || ${MACHINE_CPUARCH} == "amd64"
SUBDIR_ALL+=	dd
SUBDIR_ALL+=	fuzzer
SUBDIR_ALL+=	fuzzer_no_main
SUBDIR_ALL+=	msan
SUBDIR_ALL+=	msan_cxx
SUBDIR_ALL+=	tsan
SUBDIR_ALL+=	tsan_cxx
.endif # aarch64 || amd64

.if ${MACHINE_CPUARCH} == "aarch64" || ${MACHINE_CPUARCH} == "amd64" || \
    ${MACHINE_CPUARCH} == "i386"
SUBDIR_ALL+=	safestack
.endif # aarch64 || amd64 || i386

.if ${MACHINE_CPUARCH} == "aarch64" || ${MACHINE_CPUARCH} == "amd64" || \
    ${MACHINE_CPUARCH} == "arm"
SUBDIR_ALL+=	xray
SUBDIR_ALL+=	xray-basic
SUBDIR_ALL+=	xray-fdr
SUBDIR_ALL+=	xray-profiling
.endif # aarch64 || amd64 || arm

SUBDIR+=	${SUBDIR_ALL:O}

.include <bsd.subdir.mk>
# DO NOT DELETE
