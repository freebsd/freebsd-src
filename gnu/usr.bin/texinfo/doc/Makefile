# $FreeBSD: src/gnu/usr.bin/texinfo/doc/Makefile,v 1.12.52.1 2008/11/25 02:59:29 kensmith Exp $

INFO=	info info-stnd texinfo
SRCDIR=	${TXIDIR}/doc

CLEANFILES=	texinfo.texi

texinfo.texi: texinfo.txi
	ln -fs ${.ALLSRC} ${.TARGET}

.include <bsd.info.mk>
