# $FreeBSD: src/gnu/usr.bin/texinfo/doc/Makefile,v 1.10.2.2 2000/05/06 18:53:58 ru Exp $

INFO=	info info-stnd texinfo
SRCDIR=	${TXIDIR}/doc

CLEANFILES=	texinfo.texi

texinfo.texi: texinfo.txi
	ln -fs ${.ALLSRC} ${.TARGET}

.include <bsd.info.mk>
