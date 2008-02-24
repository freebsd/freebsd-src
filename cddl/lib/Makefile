# $FreeBSD: src/cddl/lib/Makefile,v 1.7 2007/10/09 23:31:09 obrien Exp $

.include <bsd.own.mk>

SUBDIR=	libavl \
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
