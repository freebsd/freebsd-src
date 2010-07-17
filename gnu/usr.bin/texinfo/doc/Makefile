# $FreeBSD: src/gnu/usr.bin/texinfo/doc/Makefile,v 1.12.56.1.4.1 2010/06/14 02:09:06 kensmith Exp $

INFO=	info info-stnd texinfo
SRCDIR=	${TXIDIR}/doc

CLEANFILES=	texinfo.texi

texinfo.texi: texinfo.txi
	ln -fs ${.ALLSRC} ${.TARGET}

.include <bsd.info.mk>
