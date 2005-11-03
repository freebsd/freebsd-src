# $FreeBSD: src/gnu/usr.bin/texinfo/doc/Makefile,v 1.12 2000/01/17 10:59:24 ru Exp $

INFO=	info info-stnd texinfo
SRCDIR=	${TXIDIR}/doc

CLEANFILES=	texinfo.texi

texinfo.texi: texinfo.txi
	ln -fs ${.ALLSRC} ${.TARGET}

.include <bsd.info.mk>
