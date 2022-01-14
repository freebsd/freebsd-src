# $FreeBSD$

.if ${MACHINE_CPUARCH} == "amd64" || ${MACHINE_CPUARCH} == "i386"
SUBDIR+=	include
SUBDIR+=	asan
SUBDIR+=	asan-preinit
SUBDIR+=	asan_cxx
SUBDIR+=	asan_dynamic
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

.if ${MACHINE_CPUARCH} == "riscv"
SUBDIR+=	asan
SUBDIR+=	asan-preinit
SUBDIR+=	asan_cxx
SUBDIR+=	asan_dynamic
SUBDIR+=	ubsan_minimal
SUBDIR+=	ubsan_standalone
SUBDIR+=	ubsan_standalone_cxx
.endif # riscv

SUBDIR+=	profile

SUBDIR_PARALLEL=

.include <bsd.subdir.mk>
