# $FreeBSD$

.include <bsd.own.mk>

SUBDIR=
.if ${MK_ZFS} != "no"
SUBDIR+=${_zdb}
.endif

.if ${MACHINE_ARCH} == "i386" || ${MACHINE_ARCH} == "amd64"
_zdb=	zdb
.endif

.include <bsd.subdir.mk>
