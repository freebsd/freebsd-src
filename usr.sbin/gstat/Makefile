# $FreeBSD: src/usr.sbin/gstat/Makefile,v 1.6.2.1 2006/06/10 15:40:10 simon Exp $

PROG=	gstat
MAN=	gstat.8
WARNS?=	5
DPADD=	${LIBGEOM} ${LIBDEVSTAT} ${LIBBSDXML} ${LIBCURSES} ${LIBEDIT}
LDADD=	-lgeom -ldevstat -lbsdxml -lcurses -ledit

.include <bsd.prog.mk>
