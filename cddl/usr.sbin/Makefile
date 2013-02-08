# $FreeBSD$

.include <bsd.own.mk>

SUBDIR=	${_dtrace} \
	${_dtruss} \
	${_lockstat} \
	${_zdb} \
	${_zhack}

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
.endif

.if ${MACHINE_CPUARCH} == "mips"
_dtrace=	dtrace
.endif

.if ${MACHINE_CPUARCH} == "powerpc"
_dtrace=	dtrace
_dtruss=	dtruss
_lockstat=	lockstat
.endif

.include <bsd.subdir.mk>
