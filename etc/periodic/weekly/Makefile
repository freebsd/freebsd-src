# $FreeBSD: src/etc/periodic/weekly/Makefile,v 1.4.2.3 2002/07/18 12:36:07 ru Exp $

FILES=	120.clean-kvmdb \
	310.locate \
	320.whatis \
	330.catman \
	340.noid \
	400.status-pkg \
	999.local
.if !defined(NOUUCP)
FILES+=	300.uucp
.endif

.include <bsd.prog.mk>
