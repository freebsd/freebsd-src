# $FreeBSD: src/cddl/usr.sbin/Makefile,v 1.7 2007/10/09 23:31:09 obrien Exp $

.include <bsd.own.mk>

SUBDIR=	${_zdb}

.if ${MK_ZFS} != "no"
.if ${MK_LIBTHR} != "no" || ${MK_LIBKSE} != "no"
_zdb=	zdb
.endif
.endif

.include <bsd.subdir.mk>
