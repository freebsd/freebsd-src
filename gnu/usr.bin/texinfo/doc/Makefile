# $FreeBSD: src/gnu/usr.bin/texinfo/doc/Makefile,v 1.12.54.1 2009/04/15 03:14:26 kensmith Exp $

INFO=	info info-stnd texinfo
SRCDIR=	${TXIDIR}/doc

CLEANFILES=	texinfo.texi

texinfo.texi: texinfo.txi
	ln -fs ${.ALLSRC} ${.TARGET}

.include <bsd.info.mk>
