#	$Id: Makefile,v 1.9 1999/01/14 20:00:43 markm Exp $

INFO=	info info-stnd texinfo

CLEANFILES=	texinfo.texi

texinfo.texi: texinfo.txi
	ln -fs ${.ALLSRC} ${.TARGET}

.include <bsd.info.mk>

.PATH: ${TXIDIR}/doc
