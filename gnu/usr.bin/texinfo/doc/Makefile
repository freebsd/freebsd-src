# $FreeBSD: src/gnu/usr.bin/texinfo/doc/Makefile,v 1.12.58.1 2010/02/10 00:26:20 kensmith Exp $

INFO=	info info-stnd texinfo
SRCDIR=	${TXIDIR}/doc

CLEANFILES=	texinfo.texi

texinfo.texi: texinfo.txi
	ln -fs ${.ALLSRC} ${.TARGET}

.include <bsd.info.mk>
