#	@(#)Makefile	8.1 (Berkeley) 6/8/93
# $FreeBSD: src/usr.sbin/timed/SMM.doc/timed/Makefile,v 1.2.34.1 2009/04/15 03:14:26 kensmith Exp $

DIR=	smm/12.timed
SRCS=	timed.ms
MACROS=	-ms
PRINTER=Pdp

paper.${PRINTER}: ${SRCS}
	${SOELIM} ${SRCS} | ${TBL} | ${ROFF} > ${.TARGET}

.include <bsd.doc.mk>
