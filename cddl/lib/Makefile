# $FreeBSD: src/cddl/lib/Makefile,v 1.9.2.2.4.1 2012/03/03 06:15:13 kensmith Exp $

.include <bsd.own.mk>

SUBDIR=	${_drti} \
	libavl \
	libctf \
	${_libdtrace} \
	libnvpair \
	libumem \
	libuutil \
	${_libzfs} \
	${_libzpool}

.if ${MK_ZFS} != "no"
_libzfs=	libzfs
.if ${MK_LIBTHR} != "no"
_libzpool=	libzpool
.endif
.endif

.if ${MACHINE_ARCH} == "amd64" || ${MACHINE_ARCH} == "i386"
_drti=		drti
_libdtrace=	libdtrace
.endif

.include <bsd.subdir.mk>
