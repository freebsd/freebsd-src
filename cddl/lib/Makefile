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

.if ${MACHINE_ARCH} == "i386"
.if ${MACHINE} == "i386"
_libzpool=	libzpool
.endif
.endif

.include <bsd.subdir.mk>
