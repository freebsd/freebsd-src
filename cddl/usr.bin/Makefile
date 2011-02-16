# $FreeBSD: src/cddl/usr.bin/Makefile,v 1.7.2.2.4.1 2010/12/21 17:10:29 kensmith Exp $

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
