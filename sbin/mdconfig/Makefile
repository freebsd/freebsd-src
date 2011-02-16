# $FreeBSD: src/sbin/mdconfig/Makefile,v 1.10.10.1.6.1 2010/12/21 17:09:25 kensmith Exp $

PROG=		mdconfig
MAN=		mdconfig.8
MLINKS=		mdconfig.8 vnconfig.8

DPADD=	${LIBUTIL} ${LIBGEOM} ${LIBBSDXML} ${LIBSBUF} 
LDADD=	-lutil -lgeom -lbsdxml -lsbuf

.include <bsd.prog.mk>
