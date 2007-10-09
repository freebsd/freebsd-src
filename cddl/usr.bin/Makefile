# $FreeBSD$

.include <bsd.own.mk>

SUBDIR=	${_ztest}

.if ${MK_ZFS} != "no"
.if ${MK_LIBTHR} != "no" || ${MK_LIBKSE} != "no"
_ztest=	ztest
.endif
.endif

.include <bsd.subdir.mk>
