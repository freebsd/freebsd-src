# $FreeBSD$

.include <bsd.own.mk>

SUBDIR=	${_zdb}

.if ${MK_ZFS} != "no"
.if ${MACHINE_ARCH} == "amd64" || ${MACHINE_ARCH} == "i386"
_zdb=	zdb
.endif
.endif

.include <bsd.subdir.mk>
