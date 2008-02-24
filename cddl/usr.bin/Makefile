# $FreeBSD: src/cddl/usr.bin/Makefile,v 1.7 2007/10/09 23:31:09 obrien Exp $

.include <bsd.own.mk>

SUBDIR=	${_ztest}

.if ${MK_ZFS} != "no"
.if ${MK_LIBTHR} != "no" || ${MK_LIBKSE} != "no"
_ztest=	ztest
.endif
.endif

.include <bsd.subdir.mk>
