# $FreeBSD: src/tools/regression/tls/ttls1/Makefile,v 1.4.26.1 2010/12/21 17:10:29 kensmith Exp $

PROG=	ttls1
LDADD+=	-L../libxx -lxx -Wl,--rpath=${.OBJDIR}/../libxx
LDADD+=	-L../libyy -lyy -Wl,--rpath=${.OBJDIR}/../libyy
NO_MAN=
DEBUG_FLAGS= -g

.include <bsd.prog.mk>
