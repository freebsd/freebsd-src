# $FreeBSD$

.include <bsd.own.mk>

SUBDIR=	libavl
SUBDIR+=libnvpair
SUBDIR+=libumem
SUBDIR+=libuutil
.if ${MK_CDDL} != "no"
SUBDIR+=libzfs
SUBDIR+=${_libzpool}
.endif

.if ${MACHINE_ARCH} == "i386" || ${MACHINE_ARCH} == "amd64"
_libzpool=	libzpool
.endif

.include <bsd.subdir.mk>
