
SD_ASAN+=	asan
SD_ASAN+=	asan-preinit
SD_ASAN+=	asan_cxx
SD_ASAN+=	asan_dynamic
SD_ASAN+=	asan_static

SD_CFI+=	cfi
SD_CFI+= 	cfi_diag

SD_DD+=		dd

SD_FUZZER+=	fuzzer
SD_FUZZER+=	fuzzer_interceptors
SD_FUZZER+=	fuzzer_no_main

SD_INCLUDE+=	include

SD_MSAN+=	msan
SD_MSAN+=	msan_cxx

SD_PROFILE+=	profile

SD_SAFESTACK+=	safestack

SD_STATS+=	stats
SD_STATS+=	stats_client

SD_TSAN+=	tsan
SD_TSAN+=	tsan_cxx

SD_UBSAN+=	ubsan_minimal
SD_UBSAN+=	ubsan_standalone
SD_UBSAN+=	ubsan_standalone_cxx

SD_XRAY+=	xray
SD_XRAY+=	xray-basic
SD_XRAY+=	xray-fdr
SD_XRAY+=	xray-profiling

.if ${MACHINE_CPUARCH} == "aarch64"
SUBDIR+=	${SD_ASAN}
SUBDIR+=	${SD_CFI}
SUBDIR+=	${SD_FUZZER}
SUBDIR+=	${SD_MSAN}
SUBDIR+=	${SD_SAFESTACK}
SUBDIR+=	${SD_STATS}
SUBDIR+=	${SD_TSAN}
SUBDIR+=	${SD_UBSAN}
SUBDIR+=	${SD_XRAY}
.elif ${MACHINE_CPUARCH} == "amd64"
SUBDIR+=	${SD_ASAN}
SUBDIR+=	${SD_CFI}
SUBDIR+=	${SD_DD}
SUBDIR+=	${SD_FUZZER}
SUBDIR+=	${SD_MSAN}
SUBDIR+=	${SD_SAFESTACK}
SUBDIR+=	${SD_STATS}
SUBDIR+=	${SD_TSAN}
SUBDIR+=	${SD_UBSAN}
SUBDIR+=	${SD_XRAY}
.elif ${MACHINE_CPUARCH} == "arm"
# nothing for 32-bit arm
.elif ${MACHINE_CPUARCH} == "i386"
SUBDIR+=	${SD_ASAN}
SUBDIR+=	${SD_CFI}
SUBDIR+=	${SD_SAFESTACK}
SUBDIR+=	${SD_STATS}
SUBDIR+=	${SD_UBSAN}
.elif ${MACHINE_ARCH} == "powerpc"
# nothing for 32-bit powerpc
.elif ${MACHINE_ARCH} == "powerpc64"
SUBDIR+=	${SD_ASAN}
SUBDIR+=	${SD_MSAN}
SUBDIR+=	${SD_STATS}
SUBDIR+=	${SD_TSAN}
SUBDIR+=	${SD_UBSAN}
.elif ${MACHINE_ARCH} == "powerpc64le"
SUBDIR+=	${SD_ASAN}
SUBDIR+=	${SD_MSAN}
SUBDIR+=	${SD_STATS}
SUBDIR+=	${SD_TSAN}
SUBDIR+=	${SD_UBSAN}
SUBDIR+=	${SD_XRAY}
.elif ${MACHINE_CPUARCH} == "riscv"
SUBDIR+=	${SD_ASAN}
SUBDIR+=	${SD_STATS}
SUBDIR+=	${SD_UBSAN}
.endif

SUBDIR+=	${SD_INCLUDE}
SUBDIR+=	${SD_PROFILE}

SUBDIR_PARALLEL=

.include <bsd.subdir.mk>
