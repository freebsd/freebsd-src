# $FreeBSD$

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
