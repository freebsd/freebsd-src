# $FreeBSD: src/etc/periodic/weekly/Makefile,v 1.4.2.2 2000/10/31 09:44:13 dougb Exp $

BIN=	120.clean-kvmdb \
	310.locate \
	320.whatis \
	330.catman \
	340.noid \
	400.status-pkg \
	999.local
.if !defined(NOUUCP)
BIN+=	300.uucp
.endif

.include <bsd.prog.mk>
