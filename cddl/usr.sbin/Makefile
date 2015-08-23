# $FreeBSD$

.include <src.opts.mk>

SUBDIR=	${_dtrace} \
	${_dtruss} \
	${_lockstat} \
	${_plockstat} \
	${_tests} \
	${_zdb} \
	${_zhack}

.if ${MK_TESTS} != "no"
_tests=	tests
.endif

.if ${MK_ZFS} != "no"
.if ${MK_LIBTHR} != "no"
_zdb=	zdb
_zhack=	zhack
.endif
.endif

.if ${MACHINE_ARCH} == "amd64" || ${MACHINE_ARCH} == "i386"
_dtrace=	dtrace
_dtruss=	dtruss
_lockstat=	lockstat
.if defined(WITH_PLOCKSTAT)
_plockstat=	plockstat
.endif
.endif

.if ${MACHINE_CPUARCH} == "aarch64" || ${MACHINE_CPUARCH} == "arm"
_dtrace=	dtrace
_dtruss=	dtruss
_lockstat=	lockstat
.endif

.if ${MACHINE_CPUARCH} == "mips"
_dtrace=	dtrace
.endif

.if ${MACHINE_CPUARCH} == "powerpc"
_dtrace=	dtrace
_dtruss=	dtruss
_lockstat=	lockstat
.endif

SUBDIR_PARALLEL=

.include <bsd.subdir.mk>
