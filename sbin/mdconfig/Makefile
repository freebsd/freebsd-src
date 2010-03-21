# $FreeBSD: src/sbin/mdconfig/Makefile,v 1.10.12.1 2010/02/10 00:26:20 kensmith Exp $

PROG=		mdconfig
MAN=		mdconfig.8
MLINKS=		mdconfig.8 vnconfig.8

DPADD=	${LIBUTIL} ${LIBGEOM} ${LIBBSDXML} ${LIBSBUF} 
LDADD=	-lutil -lgeom -lbsdxml -lsbuf

.include <bsd.prog.mk>
