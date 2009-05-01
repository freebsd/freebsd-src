# $FreeBSD: src/tools/regression/tls/ttls1/Makefile,v 1.4.20.1 2009/04/15 03:14:26 kensmith Exp $

PROG=	ttls1
LDADD+=	-L../libxx -lxx -Wl,--rpath=${.OBJDIR}/../libxx
LDADD+=	-L../libyy -lyy -Wl,--rpath=${.OBJDIR}/../libyy
NO_MAN=
DEBUG_FLAGS= -g

.include <bsd.prog.mk>
