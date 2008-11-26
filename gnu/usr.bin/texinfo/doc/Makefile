# $FreeBSD: src/gnu/usr.bin/texinfo/doc/Makefile,v 1.12.50.1 2008/10/02 02:57:24 kensmith Exp $

INFO=	info info-stnd texinfo
SRCDIR=	${TXIDIR}/doc

CLEANFILES=	texinfo.texi

texinfo.texi: texinfo.txi
	ln -fs ${.ALLSRC} ${.TARGET}

.include <bsd.info.mk>
