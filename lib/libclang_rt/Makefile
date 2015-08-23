# $FreeBSD$

.include <src.opts.mk>

.if ${MACHINE_CPUARCH} == "i386" || ${MACHINE_CPUARCH} == "amd64"
SUBDIR+= asan\
	 asan_cxx\
	 san\
	 ubsan\
	 ubsan_cxx
.endif

.if ${MACHINE_CPUARCH} == "i386" || ${MACHINE_CPUARCH} == "amd64" || \
    (${MACHINE_CPUARCH} == "arm" && ${MACHINE_ARCH} != "armeb")
SUBDIR+= profile
.endif

.include <bsd.subdir.mk>
