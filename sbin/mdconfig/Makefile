# $FreeBSD: src/sbin/mdconfig/Makefile,v 1.10.10.1.2.1 2009/10/25 01:10:29 kensmith Exp $

PROG=		mdconfig
MAN=		mdconfig.8
MLINKS=		mdconfig.8 vnconfig.8

DPADD=	${LIBUTIL} ${LIBGEOM} ${LIBBSDXML} ${LIBSBUF} 
LDADD=	-lutil -lgeom -lbsdxml -lsbuf

.include <bsd.prog.mk>
