# $FreeBSD$

.include <bsd.own.mk>

SUBDIR=	${_ztest}

.if ${MK_ZFS} != "no"
.if ${MACHINE_ARCH} == "amd64" || ${MACHINE_ARCH} == "i386"
_ztest=	ztest
.endif
.endif

.include <bsd.subdir.mk>
