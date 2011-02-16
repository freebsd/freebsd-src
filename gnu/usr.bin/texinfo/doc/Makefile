# $FreeBSD: src/gnu/usr.bin/texinfo/doc/Makefile,v 1.12.56.1.6.1 2010/12/21 17:09:25 kensmith Exp $

INFO=	info info-stnd texinfo
SRCDIR=	${TXIDIR}/doc

CLEANFILES=	texinfo.texi

texinfo.texi: texinfo.txi
	ln -fs ${.ALLSRC} ${.TARGET}

.include <bsd.info.mk>
