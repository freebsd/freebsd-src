# $FreeBSD: src/cddl/lib/Makefile,v 1.7.2.2.2.1 2010/02/10 00:26:20 kensmith Exp $

.include <bsd.own.mk>

SUBDIR=	drti \
	libavl \
	libctf \
	libdtrace \
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

.include <bsd.subdir.mk>
