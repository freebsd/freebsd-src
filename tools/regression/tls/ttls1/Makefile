# $FreeBSD: src/tools/regression/tls/ttls1/Makefile,v 1.4.18.1 2008/11/25 02:59:29 kensmith Exp $

PROG=	ttls1
LDADD+=	-L../libxx -lxx -Wl,--rpath=${.OBJDIR}/../libxx
LDADD+=	-L../libyy -lyy -Wl,--rpath=${.OBJDIR}/../libyy
NO_MAN=
DEBUG_FLAGS= -g

.include <bsd.prog.mk>
