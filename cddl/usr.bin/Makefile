# $FreeBSD$

.include <bsd.own.mk>

.if ${MK_ZFS} != "no"
SUBDIR=	${_ztest}
.endif

.if ${MACHINE_ARCH} == "i386" || ${MACHINE_ARCH} == "amd64"
_ztest=	ztest
.endif

.include <bsd.subdir.mk>
