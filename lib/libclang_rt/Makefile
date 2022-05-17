# $FreeBSD$

.if ${MACHINE_CPUARCH} == "amd64" || ${MACHINE_CPUARCH} == "i386"
SUBDIR+=	include
SUBDIR+=	asan
SUBDIR+=	asan-preinit
SUBDIR+=	asan_cxx
SUBDIR+=	asan_dynamic
SUBDIR+=	asan_static
SUBDIR+=	cfi
SUBDIR+=	cfi_diag
SUBDIR+=	safestack
SUBDIR+=	stats
SUBDIR+=	stats_client
SUBDIR+=	ubsan_minimal
SUBDIR+=	ubsan_standalone
SUBDIR+=	ubsan_standalone_cxx
.endif # amd64 || i386

.if ${MACHINE_CPUARCH} == "amd64"
SUBDIR+=	dd
SUBDIR+=	fuzzer
SUBDIR+=	fuzzer_interceptors
SUBDIR+=	fuzzer_no_main
SUBDIR+=	msan
SUBDIR+=	msan_cxx
SUBDIR+=	tsan
SUBDIR+=	tsan_cxx
SUBDIR+=	xray
SUBDIR+=	xray-basic
SUBDIR+=	xray-fdr
SUBDIR+=	xray-profiling
.endif # amd64

.if ${MACHINE_ARCH} == "powerpc64" || ${MACHINE_ARCH} == "powerpc64le"
SUBDIR+=	include
SUBDIR+=	asan
SUBDIR+=	asan-preinit
SUBDIR+=	asan_cxx
SUBDIR+=	asan_dynamic
SUBDIR+=	asan_static
SUBDIR+=	msan
SUBDIR+=	msan_cxx
SUBDIR+=	stats
SUBDIR+=	stats_client
SUBDIR+=	tsan
SUBDIR+=	tsan_cxx
SUBDIR+=	ubsan_minimal
SUBDIR+=	ubsan_standalone
SUBDIR+=	ubsan_standalone_cxx
.endif # powerpc64 || powerpc64le

.if ${MACHINE_ARCH} == "powerpc64le"
SUBDIR+=	xray
SUBDIR+=	xray-basic
SUBDIR+=	xray-fdr
SUBDIR+=	xray-profiling
.endif # powerpc64le

.if ${MACHINE_CPUARCH} == "riscv"
SUBDIR+=	include
SUBDIR+=	asan
SUBDIR+=	asan-preinit
SUBDIR+=	asan_cxx
SUBDIR+=	asan_dynamic
SUBDIR+=	stats
SUBDIR+=	stats_client
SUBDIR+=	ubsan_minimal
SUBDIR+=	ubsan_standalone
SUBDIR+=	ubsan_standalone_cxx
.endif # riscv

SUBDIR+=	profile

SUBDIR_PARALLEL=

.include <bsd.subdir.mk>
