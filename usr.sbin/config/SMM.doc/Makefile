#	@(#)Makefile	8.1 (Berkeley) 6/8/93
# $FreeBSD: src/usr.sbin/config/SMM.doc/Makefile,v 1.2 2001/07/20 06:19:39 obrien Exp $

DIR=	smm/02.config
SRCS=	0.t 1.t 2.t 3.t 4.t 5.t 6.t a.t b.t c.t d.t e.t
MACROS=	-ms

paper.ps: ${SRCS}
	${TBL} ${SRCS} | ${ROFF} > ${.TARGET}

.include <bsd.doc.mk>
