# $FreeBSD: src/gnu/usr.bin/texinfo/doc/Makefile,v 1.12.56.1.8.1 2012/03/03 06:15:13 kensmith Exp $

INFO=	info info-stnd texinfo
SRCDIR=	${TXIDIR}/doc

CLEANFILES=	texinfo.texi

texinfo.texi: texinfo.txi
	ln -fs ${.ALLSRC} ${.TARGET}

.include <bsd.info.mk>
