# $FreeBSD: src/cddl/usr.bin/Makefile,v 1.7.2.2.2.1 2010/02/10 00:26:20 kensmith Exp $

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
