# $FreeBSD$

.include <bsd.own.mk>

SUBDIR=
.if ${MK_ZFS} != "no"
SUBDIR+=${_zdb}
.endif

.if ${MACHINE_ARCH} == "i386"
.if ${MACHINE} == "i386"
_zdb=	zdb
.endif
.endif

.include <bsd.subdir.mk>
