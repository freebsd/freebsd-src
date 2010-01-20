# $FreeBSD$

.include <bsd.own.mk>

SUBDIR=	dtrace \
	${_zdb}

.if ${MK_ZFS} != "no"
.if ${MK_LIBTHR} != "no"
_zdb=	zdb
.endif
.endif

.include <bsd.subdir.mk>
