# $FreeBSD: src/gnu/usr.bin/texinfo/doc/Makefile,v 1.10.2.1 1999/08/29 14:35:24 peter Exp $

INFO=	info info-stnd texinfo

CLEANFILES=	texinfo.texi

texinfo.texi: texinfo.txi
	ln -fs ${.ALLSRC} ${.TARGET}

.include <bsd.info.mk>

.PATH: ${TXIDIR}/doc
