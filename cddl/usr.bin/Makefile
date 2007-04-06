# $FreeBSD$

.include <bsd.own.mk>

.if ${MK_ZFS} != "no"
SUBDIR=	${_ztest}
.endif

.if ${MACHINE_ARCH} == "i386"
.if ${MACHINE} == "i386"
_ztest=	ztest
.endif
.endif

.include <bsd.subdir.mk>
