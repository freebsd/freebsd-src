# $FreeBSD: src/cddl/usr.bin/Makefile,v 1.7.2.1.2.1 2008/11/25 02:59:29 kensmith Exp $

.include <bsd.own.mk>

SUBDIR= \
	ctfconvert \
	ctfdump \
	ctfmerge \
	sgsmsg \
	${_ztest}

.if ${MK_ZFS} != "no"
.if ${MK_LIBTHR} != "no" || ${MK_LIBKSE} != "no"
_ztest=	ztest
.endif
.endif

.include <bsd.subdir.mk>
