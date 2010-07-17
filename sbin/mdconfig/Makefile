# $FreeBSD: src/sbin/mdconfig/Makefile,v 1.10.10.1.4.1 2010/06/14 02:09:06 kensmith Exp $

PROG=		mdconfig
MAN=		mdconfig.8
MLINKS=		mdconfig.8 vnconfig.8

DPADD=	${LIBUTIL} ${LIBGEOM} ${LIBBSDXML} ${LIBSBUF} 
LDADD=	-lutil -lgeom -lbsdxml -lsbuf

.include <bsd.prog.mk>
