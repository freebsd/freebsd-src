# $FreeBSD: src/cddl/usr.bin/Makefile,v 1.10.2.1.2.1 2009/10/25 01:10:29 kensmith Exp $

.include <bsd.own.mk>

SUBDIR= \
	ctfconvert \
	ctfdump \
	ctfmerge \
	sgsmsg \
	${_zinject} \
	${_ztest}

.if ${MK_ZFS} != "no"
_zinject= zinject
.if ${MK_LIBTHR} != "no"
_ztest=	ztest
.endif
.endif

.include <bsd.subdir.mk>
