# $FreeBSD$

.include <bsd.own.mk>

SUBDIR=	${_zdb}

.if ${MK_ZFS} != "no"
.if ${MK_LIBTHR} != "no" || ${MK_LIBPTHREAD} != "no"
_zdb=	zdb
.endif
.endif

.include <bsd.subdir.mk>
