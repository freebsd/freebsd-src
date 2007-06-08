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
_libzpool=	libzpool
.endif

.include <bsd.subdir.mk>
