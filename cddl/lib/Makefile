# $FreeBSD: src/cddl/lib/Makefile,v 1.7.2.1.4.1 2009/04/15 03:14:26 kensmith Exp $

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
.if ${MK_LIBTHR} != "no" || ${MK_LIBKSE} != "no"
_libzpool=	libzpool
.endif
.endif

.include <bsd.subdir.mk>
