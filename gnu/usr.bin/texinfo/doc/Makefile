# $FreeBSD$

INFO=	info info-stnd texinfo

CLEANFILES=	texinfo.texi

texinfo.texi: texinfo.txi
	ln -fs ${.ALLSRC} ${.TARGET}

.include <bsd.info.mk>

.PATH: ${TXIDIR}/doc
