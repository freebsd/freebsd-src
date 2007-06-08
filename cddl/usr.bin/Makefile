# $FreeBSD$

.include <bsd.own.mk>

SUBDIR=	${_ztest}

.if ${MK_ZFS} != "no"
_ztest=	ztest
.endif

.include <bsd.subdir.mk>
