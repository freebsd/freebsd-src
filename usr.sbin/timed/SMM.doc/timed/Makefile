#	@(#)Makefile	8.1 (Berkeley) 6/8/93
# $FreeBSD: src/usr.sbin/timed/SMM.doc/timed/Makefile,v 1.2.32.1 2008/11/25 02:59:29 kensmith Exp $

DIR=	smm/12.timed
SRCS=	timed.ms
MACROS=	-ms
PRINTER=Pdp

paper.${PRINTER}: ${SRCS}
	${SOELIM} ${SRCS} | ${TBL} | ${ROFF} > ${.TARGET}

.include <bsd.doc.mk>
