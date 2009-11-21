# $FreeBSD: src/gnu/usr.bin/texinfo/doc/Makefile,v 1.12.56.1.2.1 2009/10/25 01:10:29 kensmith Exp $

INFO=	info info-stnd texinfo
SRCDIR=	${TXIDIR}/doc

CLEANFILES=	texinfo.texi

texinfo.texi: texinfo.txi
	ln -fs ${.ALLSRC} ${.TARGET}

.include <bsd.info.mk>
