#	$Id: Makefile,v 1.10 1999/01/19 23:36:05 bde Exp $

INFO=	info info-stnd texinfo

CLEANFILES=	texinfo.texi

texinfo.texi: texinfo.txi
	ln -fs ${.ALLSRC} ${.TARGET}

.include <bsd.info.mk>

.PATH: ${TXIDIR}/doc
